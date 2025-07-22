#ifndef __EMSCRIPTEN__
#error "__EMSCRIPTEN__ not defined!"
#endif

#include <string>
#include <zlib.h>
#include <iostream>

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#define GLFW_INCLUDE_ES3
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>

#include "../imgui/imgui.h"
static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t), "ImDrawIdx is expected to be 16 bits!");

using int32  = int;
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = unsigned int;

#include "RemoteImGui.h"

// window
int32 g_windowWidth = 0;
int32 g_windowHeight = 0;
int32 g_mouseCursor = -1;
GLFWwindow* g_window = nullptr;

// rendering
GLuint g_indexBuffer = 0;
GLuint g_vertexBuffer = 0;
GLuint g_shaderProgram = 0;
size_t g_indexBufferSize = 0;
size_t g_vertexBufferSize = 0;
GLuint g_invalidTexture = 0;
std::vector<GLuint> g_registeredTextures;
std::vector<ImDrawCmd> g_drawCommands;

// network
bool g_isConnected = false;
float g_reconnectTimer = 0.f;
RemoteImGui::FInputPacket g_remoteInputs = {};
std::vector<uint8> g_messageBuffer;
EMSCRIPTEN_WEBSOCKET_T g_webSocket = NULL;

void reset_client_state()
{
	g_mouseCursor = -1;
	g_windowWidth = 0;
	g_windowHeight = 0;

	for (GLuint textureId : g_registeredTextures)
	{
		if (textureId != 0)
		{
			glDeleteTextures(1, &textureId);
		}
	}
	g_registeredTextures.clear();
}

EM_JS(int32, canvas_get_width, (),
{
	return Module.canvas.width;
});

EM_JS(int32, canvas_get_height, (),
{
	return Module.canvas.height;
});

EM_JS(void, resize_canvas, (),
{
	js_resizeCanvas();
});

int32 map_glfw_to_imgui_key(int32 keycode)
{
	switch (keycode)
	{
		case GLFW_KEY_SPACE: return ImGuiKey_Space; 
		case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe; 
		case GLFW_KEY_COMMA: return ImGuiKey_Comma; 
		case GLFW_KEY_MINUS: return ImGuiKey_Minus; 
		case GLFW_KEY_PERIOD: return ImGuiKey_Period; 
		case GLFW_KEY_SLASH: return ImGuiKey_Slash; 
		case GLFW_KEY_0: return ImGuiKey_0; 
		case GLFW_KEY_1: return ImGuiKey_1; 
		case GLFW_KEY_2: return ImGuiKey_2; 
		case GLFW_KEY_3: return ImGuiKey_3; 
		case GLFW_KEY_4: return ImGuiKey_4; 
		case GLFW_KEY_5: return ImGuiKey_5; 
		case GLFW_KEY_6: return ImGuiKey_6; 
		case GLFW_KEY_7: return ImGuiKey_7; 
		case GLFW_KEY_8: return ImGuiKey_8; 
		case GLFW_KEY_9: return ImGuiKey_9; 
		case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon; 
		case GLFW_KEY_EQUAL: return ImGuiKey_Equal; 
		case GLFW_KEY_A: return ImGuiKey_A; 
		case GLFW_KEY_B: return ImGuiKey_B; 
		case GLFW_KEY_C: return ImGuiKey_C; 
		case GLFW_KEY_D: return ImGuiKey_D; 
		case GLFW_KEY_E: return ImGuiKey_E; 
		case GLFW_KEY_F: return ImGuiKey_F; 
		case GLFW_KEY_G: return ImGuiKey_G; 
		case GLFW_KEY_H: return ImGuiKey_H; 
		case GLFW_KEY_I: return ImGuiKey_I; 
		case GLFW_KEY_J: return ImGuiKey_J; 
		case GLFW_KEY_K: return ImGuiKey_K; 
		case GLFW_KEY_L: return ImGuiKey_L; 
		case GLFW_KEY_M: return ImGuiKey_M; 
		case GLFW_KEY_N: return ImGuiKey_N; 
		case GLFW_KEY_O: return ImGuiKey_O; 
		case GLFW_KEY_P: return ImGuiKey_P; 
		case GLFW_KEY_Q: return ImGuiKey_Q; 
		case GLFW_KEY_R: return ImGuiKey_R; 
		case GLFW_KEY_S: return ImGuiKey_S; 
		case GLFW_KEY_T: return ImGuiKey_T; 
		case GLFW_KEY_U: return ImGuiKey_U; 
		case GLFW_KEY_V: return ImGuiKey_V; 
		case GLFW_KEY_W: return ImGuiKey_W; 
		case GLFW_KEY_X: return ImGuiKey_X; 
		case GLFW_KEY_Y: return ImGuiKey_Y; 
		case GLFW_KEY_Z: return ImGuiKey_Z; 
		case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket; 
		case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash; 
		case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket; 
		case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent; 
		case GLFW_KEY_WORLD_1: return 0xffffff; 
		case GLFW_KEY_WORLD_2: return 0xffffff; 
		case GLFW_KEY_ESCAPE: return ImGuiKey_Escape; 
		case GLFW_KEY_ENTER: return ImGuiKey_Enter; 
		case GLFW_KEY_TAB: return ImGuiKey_Tab; 
		case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace; 
		case GLFW_KEY_INSERT: return ImGuiKey_Insert; 
		case GLFW_KEY_DELETE: return ImGuiKey_Delete; 
		case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow; 
		case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow; 
		case GLFW_KEY_DOWN: return ImGuiKey_DownArrow; 
		case GLFW_KEY_UP: return ImGuiKey_UpArrow; 
		case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp; 
		case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown; 
		case GLFW_KEY_HOME: return ImGuiKey_Home; 
		case GLFW_KEY_END: return ImGuiKey_End; 
		case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock; 
		case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock; 
		case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock; 
		case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen; 
		case GLFW_KEY_PAUSE: return ImGuiKey_Pause; 
		case GLFW_KEY_F1: return ImGuiKey_F1; 
		case GLFW_KEY_F2: return ImGuiKey_F2; 
		case GLFW_KEY_F3: return ImGuiKey_F3; 
		case GLFW_KEY_F4: return ImGuiKey_F4; 
		case GLFW_KEY_F5: return ImGuiKey_F5; 
		case GLFW_KEY_F6: return ImGuiKey_F6; 
		case GLFW_KEY_F7: return ImGuiKey_F7; 
		case GLFW_KEY_F8: return ImGuiKey_F8; 
		case GLFW_KEY_F9: return ImGuiKey_F9; 
		case GLFW_KEY_F10: return ImGuiKey_F10; 
		case GLFW_KEY_F11: return ImGuiKey_F11; 
		case GLFW_KEY_F12: return ImGuiKey_F12; 
		case GLFW_KEY_F13: return ImGuiKey_F13; 
		case GLFW_KEY_F14: return ImGuiKey_F14; 
		case GLFW_KEY_F15: return ImGuiKey_F15; 
		case GLFW_KEY_F16: return ImGuiKey_F16; 
		case GLFW_KEY_F17: return ImGuiKey_F17; 
		case GLFW_KEY_F18: return ImGuiKey_F18; 
		case GLFW_KEY_F19: return ImGuiKey_F19; 
		case GLFW_KEY_F20: return ImGuiKey_F20; 
		case GLFW_KEY_F21: return ImGuiKey_F21; 
		case GLFW_KEY_F22: return ImGuiKey_F22; 
		case GLFW_KEY_F23: return ImGuiKey_F23; 
		case GLFW_KEY_F24: return ImGuiKey_F24; 
		case GLFW_KEY_F25: return 0xffffff; 
		case GLFW_KEY_KP_0: return ImGuiKey_Keypad0; 
		case GLFW_KEY_KP_1: return ImGuiKey_Keypad1; 
		case GLFW_KEY_KP_2: return ImGuiKey_Keypad2; 
		case GLFW_KEY_KP_3: return ImGuiKey_Keypad3; 
		case GLFW_KEY_KP_4: return ImGuiKey_Keypad4; 
		case GLFW_KEY_KP_5: return ImGuiKey_Keypad5; 
		case GLFW_KEY_KP_6: return ImGuiKey_Keypad6; 
		case GLFW_KEY_KP_7: return ImGuiKey_Keypad7; 
		case GLFW_KEY_KP_8: return ImGuiKey_Keypad8; 
		case GLFW_KEY_KP_9: return ImGuiKey_Keypad9; 
		case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal; 
		case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide; 
		case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply; 
		case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract; 
		case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd; 
		case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter; 
		case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual; 
		case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift; 
		case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl; 
		case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt; 
		case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper; 
		case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift; 
		case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl; 
		case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt; 
		case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper; 
		case GLFW_KEY_MENU: return ImGuiKey_Menu;
	}
	return 0xffffff;
}

void update_mouse_cursor(int32 mouseCursor)
{
	if (g_mouseCursor != mouseCursor)
	{
		g_mouseCursor = mouseCursor;
		switch(mouseCursor)
		{
			case ImGuiMouseCursor_Arrow:
				EM_ASM(document.body.style.cursor = 'default';);
			break;
			case ImGuiMouseCursor_TextInput:
				EM_ASM(document.body.style.cursor = 'text';);
			break;
			case ImGuiMouseCursor_ResizeAll:
				EM_ASM(document.body.style.cursor = 'move';);
			break;
			case ImGuiMouseCursor_ResizeNS:
				EM_ASM(document.body.style.cursor = 'ns-resize';);
			break;
			case ImGuiMouseCursor_ResizeEW:
				EM_ASM(document.body.style.cursor = 'ew-resize';);
			break;
			case ImGuiMouseCursor_ResizeNESW:
				EM_ASM(document.body.style.cursor = 'nesw-resize';);                        
			break;
			case ImGuiMouseCursor_ResizeNWSE:
				EM_ASM(document.body.style.cursor = 'nwse-resize';);                        
			break;
			case ImGuiMouseCursor_Hand:
				EM_ASM(document.body.style.cursor = 'pointer';);                        
			break;
			case ImGuiMouseCursor_NotAllowed:
				EM_ASM(document.body.style.cursor = 'not-allowed';);                        
				break;
			default:
				EM_ASM(document.body.style.cursor = 'default';);                        
				break;
		}
	}
}

EM_BOOL onopen(int32 eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData)
{
	std::cout << "websocket::onopen\n";

	g_isConnected = true;
	reset_client_state();

	return EM_TRUE;
}
EM_BOOL onerror(int32 eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData)
{
	std::cout << "websocket::onerror\n";

	g_isConnected = false;
	g_reconnectTimer = 5.f;

	return EM_TRUE;
}
EM_BOOL onclose(int32 eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData)
{
	std::cout << "websocket::onclose\n";

	g_isConnected = false;
	g_reconnectTimer = 5.f;

	return EM_TRUE;
}
EM_BOOL onmessage(int32 eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	if (websocketEvent->isText)
	{
		// text message not handled!
		std::cout << "Text messages are not currently handled, dropping packet.\n";
	}
	else
	{
		if (websocketEvent->numBytes > 0 && websocketEvent->data)
		{
			const uint32 packetSize = *(uint32*)&websocketEvent->data[0];
			if (packetSize != (websocketEvent->numBytes - sizeof(uint32)))
			{
				std::cout << "Packet size error!\n";
				return EM_FALSE;
			}

			const uint8* packetBuffer = (uint8_t*)websocketEvent->data + sizeof(uint32);
			if (/*bUseCompression=*/false)
			{
				const uint32 expectedUncompressedDataSize = *(uint32*)&websocketEvent->data[4];
				if (g_messageBuffer.size() < expectedUncompressedDataSize)
				{
					g_messageBuffer.resize(expectedUncompressedDataSize);
				}

				// decompress
				size_t uncompressedBufferSize = 0;
				{
					z_stream stream;
					stream.zalloc = Z_NULL;
					stream.zfree = Z_NULL;
					stream.opaque = Z_NULL;
					stream.avail_in = packetSize;
					stream.next_in = (uint8*)&websocketEvent->data[8];
					stream.avail_out = g_messageBuffer.size();
					stream.next_out = &g_messageBuffer[0];

					int32 result = inflateInit(&stream);
					if (result != Z_OK)
					{
						std::cout << "Failed to uncompress packet. inflateEnd() returned error!\n";
						return EM_FALSE;
					}

					result = inflate(&stream, Z_NO_FLUSH);              
					if (result == Z_STREAM_END)
					{
						uncompressedBufferSize = stream.total_out;
					}
					
					result = inflateEnd(&stream);
					if (result < Z_OK)
					{
						std::cout << "Failed to uncompress packet. inflateEnd() returned error!\n";
						return EM_FALSE;
					}
				}
				
				if (expectedUncompressedDataSize != uncompressedBufferSize)
				{
					std::cout << "Failed to uncompress packet. Size mismatch!\n";
					return EM_FALSE;
				}

				packetBuffer = &g_messageBuffer[0];
			}
			uint32 packetReadOffset = 0;
			const uint32 packetType = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
			
			if (packetType == RemoteImGui::PacketId_Texture)
			{
				const uint32 textureWidth  = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				const uint32 textureHeight = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				const uint32 bytesPerPixel = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				
				GLuint textureId;
				glGenTextures(1, &textureId);
				glBindTexture(GL_TEXTURE_2D, textureId); 
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &packetBuffer[packetReadOffset]);
				g_registeredTextures.push_back(textureId);
			}
			else if (packetType == RemoteImGui::PacketId_DrawData)
			{
				const int32 mouseCursor      = *(int32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				const int32 totalIndexCount  = *(int32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				const int32 totalVertexCount = *(int32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
				const int32 drawCommandCount = *(int32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_indexBuffer);
				if (g_indexBufferSize < totalIndexCount * sizeof(ImDrawIdx))
				{
					g_indexBufferSize = totalIndexCount * sizeof(ImDrawIdx);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalIndexCount * sizeof(ImDrawIdx), nullptr, GL_STREAM_DRAW);
				}
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalIndexCount * sizeof(ImDrawIdx), (void*)&packetBuffer[packetReadOffset]);
				packetReadOffset += totalIndexCount * sizeof(ImDrawIdx);

				glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
				if (g_vertexBufferSize < totalVertexCount * sizeof(ImDrawVert))
				{
					g_vertexBufferSize = totalVertexCount * sizeof(ImDrawVert);
					glBufferData(GL_ARRAY_BUFFER, totalVertexCount * sizeof(ImDrawVert), nullptr, GL_STREAM_DRAW);
				}
				glBufferSubData(GL_ARRAY_BUFFER, 0, totalVertexCount * sizeof(ImDrawVert), (void*)&packetBuffer[packetReadOffset]);
				packetReadOffset += totalVertexCount * sizeof(ImDrawVert);

				g_drawCommands.clear();
				g_drawCommands.reserve(drawCommandCount);
				for (int32 Index = 0; Index < drawCommandCount; ++Index)
				{
					ImDrawCmd drawCommand = {};
					
					drawCommand.ClipRect.x = *(float*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.ClipRect.y = *(float*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.ClipRect.z = *(float*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.ClipRect.w = *(float*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;

					drawCommand.TextureId = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.VtxOffset = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.IdxOffset = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;
					drawCommand.ElemCount = *(uint32*)&packetBuffer[packetReadOffset]; packetReadOffset += 4;

					g_drawCommands.emplace_back(std::move(drawCommand));    
				}

				update_mouse_cursor(mouseCursor);               
			}
		}
	}

	return EM_TRUE;
}

void on_size_changed()
{
	std::cout << "Window Width: " << g_windowWidth << " Height: " << g_windowHeight << "\n";
	glfwSetWindowSize(g_window, g_windowWidth, g_windowHeight);

	g_remoteInputs.DisplayWidth = g_windowWidth;
	g_remoteInputs.DisplayHeight = g_windowHeight;
	g_remoteInputs.bHasDisplaySizeChangedEvent = true;
}

bool init_shaders()
{
	static const char *vertexShaderSource =
	"uniform mat4 projectionMatrix;\n"
	"attribute vec2 position;\n"
	"attribute vec2 uv;\n"
	"attribute vec4 color;\n"
	"varying vec2 Frag_UV;\n"
	"varying vec4 Frag_Color;\n"
	"void main()\n"
	"{\n"
	"   Frag_UV = uv;\n"
	"   Frag_Color = color;\n"
	"   gl_Position = projectionMatrix * vec4(position.x, position.y, 0, 1.0);\n"
	"}\n\0";

	static const char *fragmentShaderSource =
	"precision mediump float;\n"
	"varying vec2 Frag_UV;\n"
	"varying vec4 Frag_Color;\n"
	"uniform sampler2D texture;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor  = Frag_Color * texture2D(texture, Frag_UV);\n"
	"}\n\0";

	// build and compile our shader program
	// ------------------------------------
	// vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// check for shader compile errors
	int32 success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		return false;
	}

	// fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// check for shader compile errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
		return false;
	}

	// link shaders
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// check for linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
		return false;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	g_shaderProgram = shaderProgram;
	return true;
}

bool init_gl()
{
	if (!glfwInit())
	{
		std::cout << "Failed to initialize GLFW\n";
		return true;
	}

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL

	// Open a window and create its OpenGL context
	g_window = glfwCreateWindow(canvas_get_width(), canvas_get_height(), "WebGui Demo", NULL, NULL);
	if (g_window == NULL)
	{
		std::cout << "Failed to open GLFW window.\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(g_window); // Initialize GLEW

	if (!init_shaders())
	{
		return false;
	}

	{
		const uint32 textureWidth  = 1;
		const uint32 textureHeight = 1;
		const uint32 bytesPerPixel = 4;
		const uint32 textureData = 0xFFFF00FF;

		glGenTextures(1, &g_invalidTexture);
		glBindTexture(GL_TEXTURE_2D, g_invalidTexture); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &textureData);
	}

	return true;
}

void character_callback(GLFWwindow* window, uint32 codepoint)
{
	//std::cout << "CharInput: " << codepoint << '\n';

	g_remoteInputs.bHasInputCharacterCodeEvent = true;
	g_remoteInputs.CharacterCode = codepoint;
}

void handle_mod_keys(int32 mods)
{
	g_remoteInputs.bHasModShiftEvent = true;
	g_remoteInputs.ModShiftState = (mods & GLFW_MOD_SHIFT) > 0;

	g_remoteInputs.bHasModCtrlEvent = true;
	g_remoteInputs.ModCtrlState = (mods & GLFW_MOD_CONTROL) > 0;
	
	g_remoteInputs.bHasModAltEvent = true;
	g_remoteInputs.ModAltState = (mods & GLFW_MOD_ALT) > 0;
}

void key_callback(GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods)
{
	//std::cout << "KeyInput: " << key << (action == GLFW_PRESS ? " Pressed" : " Released") << '\n';

	if (action != GLFW_REPEAT)
	{
		key = map_glfw_to_imgui_key(key);

		if (key == ImGuiKey_V && ((mods & GLFW_MOD_CONTROL) > 0))
		{
			const char* clipboardText = "Yolo 123!";//glfwGetClipboardString(NULL);
			if (clipboardText && strlen(clipboardText) > 0)
			{
				g_remoteInputs.bHasClipboardTextEvent = true;
				std::memcpy(g_remoteInputs.ClipboardText, clipboardText, strlen(clipboardText));
			}
		}
		
		if (action == GLFW_PRESS)
		{
			g_remoteInputs.KeysDown[g_remoteInputs.NumKeyDownEvents++] = key;
		}
		else
		{
			g_remoteInputs.KeysUp[g_remoteInputs.NumKeyUpEvents++] = key;       
		}

		handle_mod_keys(mods);
	}
}

void mouse_button_callback(GLFWwindow* window, int32 button, int32 action, int32 mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		//std::cout << "LeftMouseButton: " << ((action == GLFW_PRESS) ? "Pressed\n" : "Released\n");

		g_remoteInputs.bHasLeftMouseButtonEvent = true;
		g_remoteInputs.LeftMouseButtonState = (action == GLFW_PRESS);

		handle_mod_keys(mods);
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		//std::cout << "RightMouseButton: " << ((action == GLFW_PRESS) ? "Pressed\n" : "Released\n");

		g_remoteInputs.bHasRightMouseButtonEvent = true;
		g_remoteInputs.RightMouseButtonState = (action == GLFW_PRESS);

		handle_mod_keys(mods);
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		//std::cout << "MiddleMouseButton: " << ((action == GLFW_PRESS) ? "Pressed\n" : "Released\n");

		g_remoteInputs.bHasMiddleMouseButtonEvent = true;
		g_remoteInputs.MiddleMouseButtonState = (action == GLFW_PRESS);

		handle_mod_keys(mods);
	}
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	//std::cout << "MousePosition: " << xpos << " " << ypos << '\n';

	g_remoteInputs.bHasMousePositionEvent = true;
	g_remoteInputs.MousePositionX = xpos;
	g_remoteInputs.MousePositionY = ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	//std::cout << "MouseScroll: " << xoffset << " " << yoffset << '\n';

	g_remoteInputs.bHasMouseWheelEvent = true;
	g_remoteInputs.MouseWheelDelta = yoffset;
}

bool init()
{
	if (!init_gl())
	{
		return false;
	}

	resize_canvas();
	glGenBuffers(1, &g_indexBuffer);
	glGenBuffers(1, &g_vertexBuffer);

	glfwSetKeyCallback(g_window, key_callback);
	glfwSetCharCallback(g_window, character_callback);
	glfwSetMouseButtonCallback(g_window, mouse_button_callback);
	glfwSetCursorPosCallback(g_window, cursor_position_callback);
	glfwSetScrollCallback(g_window, scroll_callback);

	return true;
}

void quit()
{
	if (g_webSocket != NULL)
	{
		emscripten_websocket_delete(g_webSocket);
		g_webSocket = NULL;
	}
	emscripten_websocket_deinitialize();
	
	if (g_indexBuffer != 0)
	{
		glDeleteBuffers(1, &g_indexBuffer);
		g_indexBuffer = 0;
	}

	if (g_vertexBuffer != 0)
	{
		glDeleteBuffers(1, &g_vertexBuffer);
		g_vertexBuffer = 0;
	}

	if (g_invalidTexture != 0)
	{
		glDeleteTextures(1, &g_invalidTexture);
		g_invalidTexture = 0;
	}

	if (g_shaderProgram != 0)
	{
		glDeleteProgram(g_shaderProgram);
		g_shaderProgram = 0;
	}

	reset_client_state();

	glfwTerminate();
}

void loop()
{
	const int32 width = canvas_get_width();
	const int32 height = canvas_get_height();

	if (width != g_windowWidth || height != g_windowHeight)
	{
		g_windowWidth = width;
		g_windowHeight = height;
		on_size_changed();
	}

	glfwPollEvents();

	// render
	glfwMakeContextCurrent(g_window);
	glViewport(0, 0, g_windowWidth, g_windowHeight);
	glScissor(0, 0, g_windowWidth, g_windowHeight);
	glClearColor(0.266666667, 0.266666667, 0.266666667, 0.266666667);   
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_isConnected)
	{
		glUseProgram(g_shaderProgram);

		glActiveTexture(GL_TEXTURE0);

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		const float L = 0;
		const float R = g_windowWidth;
		const float B = 0;
		const float T = g_windowHeight;
		const float ortho_projection[4][4] =
		{
			{ 2.0f/(R-L),   0.0f,         0.0f,  0.0f },
			{ 0.0f,         2.0f/(T-B),   0.0f,  0.0f },
			{ 0.0f,         0.0f,        -1.0f,  0.0f },
			{ (R+L)/(L-R),  (T+B)/(B-T),  0.0f,  1.0f },
		};
		glUniform1i(glGetUniformLocation(g_shaderProgram, "texture"), 0);
		glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "projectionMatrix"), 1, GL_FALSE, &ortho_projection[0][0]);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_indexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		for (const auto& drawCmd : g_drawCommands)
		{
			const int32 ScissorRect[4] =
			{
				std::max(0, (int32)drawCmd.ClipRect.x),
				std::max(0, (int32)drawCmd.ClipRect.y),
				std::min(g_windowWidth, (int32)drawCmd.ClipRect.z),
				std::min(g_windowHeight, (int32)drawCmd.ClipRect.w)
			};
			glScissor(ScissorRect[0], ScissorRect[1], ScissorRect[2] - ScissorRect[0], ScissorRect[3] - ScissorRect[1]);

			const size_t vertexBufferOffset = drawCmd.VtxOffset * sizeof(ImDrawVert);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void*)(offsetof(ImDrawVert, pos) + vertexBufferOffset));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void*)(offsetof(ImDrawVert, uv) + vertexBufferOffset));
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (void*)(offsetof(ImDrawVert, col) + vertexBufferOffset));

			GLuint textureId = g_invalidTexture;
			if (drawCmd.TextureId < g_registeredTextures.size())
			{
				textureId = g_registeredTextures[drawCmd.TextureId];
			}
			glBindTexture(GL_TEXTURE_2D, textureId);

			glDrawElements(GL_TRIANGLES, drawCmd.ElemCount, GL_UNSIGNED_SHORT, (void*)(intptr_t)(drawCmd.IdxOffset * sizeof(unsigned short)));
		}
	}

	// network
	g_reconnectTimer -= 1.f / 60.f;
	if (!g_isConnected && g_reconnectTimer <= 0.f)
	{
		if (g_webSocket != NULL)
		{
			emscripten_websocket_delete(g_webSocket);
			g_webSocket = NULL;
		}

		g_reconnectTimer = 5.f;

		EmscriptenWebSocketCreateAttributes ws_attrs =
		{
			"ws://127.0.0.1:7002",
			NULL,
			EM_TRUE
		};
		g_webSocket = emscripten_websocket_new(&ws_attrs);
		emscripten_websocket_set_onopen_callback(g_webSocket, NULL, onopen);
		emscripten_websocket_set_onerror_callback(g_webSocket, NULL, onerror);
		emscripten_websocket_set_onclose_callback(g_webSocket, NULL, onclose);
		emscripten_websocket_set_onmessage_callback(g_webSocket, NULL, onmessage);
	}

	if (g_isConnected && g_remoteInputs.RawState != 0)
	{
		uint32 sendBuffer[512];
		size_t packetSize = 0;

		sendBuffer[packetSize++] = 0;
		sendBuffer[packetSize++] = RemoteImGui::PacketId_InputPacket;
		sendBuffer[packetSize++] = g_remoteInputs.RawState;
		if (g_remoteInputs.bHasDisplaySizeChangedEvent)
		{
			sendBuffer[packetSize++] = uint32(g_windowWidth) << 16 | g_windowHeight;
		}
		for (int32 i = 0; i < g_remoteInputs.NumKeyDownEvents; ++i)
		{
			sendBuffer[packetSize++] = g_remoteInputs.KeysDown[i];
		}
		for (int32 i = 0; i < g_remoteInputs.NumKeyUpEvents; ++i)
		{
			sendBuffer[packetSize++] = g_remoteInputs.KeysUp[i];
		}
		if (g_remoteInputs.bHasMousePositionEvent)
		{
			sendBuffer[packetSize++] = *(uint32*)&g_remoteInputs.MousePositionX;
			sendBuffer[packetSize++] = *(uint32*)&g_remoteInputs.MousePositionY;
		}
		if (g_remoteInputs.bHasMouseWheelEvent)
		{
			sendBuffer[packetSize++] = *(uint32*)&g_remoteInputs.MouseWheelDelta;
		}
		if (g_remoteInputs.bHasInputCharacterCodeEvent)
		{
			sendBuffer[packetSize++] = g_remoteInputs.CharacterCode;
		}
		if (g_remoteInputs.bHasClipboardTextEvent)
		{
			std::memcpy((uint8*)&sendBuffer[packetSize], g_remoteInputs.ClipboardText, sizeof(g_remoteInputs.ClipboardText));
			packetSize += sizeof(g_remoteInputs.ClipboardText) / sizeof(uint32);
		}
		sendBuffer[0] = (packetSize - 1) * sizeof(uint32);

		//std::cout << "sending: " << packetSize * sizeof(uint32) << " bytes\n";
		EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(g_webSocket, sendBuffer, packetSize * sizeof(uint32));
		if (result)
		{
			std::cout << "socket send error: " << result << "\n";
		}

		std::memset(&g_remoteInputs.RawState, 0, sizeof(uint32));
	}
}

int32 main(int32 argc, char** argv)
{
	if (!init())
	{
		return 1;
	}

	emscripten_set_main_loop(loop, 0, 1);

	quit();

	return 0;
}

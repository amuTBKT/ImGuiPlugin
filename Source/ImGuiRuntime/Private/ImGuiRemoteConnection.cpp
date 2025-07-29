#include "ImGuiRemoteConnection.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiSubsystem.h"
#include "INetworkingWebSocket.h"
#include "Modules/ModuleManager.h"
#include "IWebSocketNetworkingModule.h"
#include "WebSocketNetworkingDelegates.h"

FImGuiRemoteConnection::~FImGuiRemoteConnection()
{
	Shutdown();
}

bool FImGuiRemoteConnection::Connect(const FString& Addr, int32 Port)
{
	if (IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("ImGuiRemoteConnection already connected to '%s'"), *WebServer->Info());
		return false;
	}

	FWebSocketClientConnectedCallBack Callback;
	Callback.BindLambda(
		[this](INetworkingWebSocket* Socket)
		{
			const FString RemoteEndPoint = Socket->RemoteEndPoint(/*bAppendPort=*/false);
			const FString LocalEndPoint = Socket->LocalEndPoint(/*bAppendPort=*/false);

			// TODO: validate endpoints before accepting connection?

			FWebSocketPacketReceivedCallBack ReceiveCallBack;
			ReceiveCallBack.BindLambda(
				[this](void* Data, int32 Size)
				{
					const uint32* Buffer = (uint32*)Data;
					if (!Buffer || Size <= 0)
					{
						return;
					}

					int32 ReadOffset = 0;
					const uint32_t PacketType = Buffer[ReadOffset++];
					if (PacketType == RemoteImGui::PacketId_InputPacket)
					{
						const uint32 NewState = Buffer[ReadOffset++];

						// TODO: hack to avoid queuing packets containing only mouse movement info (should ideally queue entire state)
						if ((NewState & ~(1u << 14)) == 0)
						{
							RemoteInputs.bHasMousePositionEvent = true;
							RemoteInputs.MousePositionX = BitCast<float>(Buffer[ReadOffset++]);
							RemoteInputs.MousePositionY = BitCast<float>(Buffer[ReadOffset++]);
							return;
						}

						RemoteInputs.RawState = NewState;
						if (RemoteInputs.bHasDisplaySizeChangedEvent)
						{
							RemoteInputs.DisplayWidth = Buffer[ReadOffset] >> 16;
							RemoteInputs.DisplayHeight = Buffer[ReadOffset] & 0xFFFF;
							ReadOffset += 1;
						}
						for (uint32 KeyIndex = 0; KeyIndex < RemoteInputs.NumKeyDownEvents; ++KeyIndex)
						{
							RemoteInputs.KeysDown[KeyIndex] = Buffer[ReadOffset++];
						}
						for (uint32 KeyIndex = 0; KeyIndex < RemoteInputs.NumKeyUpEvents; ++KeyIndex)
						{
							RemoteInputs.KeysUp[KeyIndex] = Buffer[ReadOffset++];
						}
						if (RemoteInputs.bHasMousePositionEvent)
						{
							RemoteInputs.MousePositionX = BitCast<float>(Buffer[ReadOffset++]);
							RemoteInputs.MousePositionY = BitCast<float>(Buffer[ReadOffset++]);
						}
						if (RemoteInputs.bHasMouseWheelEvent)
						{
							RemoteInputs.MouseWheelDelta = BitCast<float>(Buffer[ReadOffset++]);
						}
						if (RemoteInputs.bHasInputCharacterCodeEvent)
						{
							RemoteInputs.CharacterCode = Buffer[ReadOffset++];
						}
						if (RemoteInputs.bHasClipboardTextEvent)
						{
							FMemory::Memcpy(&RemoteInputs.ClipboardText[0], (uint8_t*)&Buffer[ReadOffset], sizeof(RemoteInputs.ClipboardText));
						}
					}
				});
			Socket->SetReceiveCallBack(ReceiveCallBack);

			FWebSocketInfoCallBack CloseCallback;
			CloseCallback.BindLambda( 
				[this]()
				{
					WebClient = nullptr;

					OnDisconnected.ExecuteIfBound();
				});
			Socket->SetSocketClosedCallBack(CloseCallback);

			WebClient = Socket;
			FMemory::Memzero(&RemoteInputs, sizeof(RemoteInputs));
			OnConnected.ExecuteIfBound();
		});

	WebServer = FModuleManager::Get().LoadModuleChecked<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking")).CreateServer();
	if (!WebServer || !WebServer->Init(Port, Callback, Addr))
	{
		WebServer.Reset();
	}

	return WebServer.IsValid();
}

void FImGuiRemoteConnection::NewFrame()
{
	FMemory::Memzero(&RemoteInputs.RawState, sizeof(uint32_t));

	if (WebServer.IsValid())
	{
		WebServer->Tick();

		if (WebClient)
		{
			WebClient->Tick();
		}
	}
}

void FImGuiRemoteConnection::Shutdown()
{
	WebClient = nullptr;
	WebServer.Reset();

	OnConnected = {};
	OnDisconnected = {};
}

bool FImGuiRemoteConnection::IsConnected() const
{
	return WebServer.IsValid() && (WebClient != nullptr);
}

bool FImGuiRemoteConnection::CanDrawWidgets() const
{
	return IsConnected() && (FMath::Min(RemoteInputs.DisplayWidth, RemoteInputs.DisplayHeight) > 0);
}

void FImGuiRemoteConnection::CopyClientState(ImGuiIO& IO) const
{
	IO.DisplaySize = ImVec2(RemoteInputs.DisplayWidth, RemoteInputs.DisplayHeight);

	// update clipboard text ahead of adding Ctrl+V key event.
	if (RemoteInputs.bHasClipboardTextEvent && strlen(RemoteInputs.ClipboardText) > 0)
	{
		ImGui::GetPlatformIO().Platform_SetClipboardTextFn(ImGui::GetCurrentContext(), RemoteInputs.ClipboardText);
	}

	// keyboard events
	{
		for (uint32 KeyIndex = 0; KeyIndex < RemoteInputs.NumKeyDownEvents; KeyIndex++)
		{
			const ImGuiKey Key = (ImGuiKey)RemoteInputs.KeysDown[KeyIndex];
			if (ImGui::IsNamedKey(Key))
			{
				IO.AddKeyEvent(Key, true);
			}
		}
		for (uint32 KeyIndex = 0; KeyIndex < RemoteInputs.NumKeyUpEvents; KeyIndex++)
		{
			const ImGuiKey Key = (ImGuiKey)RemoteInputs.KeysUp[KeyIndex];
			if (ImGui::IsNamedKey(Key))
			{
				IO.AddKeyEvent(Key, false);
			}
		}
		if (RemoteInputs.bHasInputCharacterCodeEvent)
		{
			IO.AddInputCharacter(RemoteInputs.CharacterCode);
		}

		if (RemoteInputs.bHasModShiftEvent)
		{
			IO.AddKeyEvent(ImGuiMod_Shift, RemoteInputs.ModShiftState > 0);
		}
		if (RemoteInputs.bHasModCtrlEvent)
		{
			IO.AddKeyEvent(ImGuiMod_Ctrl, RemoteInputs.ModCtrlState > 0);
		}
		if (RemoteInputs.bHasModAltEvent)
		{
			IO.AddKeyEvent(ImGuiMod_Alt, RemoteInputs.ModAltState > 0);
		}
	}

	// mouse events
	{
		if (RemoteInputs.bHasMousePositionEvent)
		{
			IO.AddMousePosEvent(RemoteInputs.MousePositionX, RemoteInputs.MousePositionY);
		}

		if (RemoteInputs.bHasLeftMouseButtonEvent)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Left, RemoteInputs.LeftMouseButtonState > 0);
		}
		if (RemoteInputs.bHasRightMouseButtonEvent)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Right, RemoteInputs.RightMouseButtonState > 0);
		}
		if (RemoteInputs.bHasMiddleMouseButtonEvent)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, RemoteInputs.MiddleMouseButtonState > 0);
		}

		if (RemoteInputs.bHasMouseWheelEvent)
		{
			IO.AddMouseWheelEvent(0.f, RemoteInputs.MouseWheelDelta);
		}
	}
}

void FImGuiRemoteConnection::SendRawPacket(const TArray<uint8>& Packet)
{
	if (!ensure(IsConnected()))
	{
		return;
	}

	// TODO: webclient expects uncompressed data
	if (/*bUseCompression=*/false)
	{
		const int32 UncompressedSize = Packet.NumBytes();
		const int32 MaxCompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, UncompressedSize, COMPRESS_BiasSize);

		TArray<uint8> CompressedArray;
		CompressedArray.SetNumUninitialized(MaxCompressedSize + sizeof(uint32_t));
		*(uint32*)&CompressedArray[0] = UncompressedSize;
		int32 CompressedSize = MaxCompressedSize;
		const bool bSuccess = FCompression::CompressMemory(
			NAME_Zlib,
			/*out*/CompressedArray.GetData() + sizeof(uint32), /*out*/CompressedSize,
			Packet.GetData(), UncompressedSize,
			COMPRESS_BiasSize);

		if (bSuccess)
		{
			WebClient->Send(CompressedArray.GetData(), CompressedSize + sizeof(uint32), true);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[ImGuiRemoteConnection] Failed to compress packet."));
		}
	}
	else
	{
		WebClient->Send(Packet.GetData(), Packet.NumBytes(), true);
	}
}

void FImGuiRemoteConnection::SendDrawData(const ImDrawData* DrawData, int32 MouseCursor)
{
	uint32 DrawCommandCount = 0;
	for (const ImDrawList* CmdList : DrawData->CmdLists)
	{
		DrawCommandCount += CmdList->CmdBuffer.Size;
	}

	TArray<uint8> Buffer;   
	Buffer.Reserve(
		sizeof(uint32) * 8 +
		DrawData->TotalIdxCount * sizeof(ImDrawIdx) +
		DrawData->TotalVtxCount * sizeof(ImDrawVert) +
		DrawCommandCount * sizeof(ImDrawCmd));

	Buffer.Append((uint8*)&RemoteImGui::PacketId_DrawData, sizeof(uint32));
	Buffer.Append((uint8*)&MouseCursor, sizeof(MouseCursor));
	Buffer.Append((uint8*)&DrawData->TotalIdxCount, sizeof(DrawData->TotalIdxCount));
	Buffer.Append((uint8*)&DrawData->TotalVtxCount, sizeof(DrawData->TotalVtxCount));
	Buffer.Append((uint8*)&DrawCommandCount, sizeof(DrawCommandCount));

	for (const ImDrawList* CmdList : DrawData->CmdLists)
	{
		Buffer.Append((uint8*)CmdList->IdxBuffer.Data, CmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
	}
	for (const ImDrawList* CmdList : DrawData->CmdLists)
	{
		Buffer.Append((uint8*)CmdList->VtxBuffer.Data, CmdList->VtxBuffer.Size * sizeof(ImDrawVert));
	}

	uint32 GlobalVertexOffset = 0;
	uint32 GlobalIndexOffset = 0;
	for (const ImDrawList* CmdList : DrawData->CmdLists)
	{
		for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
		{
			Buffer.Append((uint8*)&DrawCmd.ClipRect, sizeof(DrawCmd.ClipRect));

			const uint32 TextureIndex = UImGuiSubsystem::ImGuiIDToIndex(DrawCmd.TextureId);
			Buffer.Append((uint8*)&TextureIndex, sizeof(TextureIndex));

			const uint32 VertexOffset = DrawCmd.VtxOffset + GlobalVertexOffset;
			const uint32 IndexOffset  = DrawCmd.IdxOffset + GlobalIndexOffset;
			const uint32 ElementCount = DrawCmd.ElemCount;

			Buffer.Append((uint8*)&VertexOffset, sizeof(VertexOffset));
			Buffer.Append((uint8*)&IndexOffset,  sizeof(IndexOffset));
			Buffer.Append((uint8*)&ElementCount, sizeof(ElementCount));
		}
		GlobalIndexOffset  += CmdList->IdxBuffer.Size;
		GlobalVertexOffset += CmdList->VtxBuffer.Size;
	}

	SendRawPacket(Buffer);
}

void FImGuiRemoteConnection::SendTextureData(int32 TextureWidth, int32 TextureHeight, int32 BytesPerPixel, uint8* TextureData)
{
	TArray<uint8> Buffer;
	Buffer.Append((uint8*)&RemoteImGui::PacketId_Texture, sizeof(uint32));
	Buffer.Append((uint8*)&TextureWidth, sizeof(uint32));
	Buffer.Append((uint8*)&TextureHeight, sizeof(uint32));
	Buffer.Append((uint8*)&BytesPerPixel, sizeof(uint32));
	Buffer.Append(TextureData, TextureWidth * TextureHeight * BytesPerPixel);

	SendRawPacket(Buffer);
}

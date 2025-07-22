#pragma once

namespace RemoteImGui
{
	static constexpr uint32 MakeRemoteImGuiPacketId(unsigned char ch0, unsigned char ch1, unsigned char ch2, unsigned char ch3)
	{
		return uint32(ch0) | (uint32(ch1) << 8) | (uint32(ch2) << 16) | (uint32(ch3) << 24);
	}
	static inline uint32 PacketId_Texture		= MakeRemoteImGuiPacketId('_', 'T', 'E', 'X');
	static inline uint32 PacketId_DrawData		= MakeRemoteImGuiPacketId('D', 'R', 'A', 'W');
	static inline uint32 PacketId_InputPacket	= MakeRemoteImGuiPacketId('I', 'P', 'K', 'T');

	struct FInputPacket
	{
		// replicated state
		union
		{
			struct
			{
				uint32 bHasDisplaySizeChangedEvent : 1;
				uint32 NumKeyDownEvents : 3;
				uint32 NumKeyUpEvents : 3;
				uint32 bHasLeftMouseButtonEvent : 1;
				uint32 LeftMouseButtonState : 1;
				uint32 bHasRightMouseButtonEvent : 1;
				uint32 RightMouseButtonState : 1;
				uint32 bHasMiddleMouseButtonEvent : 1;
				uint32 MiddleMouseButtonState : 1;
				uint32 bHasMouseWheelEvent : 1;
				uint32 bHasMousePositionEvent : 1;
				uint32 bHasInputCharacterCodeEvent : 1;
				uint32 bHasClipboardTextEvent : 1;
				uint32 bHasModShiftEvent : 1;
				uint32 ModShiftState : 1;
				uint32 bHasModCtrlEvent : 1;
				uint32 ModCtrlState : 1;
				uint32 bHasModAltEvent : 1;
				uint32 ModAltState : 1;
			};
			uint32 RawState = 0;
		};

		// optional data
		uint16 DisplayWidth = 0;
		uint16 DisplayHeight = 0;
		float MousePositionX = 0.f;
		float MousePositionY = 0.f;
		float MouseWheelDelta = 0.f;
		uint16 KeysDown[8];
		uint16 KeysUp[8];
		uint32 CharacterCode = 0xFFFFFFFF;
		char ClipboardText[256] = { 0 };
	};
}

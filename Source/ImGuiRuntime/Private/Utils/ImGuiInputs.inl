// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "Input/Events.h"
#include "InputCoreTypes.h"

namespace ImGuiUtils
{
	FORCEINLINE static ImGuiKey UnrealToImGuiKey(FName Keyname)
	{
		static const TMap<FName, ImGuiKey> KeyMap =
		{
			{ EKeys::BackSpace.GetFName(), ImGuiKey_Backspace },
			{ EKeys::Tab.GetFName(), ImGuiKey_Tab },
			{ EKeys::Enter.GetFName(), ImGuiKey_Enter },
			{ EKeys::Pause.GetFName(), ImGuiKey_Pause },

			{ EKeys::CapsLock.GetFName(), ImGuiKey_CapsLock },
			{ EKeys::Escape.GetFName(), ImGuiKey_Escape },
			{ EKeys::SpaceBar.GetFName(), ImGuiKey_Space },
			{ EKeys::PageUp.GetFName(), ImGuiKey_PageUp },
			{ EKeys::PageDown.GetFName(), ImGuiKey_PageDown },
			{ EKeys::End.GetFName(), ImGuiKey_End },
			{ EKeys::Home.GetFName(), ImGuiKey_Home },

			{ EKeys::Left.GetFName(), ImGuiKey_LeftArrow },
			{ EKeys::Up.GetFName(), ImGuiKey_UpArrow },
			{ EKeys::Right.GetFName(), ImGuiKey_RightArrow },
			{ EKeys::Down.GetFName(), ImGuiKey_DownArrow },

			{ EKeys::Insert.GetFName(), ImGuiKey_Insert },
			{ EKeys::Delete.GetFName(), ImGuiKey_Delete },

			{ EKeys::Zero.GetFName(), ImGuiKey_0 },
			{ EKeys::One.GetFName(), ImGuiKey_1 },
			{ EKeys::Two.GetFName(), ImGuiKey_2 },
			{ EKeys::Three.GetFName(), ImGuiKey_3 },
			{ EKeys::Four.GetFName(), ImGuiKey_4 },
			{ EKeys::Five.GetFName(), ImGuiKey_5 },
			{ EKeys::Six.GetFName(), ImGuiKey_6 },
			{ EKeys::Seven.GetFName(), ImGuiKey_7 },
			{ EKeys::Eight.GetFName(), ImGuiKey_8 },
			{ EKeys::Nine.GetFName(), ImGuiKey_9 },

			{ EKeys::A.GetFName(), ImGuiKey_A },
			{ EKeys::B.GetFName(), ImGuiKey_B },
			{ EKeys::C.GetFName(), ImGuiKey_C },
			{ EKeys::D.GetFName(), ImGuiKey_D },
			{ EKeys::E.GetFName(), ImGuiKey_E },
			{ EKeys::F.GetFName(), ImGuiKey_F },
			{ EKeys::G.GetFName(), ImGuiKey_G },
			{ EKeys::H.GetFName(), ImGuiKey_H },
			{ EKeys::I.GetFName(), ImGuiKey_I },
			{ EKeys::J.GetFName(), ImGuiKey_J },
			{ EKeys::K.GetFName(), ImGuiKey_K },
			{ EKeys::L.GetFName(), ImGuiKey_L },
			{ EKeys::M.GetFName(), ImGuiKey_M },
			{ EKeys::N.GetFName(), ImGuiKey_N },
			{ EKeys::O.GetFName(), ImGuiKey_O },
			{ EKeys::P.GetFName(), ImGuiKey_P },
			{ EKeys::Q.GetFName(), ImGuiKey_Q },
			{ EKeys::R.GetFName(), ImGuiKey_R },
			{ EKeys::S.GetFName(), ImGuiKey_S },
			{ EKeys::T.GetFName(), ImGuiKey_T },
			{ EKeys::U.GetFName(), ImGuiKey_U },
			{ EKeys::V.GetFName(), ImGuiKey_V },
			{ EKeys::W.GetFName(), ImGuiKey_W },
			{ EKeys::X.GetFName(), ImGuiKey_X },
			{ EKeys::Y.GetFName(), ImGuiKey_Y },
			{ EKeys::Z.GetFName(), ImGuiKey_Z },

			{ EKeys::NumPadZero.GetFName(), ImGuiKey_Keypad0 },
			{ EKeys::NumPadOne.GetFName(), ImGuiKey_Keypad1 },
			{ EKeys::NumPadTwo.GetFName(), ImGuiKey_Keypad2 },
			{ EKeys::NumPadThree.GetFName(), ImGuiKey_Keypad3 },
			{ EKeys::NumPadFour.GetFName(), ImGuiKey_Keypad4 },
			{ EKeys::NumPadFive.GetFName(), ImGuiKey_Keypad5 },
			{ EKeys::NumPadSix.GetFName(), ImGuiKey_Keypad6 },
			{ EKeys::NumPadSeven.GetFName(), ImGuiKey_Keypad7 },
			{ EKeys::NumPadEight.GetFName(), ImGuiKey_Keypad8 },
			{ EKeys::NumPadNine.GetFName(), ImGuiKey_Keypad9 },

			{ EKeys::Multiply.GetFName(), ImGuiKey_KeypadMultiply },
			{ EKeys::Add.GetFName(), ImGuiKey_KeypadAdd },
			{ EKeys::Subtract.GetFName(), ImGuiKey_KeypadSubtract },
			{ EKeys::Decimal.GetFName(), ImGuiKey_KeypadDecimal },
			{ EKeys::Divide.GetFName(), ImGuiKey_KeypadDivide },

			{ EKeys::F1.GetFName(), ImGuiKey_F1 },
			{ EKeys::F2.GetFName(), ImGuiKey_F2 },
			{ EKeys::F3.GetFName(), ImGuiKey_F3 },
			{ EKeys::F4.GetFName(), ImGuiKey_F4 },
			{ EKeys::F5.GetFName(), ImGuiKey_F5 },
			{ EKeys::F6.GetFName(), ImGuiKey_F6 },
			{ EKeys::F7.GetFName(), ImGuiKey_F7 },
			{ EKeys::F8.GetFName(), ImGuiKey_F8 },
			{ EKeys::F9.GetFName(), ImGuiKey_F9 },
			{ EKeys::F10.GetFName(), ImGuiKey_F10 },
			{ EKeys::F11.GetFName(), ImGuiKey_F11 },
			{ EKeys::F12.GetFName(), ImGuiKey_F12 },

			{ EKeys::NumLock.GetFName(), ImGuiKey_NumLock },
			{ EKeys::ScrollLock.GetFName(), ImGuiKey_ScrollLock },

			{ EKeys::LeftShift.GetFName(), ImGuiKey_LeftShift },
			{ EKeys::RightShift.GetFName(), ImGuiKey_RightShift },
			{ EKeys::LeftControl.GetFName(), ImGuiKey_LeftCtrl },
			{ EKeys::RightControl.GetFName(), ImGuiKey_RightCtrl },
			{ EKeys::LeftAlt.GetFName(), ImGuiKey_LeftAlt },
			{ EKeys::RightAlt.GetFName(), ImGuiKey_RightAlt },
			{ EKeys::LeftCommand.GetFName(), ImGuiKey_None },
			{ EKeys::RightCommand.GetFName(), ImGuiKey_None },

			{ EKeys::Semicolon.GetFName(), ImGuiKey_Semicolon },
			{ EKeys::Equals.GetFName(), ImGuiKey_Equal },
			{ EKeys::Comma.GetFName(), ImGuiKey_Comma },
			{ EKeys::Underscore.GetFName(), ImGuiKey_None },
			{ EKeys::Hyphen.GetFName(), ImGuiKey_None },
			{ EKeys::Period.GetFName(), ImGuiKey_Period },
			{ EKeys::Slash.GetFName(), ImGuiKey_Slash },
			{ EKeys::Tilde.GetFName(), ImGuiKey_None },
			{ EKeys::LeftBracket.GetFName(), ImGuiKey_LeftBracket },
			{ EKeys::Backslash.GetFName(), ImGuiKey_Backslash },
			{ EKeys::RightBracket.GetFName(), ImGuiKey_RightBracket },
			{ EKeys::Apostrophe.GetFName(), ImGuiKey_Apostrophe },

			{ EKeys::Ampersand.GetFName(), ImGuiKey_None },
			{ EKeys::Asterix.GetFName(), ImGuiKey_None },
			{ EKeys::Caret.GetFName(), ImGuiKey_None },
			{ EKeys::Colon.GetFName(), ImGuiKey_None },
			{ EKeys::Dollar.GetFName(), ImGuiKey_None },
			{ EKeys::Exclamation.GetFName(), ImGuiKey_None },
			{ EKeys::LeftParantheses.GetFName(), ImGuiKey_None },
			{ EKeys::RightParantheses.GetFName(), ImGuiKey_None },
			{ EKeys::Quote.GetFName(), ImGuiKey_None },

			{ EKeys::Gamepad_LeftThumbstick.GetFName(), ImGuiKey_GamepadL3 },
			{ EKeys::Gamepad_RightThumbstick.GetFName(), ImGuiKey_GamepadR3 },
			{ EKeys::Gamepad_Special_Left.GetFName(), ImGuiKey_GamepadBack },
			{ EKeys::Gamepad_Special_Left_X.GetFName(), ImGuiKey_None },
			{ EKeys::Gamepad_Special_Left_Y.GetFName(), ImGuiKey_None },
			{ EKeys::Gamepad_Special_Right.GetFName(), ImGuiKey_GamepadStart },
			{ EKeys::Gamepad_FaceButton_Bottom.GetFName(), ImGuiKey_GamepadFaceDown },
			{ EKeys::Gamepad_FaceButton_Right.GetFName(), ImGuiKey_GamepadFaceRight },
			{ EKeys::Gamepad_FaceButton_Left.GetFName(), ImGuiKey_GamepadFaceLeft },
			{ EKeys::Gamepad_FaceButton_Top.GetFName(), ImGuiKey_GamepadFaceUp },
			{ EKeys::Gamepad_LeftShoulder.GetFName(), ImGuiKey_GamepadL1 },
			{ EKeys::Gamepad_RightShoulder.GetFName(), ImGuiKey_GamepadR1 },
			{ EKeys::Gamepad_LeftTrigger.GetFName(), ImGuiKey_GamepadL2 },
			{ EKeys::Gamepad_RightTrigger.GetFName(), ImGuiKey_GamepadR2 },
			{ EKeys::Gamepad_DPad_Up.GetFName(), ImGuiKey_GamepadDpadUp },
			{ EKeys::Gamepad_DPad_Down.GetFName(), ImGuiKey_GamepadDpadDown },
			{ EKeys::Gamepad_DPad_Right.GetFName(), ImGuiKey_GamepadDpadRight },
			{ EKeys::Gamepad_DPad_Left.GetFName(), ImGuiKey_GamepadDpadLeft },

			{ EKeys::Gamepad_LeftStick_Up.GetFName(), ImGuiKey_GamepadLStickUp },
			{ EKeys::Gamepad_LeftStick_Down.GetFName(), ImGuiKey_GamepadLStickDown },
			{ EKeys::Gamepad_LeftStick_Right.GetFName(), ImGuiKey_GamepadLStickRight },
			{ EKeys::Gamepad_LeftStick_Left.GetFName(), ImGuiKey_GamepadLStickLeft },

			{ EKeys::Gamepad_RightStick_Up.GetFName(), ImGuiKey_GamepadRStickUp },
			{ EKeys::Gamepad_RightStick_Down.GetFName(), ImGuiKey_GamepadRStickDown },
			{ EKeys::Gamepad_RightStick_Right.GetFName(), ImGuiKey_GamepadRStickRight },
			{ EKeys::Gamepad_RightStick_Left.GetFName(), ImGuiKey_GamepadRStickLeft },
		};

		const ImGuiKey* Key = KeyMap.Find(Keyname);
		return Key ? *Key : ImGuiKey_None;
	}

	FORCEINLINE static EMouseCursor::Type ImGuiToUnrealCursor(ImGuiMouseCursor Cursor)
	{
		static constexpr EMouseCursor::Type ImGuiToSlateCursor[] =
		{
			EMouseCursor::Default,
			EMouseCursor::TextEditBeam,
			EMouseCursor::CardinalCross,
			EMouseCursor::ResizeUpDown,
			EMouseCursor::ResizeLeftRight,
			EMouseCursor::ResizeSouthWest,
			EMouseCursor::ResizeSouthEast,
			EMouseCursor::Hand,
			EMouseCursor::Default,
			EMouseCursor::Default,
			EMouseCursor::SlashedCircle,
		};
		static_assert(UE_ARRAY_COUNT(ImGuiToSlateCursor) == ImGuiMouseCursor_COUNT);

		return ImGuiToSlateCursor[Cursor];
	}

	FORCEINLINE static int32 UnrealToImGuiMouseButton(FKey MouseKey)
	{
		int32 MouseButton = ImGuiMouseButton_Left; //MouseKey == EKeys::LeftMouseButton
		if (MouseKey == EKeys::RightMouseButton)
		{
			MouseButton = ImGuiMouseButton_Right;
		}
		else if (MouseKey == EKeys::MiddleMouseButton)
		{
			MouseButton = ImGuiMouseButton_Middle;
		}
		return MouseButton;
	}
}

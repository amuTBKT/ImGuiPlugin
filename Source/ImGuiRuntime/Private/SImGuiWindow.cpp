#include "SImGuiWindow.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "ImGuiRuntimeModule.h"

#if STATS
#include "Stats/StatsData.h"
#endif

DECLARE_CYCLE_STAT(TEXT("ImGui Tick"), STAT_TickWidget, STATGROUP_ImGui);
DECLARE_CYCLE_STAT(TEXT("ImGui Render"), STAT_RenderWidget, STATGROUP_ImGui);

void SImGuiWindow::Construct(const FArguments& InArgs)
{
	m_ImGuiContext = ImGui::CreateContext();

	ImGui::SetCurrentContext(m_ImGuiContext);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	//[TODO] setting?
	ImGui::StyleColorsDark();

	//[TODO] not using Imgui font, we use texture asset instead
	io.Fonts->Build();

	FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	io.Fonts->TexID = ImGuiRuntimeModule.GetDefaultFontTextureID();
}

SImGuiWindow::~SImGuiWindow()
{
	if (m_ImGuiContext)
	{
		ImGui::DestroyContext(m_ImGuiContext);
		m_ImGuiContext = nullptr;
	}
}

void SImGuiWindow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickWidget);

	Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	ImGuiIO& IO = GetImGuiIO();
	
	// new frame setup
	{
		ImGui::NewFrame();

		IO.DisplaySize = { (float)AllottedGeometry.GetAbsoluteSize().X, (float)AllottedGeometry.GetAbsoluteSize().Y };
		IO.DeltaTime = InDeltaTime;		
	}

	TickInternal(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SImGuiWindow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWidget);

	checkf(m_ImGuiContext, TEXT("ImGuiContext is invalid!"));

	ImGui::SetCurrentContext(m_ImGuiContext);
	ImGui::Render();

	FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");

	ImDrawData* DrawData = ImGui::GetDrawData();
	if (DrawData->DisplaySize.x > 0.0f && DrawData->DisplaySize.y > 0.0f)
	{
		if (DrawData->TotalVtxCount > 0 && DrawData->TotalIdxCount > 0)
		{
#if 1
			// needed to adjust for border and title bar...
			const FSlateRenderTransform& WidgetOffsetTransform = AllottedGeometry.GetAccumulatedRenderTransform();

			TArray<FSlateVertex> VertexBuffer;
			TArray<SlateIndex> IndexBuffer;
			int32 IndexBufferOffset = 0;
			for (int32 CmdListIndex = 0; CmdListIndex < DrawData->CmdListsCount; CmdListIndex++)
			{
				const ImDrawList* CmdList = DrawData->CmdLists[CmdListIndex];

				VertexBuffer.SetNum(CmdList->VtxBuffer.Size, false);
				for (int32 VertIndex = 0; VertIndex < CmdList->VtxBuffer.Size; ++VertIndex)
				{
					const ImDrawVert& ImGuiVertex = CmdList->VtxBuffer[VertIndex];
					FSlateVertex& SlateVertex = VertexBuffer[VertIndex];

					const FVector2D TransformedPos = WidgetOffsetTransform.TransformPoint(FVector2D{ ImGuiVertex.pos.x, ImGuiVertex.pos.y });
					SlateVertex.Position = { (float)TransformedPos.X, (float)TransformedPos.Y };

					SlateVertex.TexCoords[0] = ImGuiVertex.uv.x;
					SlateVertex.TexCoords[1] = ImGuiVertex.uv.y;
					SlateVertex.TexCoords[2] = SlateVertex.TexCoords[3] = 1.f;

					SlateVertex.Color.AlignmentDummy = ImGuiVertex.col;
					std::swap(SlateVertex.Color.R, SlateVertex.Color.B);
				}

				for (int CmdBufferIndex = 0; CmdBufferIndex < CmdList->CmdBuffer.Size; CmdBufferIndex++)
				{
					const ImDrawCmd& DrawCommand = CmdList->CmdBuffer[CmdBufferIndex];

					IndexBuffer.SetNum(DrawCommand.ElemCount, false);
					for (uint32 Index = 0; Index < DrawCommand.ElemCount; ++Index)
					{
						IndexBuffer[Index] = CmdList->IdxBuffer[DrawCommand.IdxOffset + Index];
					}

					FSlateRect ImGuiClippingRect;
					ImGuiClippingRect.Left = DrawCommand.ClipRect.x;
					ImGuiClippingRect.Top = DrawCommand.ClipRect.y;
					ImGuiClippingRect.Right = DrawCommand.ClipRect.z;
					ImGuiClippingRect.Bottom = DrawCommand.ClipRect.w;

					// Transform clipping rectangle to screen space and apply to elements that we draw.
					const FSlateRect ClippingRect = TransformRect(WidgetOffsetTransform, ImGuiClippingRect).IntersectionWith(MyClippingRect);
					OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });

					// Add elements to the list.
					FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, ImGuiRuntimeModule.GetResourceHandle(DrawCommand.TextureId), VertexBuffer, IndexBuffer, nullptr, 0, 0);

					OutDrawElements.PopClip();
				}
			}
#else

#endif
		}
	}

	return Super::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
}

#pragma region SLATE_INPUT
static ImGuiKey FKeyToImGuiKey(FName Keyname)
{
#define LITERAL_TRANSLATION(Key) { EKeys::##Key.GetFName(), ImGuiKey_##Key }
	// not an exhaustive mapping, some keys are missing :^|
	static const TMap<FName, ImGuiKey> FKeyToImGuiKey =
	{
		LITERAL_TRANSLATION(A), LITERAL_TRANSLATION(B), LITERAL_TRANSLATION(C), LITERAL_TRANSLATION(D), LITERAL_TRANSLATION(E), LITERAL_TRANSLATION(F),
		LITERAL_TRANSLATION(G), LITERAL_TRANSLATION(H), LITERAL_TRANSLATION(I), LITERAL_TRANSLATION(J), LITERAL_TRANSLATION(K), LITERAL_TRANSLATION(L),
		LITERAL_TRANSLATION(M), LITERAL_TRANSLATION(N), LITERAL_TRANSLATION(O), LITERAL_TRANSLATION(P), LITERAL_TRANSLATION(Q), LITERAL_TRANSLATION(R),
		LITERAL_TRANSLATION(S), LITERAL_TRANSLATION(T), LITERAL_TRANSLATION(U), LITERAL_TRANSLATION(V), LITERAL_TRANSLATION(W), LITERAL_TRANSLATION(X),
		LITERAL_TRANSLATION(Y), LITERAL_TRANSLATION(Z),
		LITERAL_TRANSLATION(F1), LITERAL_TRANSLATION(F2), LITERAL_TRANSLATION(F3), LITERAL_TRANSLATION(F4),
		LITERAL_TRANSLATION(F5), LITERAL_TRANSLATION(F6), LITERAL_TRANSLATION(F7), LITERAL_TRANSLATION(F8),
		LITERAL_TRANSLATION(F9), LITERAL_TRANSLATION(F10), LITERAL_TRANSLATION(F11), LITERAL_TRANSLATION(F12),
		LITERAL_TRANSLATION(Enter), LITERAL_TRANSLATION(Insert), LITERAL_TRANSLATION(Delete), LITERAL_TRANSLATION(Escape), LITERAL_TRANSLATION(Tab),
		LITERAL_TRANSLATION(PageUp), LITERAL_TRANSLATION(PageDown), LITERAL_TRANSLATION(Home), LITERAL_TRANSLATION(End),
		LITERAL_TRANSLATION(NumLock), LITERAL_TRANSLATION(ScrollLock), LITERAL_TRANSLATION(CapsLock),
		LITERAL_TRANSLATION(RightBracket), LITERAL_TRANSLATION(LeftBracket), LITERAL_TRANSLATION(Backslash), LITERAL_TRANSLATION(Slash),
		LITERAL_TRANSLATION(Semicolon), LITERAL_TRANSLATION(Period), LITERAL_TRANSLATION(Comma), LITERAL_TRANSLATION(Apostrophe), LITERAL_TRANSLATION(Pause),
		{ EKeys::Zero.GetFName(), ImGuiKey_0 }, { EKeys::One.GetFName(), ImGuiKey_1 }, { EKeys::Two.GetFName(), ImGuiKey_2 },
		{ EKeys::Three.GetFName(), ImGuiKey_3 }, { EKeys::Four.GetFName(), ImGuiKey_4 }, { EKeys::Five.GetFName(), ImGuiKey_5 },
		{ EKeys::Six.GetFName(), ImGuiKey_6 }, { EKeys::Seven.GetFName(), ImGuiKey_7 }, { EKeys::Eight.GetFName(), ImGuiKey_8 }, { EKeys::Nine.GetFName(), ImGuiKey_9 },
		{ EKeys::NumPadZero.GetFName(), ImGuiKey_Keypad0 }, { EKeys::NumPadOne.GetFName(), ImGuiKey_Keypad1 }, { EKeys::NumPadTwo.GetFName(), ImGuiKey_Keypad2 },
		{ EKeys::NumPadThree.GetFName(), ImGuiKey_Keypad3 }, { EKeys::NumPadFour.GetFName(), ImGuiKey_Keypad4 }, { EKeys::NumPadFive.GetFName(), ImGuiKey_Keypad5 },
		{ EKeys::NumPadSix.GetFName(), ImGuiKey_Keypad6 }, { EKeys::NumPadSeven.GetFName(), ImGuiKey_Keypad7 }, { EKeys::NumPadEight.GetFName(), ImGuiKey_Keypad8 },
		{ EKeys::NumPadNine.GetFName(), ImGuiKey_Keypad9 },
		{ EKeys::LeftShift.GetFName(), ImGuiKey_LeftShift }, { EKeys::LeftControl.GetFName(), ImGuiKey_LeftCtrl }, { EKeys::LeftAlt.GetFName(), ImGuiKey_LeftAlt },
		{ EKeys::RightShift.GetFName(), ImGuiKey_RightShift }, { EKeys::RightControl.GetFName(), ImGuiKey_RightCtrl }, { EKeys::RightAlt.GetFName(), ImGuiKey_RightAlt },
		{ EKeys::SpaceBar.GetFName(), ImGuiKey_Space }, { EKeys::BackSpace.GetFName(), ImGuiKey_Backspace },
		{ EKeys::Up.GetFName(), ImGuiKey_UpArrow }, { EKeys::Down.GetFName(), ImGuiKey_DownArrow },
		{ EKeys::Left.GetFName(), ImGuiKey_LeftArrow }, { EKeys::Right.GetFName(), ImGuiKey_RightArrow },
		{ EKeys::Subtract.GetFName(), ImGuiKey_KeypadSubtract }, { EKeys::Add.GetFName(), ImGuiKey_KeypadAdd },
		{ EKeys::Multiply.GetFName(), ImGuiKey_KeypadMultiply }, { EKeys::Divide.GetFName(), ImGuiKey_KeypadDivide },
		{ EKeys::Decimal.GetFName(), ImGuiKey_KeypadDecimal }, { EKeys::Equals.GetFName(), ImGuiKey_Equal },
	};
#undef LITERAL_TRANSLATION

	/*
	[TODO] These are not added....
	switch (KeyCode)
	{
	case VK_OEM_3: return ImGuiKey_GraveAccent;
	case VK_LWIN: return ImGuiKey_LeftSuper;
	case VK_RWIN: return ImGuiKey_RightSuper;
	case VK_APPS: return ImGuiKey_Menu;
	default: return ImGuiKey_None;
	}
	*/

	const ImGuiKey* Key = FKeyToImGuiKey.Find(Keyname);
	return Key ? *Key : ImGuiKey_None;
}

static int32 FMouseKeyToImGuiKey(FKey MouseKey)
{
	int32 MouseButton = 0;
	if (MouseKey == EKeys::LeftMouseButton)
	{
		MouseButton = 0;
	}
	else if (MouseKey == EKeys::RightMouseButton)
	{
		MouseButton = 1;
	}
	else if (MouseKey == EKeys::MiddleMouseButton)
	{
		MouseButton = 2;
	}
	return MouseButton;
}

void SImGuiWindow::AddMouseButtonEvent(FKey MouseKey, bool IsDown)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseButtonEvent(FMouseKeyToImGuiKey(MouseKey), IsDown);
}

void SImGuiWindow::AddKeyEvent(FKeyEvent KeyEvent, bool IsDown)
{
	ImGuiIO& IO = GetImGuiIO();

	const ImGuiKey ImGuiKey = FKeyToImGuiKey(KeyEvent.GetKey().GetFName());
	if (ImGuiKey != ImGuiKey_None)
	{
		IO.AddKeyEvent(ImGuiKey, IsDown);
	}

	IO.AddKeyEvent(ImGuiKey_ModShift, KeyEvent.GetModifierKeys().IsShiftDown());
	IO.AddKeyEvent(ImGuiKey_ModCtrl, KeyEvent.GetModifierKeys().IsControlDown());
	IO.AddKeyEvent(ImGuiKey_ModAlt, KeyEvent.GetModifierKeys().IsAltDown());
}

FReply SImGuiWindow::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddInputCharacterUTF16(CharacterEvent.GetCharacter());

	return FReply::Handled();
}

FReply SImGuiWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(KeyEvent, true);

	return FReply::Handled();
}

FReply SImGuiWindow::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(KeyEvent, false);

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), true);

	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SImGuiWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), false);

	return FReply::Handled().ReleaseMouseCapture();
}

FReply SImGuiWindow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), true);

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseWheelEvent(0.f, MouseEvent.GetWheelDelta());

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(LocalMousePosition.X, LocalMousePosition.Y);

	return FReply::Handled();
}
#pragma endregion SLATE_INPUT

void SImGuiMainWindow::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	if (ImGuiRuntimeModule.GetMainWindowTickDelegate().IsBound())
	{
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
		ImGuiRuntimeModule.GetMainWindowTickDelegate().Broadcast(InDeltaTime);
	}
	else
	{
		const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGui::SetNextWindowDockID(MainDockSpaceID);
		if (ImGui::Begin("Empty", nullptr))
		{
			ImGui::Text("Nothing to display...");
			ImGui::End();
		}
	}
}

void SImGuiWidgetWindow::Construct(const FArguments& InArgs)
{
	Super::Construct(Super::FArguments());

	m_OnTickDelegate = InArgs._OnTickDelegate;
}

void SImGuiWidgetWindow::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
	ImGui::SetNextWindowDockID(MainDockSpaceID);

	m_OnTickDelegate.ExecuteIfBound(InDeltaTime);
}
#pragma once

#include <Widgets/DeclarativeSyntaxSupport.h>
#include <Widgets/SCompoundWidget.h>
#include <Textures/SlateShaderResource.h>
#include "ImGuiPluginDelegates.h"
#include "imgui.h"

struct ImGuiContext;

class IMGUIPLUGIN_API SImGuiWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImGuiWindow)
	{		
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SImGuiWindow();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;

#pragma region SLATE_INPUT
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
#pragma endregion SLATE_INPUT

private:
	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) = 0;

	ImGuiIO& GetImGuiIO()
	{
		checkf(m_ImGuiContext, TEXT("ImGuiContext is invalid!"));

		ImGui::SetCurrentContext(m_ImGuiContext);
		return ImGui::GetIO();
	}

	void AddMouseButtonEvent(FKey MouseKey, bool IsDown);
	void AddKeyEvent(FKeyEvent KeyEvent, bool IsDown);

private:
	using Super = SCompoundWidget;

protected:
	ImGuiContext* m_ImGuiContext = nullptr;
};

/* Default window, only one instance active at a time */
class SImGuiMainWindow : public SImGuiWindow
{
private:
	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	using Super = SImGuiWindow;
};

/* Window used for dynamic widgets (ColorPicker etc..) */
class SImGuiWidgetWindow : public SImGuiWindow
{
public:
	SLATE_BEGIN_ARGS(SImGuiWidgetWindow)
		:_OnTickDelegate() {}
		SLATE_EVENT(FOnTickImGuiWidgetDelegate, OnTickDelegate);	
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

private:
	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	using Super = SImGuiWindow;

	FOnTickImGuiWidgetDelegate m_OnTickDelegate = {};
};
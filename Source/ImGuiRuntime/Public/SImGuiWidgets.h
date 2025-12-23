// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Widgets/SLeafWidget.h"
#include "ImGuiPluginDelegates.h"
#include "Containers/AnsiString.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct ImGuiIO;
struct ImGuiContext;
struct FImGuiTickContext;

namespace ImGuiUtils
{
	class FWidgetDrawer;
	class SImGuiViewportWidget;
}

class IMGUIRUNTIME_API SImGuiWidgetBase : public SLeafWidget
{
	using Super = SLeafWidget;

	friend class ImGuiUtils::SImGuiViewportWidget;

public:
	SLATE_BEGIN_ARGS(SImGuiWidgetBase)
		: _MainViewportWindow(nullptr)
		, _ConfigFileName(nullptr)
		, _bEnableViewports(true)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, MainViewportWindow);
		SLATE_ARGUMENT(const ANSICHAR*, ConfigFileName);
		SLATE_ARGUMENT(bool, bEnableViewports);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SImGuiWidgetBase();

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual void Tick(const FGeometry& WidgetGeometry, const double InCurrentTime, const float InDeltaTime) override final;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& WidgetGeometry, const FSlateRect& ClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override final;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyChar(const FGeometry& WidgetGeometry, const FCharacterEvent& CharacterEvent) override;

	virtual FReply OnKeyDown(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override;

	virtual FReply OnKeyUp(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override;

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDown(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseWheel(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseMove(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override;

	virtual FCursorReply OnCursorQuery(const FGeometry& WidgetGeometry, const FPointerEvent& CursorEvent) const override;

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	FORCEINLINE ImGuiIO& GetImGuiIO() const;
	FORCEINLINE void AddKeyEvent(ImGuiIO& IO, FKeyEvent KeyEvent, bool IsDown);

	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) = 0;

protected:
	FAnsiString m_ConfigFilePath;
	ImGuiContext* m_ImGuiContext = nullptr;
	TSharedPtr<ImGuiUtils::FWidgetDrawer> m_WidgetDrawers[2];

	// TODO: initial zoom support, can we do better than this?
	float m_WindowScale = 1.f;

	bool m_IsDragOverActive = false;
	TSharedPtr<class FDragDropOperation> LastDragDropOperation;
};

/* Dynamic widgets (ColorPicker etc..) */
class IMGUIRUNTIME_API SImGuiWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	SLATE_BEGIN_ARGS(SImGuiWidget)
		: _MainViewportWindow(nullptr)
		, _OnTickDelegate()
		, _ConfigFileName(nullptr)
		, _bEnableViewports(true)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, MainViewportWindow);
		SLATE_EVENT(FOnTickImGuiWidgetDelegate, OnTickDelegate);
		SLATE_ARGUMENT(const ANSICHAR*, ConfigFileName);
		SLATE_ARGUMENT(bool, bEnableViewports);
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

private:
	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) override;

private:
	FOnTickImGuiWidgetDelegate m_OnTickDelegate = {};
};

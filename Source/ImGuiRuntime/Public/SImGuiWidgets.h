// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "UObject/GCObject.h"

#include "ImGuiPluginDelegates.h"

struct ImGuiIO;
struct ImGuiContext;
struct FImGuiTickContext;
class UTextureRenderTarget2D;

namespace ImGuiUtils
{
	class SImGuiViewportWidget;
}

class IMGUIRUNTIME_API SImGuiWidgetBase : public SLeafWidget, public FGCObject
{
	using Super = SLeafWidget;

	struct FImGuiTickResult
	{
		bool bWasDragOperationHandled = false;
	};

	friend class ImGuiUtils::SImGuiViewportWidget;

public:
	SLATE_BEGIN_ARGS(SImGuiWidgetBase)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> MainViewportWindow, bool UseTranslucentBackground);
	virtual ~SImGuiWidgetBase();

	// GCObject interface 
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// GCObject interface END

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

	FImGuiTickResult TickImGui(const FGeometry* WidgetGeometry, FImGuiTickContext* TickContext);
	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) = 0;

protected:
	FSlateBrush m_ImGuiSlateBrush;
	TObjectPtr<UTextureRenderTarget2D> m_ImGuiRT = nullptr;

	ImGuiContext* m_ImGuiContext = nullptr;

	// TODO: initial zoom support, can we do better than this?
	float m_WindowScale = 1.f;

	// minor optimization to avoid texture clears when using opaque window.
	bool m_ClearRenderTargetEveryFrame = false;

	// widgets can often be ticked from input events, so keep track to avoid ticking them again during Tick event.
	bool m_ImGuiTickedByInputProcessing = false;
	bool m_IsDragOverActive = false;
};

/* Main window widget, only one instance active at a time */
class SImGuiMainWindowWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> MainViewportWindow);

private:
	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) override;
};

/* Dynamic widgets (ColorPicker etc..) */
class IMGUIRUNTIME_API SImGuiWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	SLATE_BEGIN_ARGS(SImGuiWidget)
		: _OnTickDelegate()
		{}
		SLATE_EVENT(FOnTickImGuiWidgetDelegate, OnTickDelegate);
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> MainViewportWindow);

private:
	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) override;

private:
	FOnTickImGuiWidgetDelegate m_OnTickDelegate = {};
};

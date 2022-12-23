#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"

#include "ImGuiPluginDelegates.h"

struct ImGuiContext;
class UTextureRenderTarget2D;

class IMGUIRUNTIME_API SImGuiWidgetBase : public SCompoundWidget, public FGCObject
{
	using Super = SCompoundWidget;
public:
	SLATE_BEGIN_ARGS(SImGuiWidgetBase)
	{		
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SImGuiWidgetBase();

	//~ GCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ GCObject Interface

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	FORCEINLINE ImGuiIO& GetImGuiIO() const;
	
	FORCEINLINE void AddMouseButtonEvent(FKey MouseKey, bool IsDown);
	FORCEINLINE void AddKeyEvent(FKeyEvent KeyEvent, bool IsDown);

	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) = 0;

protected:
	FSlateBrush m_ImGuiSlateBrush;

	ImGuiContext* m_ImGuiContext = nullptr;
	UTextureRenderTarget2D* m_ImGuiRT = nullptr;

	// TODO: initial zoom support, can we do better than this?
	float m_WindowScale = 1.f;
};

/* Main window widget, only one instance active at a time */
class SImGuiMainWindowWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
private:
	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};

/* Dynamic widgets (ColorPicker etc..) */
class IMGUIRUNTIME_API SImGuiWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	SLATE_BEGIN_ARGS(SImGuiWidget)
		: _OnTickDelegate()
		, _AllowUndocking(true)
		{}
		SLATE_EVENT(FOnTickImGuiWidgetDelegate, OnTickDelegate);	
		SLATE_ATTRIBUTE(bool, AllowUndocking);	
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

private:
	virtual void TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FOnTickImGuiWidgetDelegate m_OnTickDelegate = {};
	bool m_AllowUndocking = false;
};
// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Delegates/Delegate.h"

// ドロップターゲットラッパー / Drop target wrapper with visual border feedback.
// Content をラップし、ドラッグオーバー中だけ青い枠線を描画する。
class SDropTargetOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDropTargetOverlay) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	using FOnDragDropDelegate = TDelegate<FReply(const FGeometry&, const FDragDropEvent&)>;

	FOnDragDropDelegate OnDragOverDelegate;
	FOnDragDropDelegate OnDropDelegate;

	void Construct(const FArguments& InArgs);

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	bool bIsDragOver = false;
};

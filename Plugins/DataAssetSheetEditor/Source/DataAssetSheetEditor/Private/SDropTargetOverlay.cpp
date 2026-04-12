// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDropTargetOverlay.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"

void SDropTargetOverlay::Construct(const FArguments& InArgs)
{
	bIsDragOver = false;
	ChildSlot[ InArgs._Content.Widget ];
}

FReply SDropTargetOverlay::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnDragOverDelegate.IsBound())
	{
		FReply Reply = OnDragOverDelegate.Execute(MyGeometry, DragDropEvent);
		if (Reply.IsEventHandled())
		{
			bIsDragOver = true;
			return Reply;
		}
	}
	bIsDragOver = false;
	return FReply::Unhandled();
}

void SDropTargetOverlay::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDragLeave(DragDropEvent);
	bIsDragOver = false;
}

FReply SDropTargetOverlay::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bIsDragOver = false;
	if (OnDropDelegate.IsBound())
	{
		return OnDropDelegate.Execute(MyGeometry, DragDropEvent);
	}
	return FReply::Unhandled();
}

int32 SDropTargetOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (bIsDragOver)
	{
		const FLinearColor BorderColor(0.2f, 0.6f, 1.0f, 0.8f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			FAppStyle::GetBrush("Border"),
			ESlateDrawEffect::None,
			BorderColor
		);
		return LayerId + 1;
	}
	return LayerId;
}

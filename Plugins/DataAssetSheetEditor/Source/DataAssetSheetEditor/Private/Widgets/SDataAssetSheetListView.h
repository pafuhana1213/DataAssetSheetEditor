// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Input/Reply.h"

struct FDataAssetRowData;

class SDataAssetSheetListView : public SListView<TSharedPtr<FDataAssetRowData>>
{
public:
	void SetHorizontalScrollBox(TSharedPtr<SScrollBox> InBox)
	{
		HScrollBox = InBox;
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.IsShiftDown())
		{
			if (TSharedPtr<SScrollBox> Pinned = HScrollBox.Pin())
			{
				const float Step = 96.0f;
				const float NewOffset = Pinned->GetScrollOffset() - MouseEvent.GetWheelDelta() * Step;
				Pinned->SetScrollOffset(NewOffset);
				return FReply::Handled();
			}
		}
		return SListView<TSharedPtr<FDataAssetRowData>>::OnMouseWheel(MyGeometry, MouseEvent);
	}

private:
	TWeakPtr<SScrollBox> HScrollBox;
};

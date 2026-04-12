// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Styling/SlateColor.h"

class FAssetThumbnailPool;
class FDataAssetSheetModel;
struct FDataAssetRowData;

// テーブル行ウィジェット / Table row widget for SDataAssetSheetEditor
class SDataAssetSheetRow : public SMultiColumnTableRow<TSharedPtr<FDataAssetRowData>>
{
public:
	SLATE_BEGIN_ARGS(SDataAssetSheetRow)
		: _IndexInList(0)
	{}
		SLATE_ARGUMENT(int32, IndexInList)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable,
		TSharedPtr<FDataAssetRowData> InRowData, TSharedPtr<FDataAssetSheetModel> InModel,
		TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> InListView,
		TSharedPtr<FAssetThumbnailPool> InThumbnailPool);

	virtual const FSlateBrush* GetBorder() const override;

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

private:
	FSlateColor GetRowTextColor() const;
	TSharedRef<SWidget> GenerateCellContent(const FName& ColumnId);
	void OnAssetNameClicked();

	TSharedPtr<FDataAssetRowData> RowData;
	TSharedPtr<FDataAssetSheetModel> Model;
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> OwnerListView;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	int32 IndexInList = 0;
};

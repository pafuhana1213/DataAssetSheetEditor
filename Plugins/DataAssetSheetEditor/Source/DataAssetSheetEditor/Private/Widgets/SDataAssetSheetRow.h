// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Styling/SlateColor.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FAssetThumbnailPool;
class FDataAssetSheetModel;
class UDataAssetSheet;
struct FDataAssetRowData;
struct FAssetData;

// 行のアセット差し替え通知デリゲート / Notify the editor to replace this row's asset
// OldPath = 行の現在のアセットパス, NewAsset = ピッカーで選ばれたアセット
DECLARE_DELEGATE_TwoParams(FOnReplaceRowAsset, const FSoftObjectPath& /*OldPath*/, const FAssetData& /*NewAsset*/);

// 行削除通知デリゲート / Notify the editor to delete this row from ManualAssets
DECLARE_DELEGATE_OneParam(FOnDeleteRow, TSharedPtr<FDataAssetRowData> /*RowData*/);

// 行並び替え通知デリゲート / Notify the editor to reorder ManualAssets via drag and drop
// DraggedRows をまとめて TargetRow の上(Above)/下(Below)へ移動する
DECLARE_DELEGATE_ThreeParams(FOnReorderRows, const TArray<TSharedPtr<FDataAssetRowData>>& /*DraggedRows*/,
	TSharedPtr<FDataAssetRowData> /*TargetRow*/, EItemDropZone /*DropZone*/);

// 行並び替え用のドラッグ&ドロップ操作 / Drag and drop operation carrying the rows being reordered
class FDataAssetSheetRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDataAssetSheetRowDragDropOp, FDecoratedDragDropOp)

	// ドラッグ中の行データ / Rows being dragged
	TArray<TSharedPtr<FDataAssetRowData>> DraggedRows;

	static TSharedRef<FDataAssetSheetRowDragDropOp> New(TArray<TSharedPtr<FDataAssetRowData>> InRows);
};

// テーブル行ウィジェット / Table row widget for SDataAssetSheetEditor
class SDataAssetSheetRow : public SMultiColumnTableRow<TSharedPtr<FDataAssetRowData>>
{
public:
	SLATE_BEGIN_ARGS(SDataAssetSheetRow)
		: _IndexInList(0)
	{}
		SLATE_ARGUMENT(int32, IndexInList)
		SLATE_ARGUMENT(TWeakObjectPtr<UDataAssetSheet>, Sheet)
		SLATE_EVENT(FOnReplaceRowAsset, OnReplaceRowAsset)
		SLATE_EVENT(FOnDeleteRow, OnDeleteRow)
		SLATE_EVENT(FOnReorderRows, OnReorderRows)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable,
		TSharedPtr<FDataAssetRowData> InRowData, TSharedPtr<FDataAssetSheetModel> InModel,
		TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> InListView,
		TSharedPtr<FAssetThumbnailPool> InThumbnailPool);

	virtual const FSlateBrush* GetBorder() const override;

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

	// インライン編集モード開始 / Enter inline edit mode for given column
	void EnterEditMode(FName ColumnId);

	// インライン編集モード終了 / Exit inline edit mode
	void ExitEditMode();

private:
	FSlateColor GetRowTextColor() const;
	TSharedRef<SWidget> GenerateCellContent(const FName& ColumnId);

	// アセット名セルを構築 / Build the AssetName cell (picker when editable, read-only otherwise)
	TSharedRef<SWidget> GenerateAssetNameCell();

	// この行がピッカーで編集可能か（ManualAssets由来かつ非bShowAll）/ Whether this row's asset can be swapped via the picker
	bool IsRowEditable() const;

	// コンテンツブラウザでこの行のアセットを表示 / Sync to this row's asset in the Content Browser
	FReply OnBrowseToAssetClicked();

	// この行を ManualAssets から削除 / Delete this row from ManualAssets (inline button)
	FReply OnDeleteRowClicked();

	// ドラッグ&ドロップによる並び替え / Drag-and-drop row reorder handlers
	FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDataAssetRowData> TargetItem);
	FReply HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDataAssetRowData> TargetItem);

	// この行がドラッグ並び替え可能か（手動行かつ未ソート）/ Whether this row can participate in drag reorder
	bool CanReorder() const;

	// プロパティ値をコミット / Commit property value from inline edit
	void CommitPropertyEdit(FProperty* Prop, const FString& NewValue);

	TSharedPtr<FDataAssetRowData> RowData;
	TSharedPtr<FDataAssetSheetModel> Model;
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> OwnerListView;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TWeakObjectPtr<UDataAssetSheet> WeakSheet;
	FOnReplaceRowAsset OnReplaceRowAsset;
	FOnDeleteRow OnDeleteRow;
	FOnReorderRows OnReorderRows;
	int32 IndexInList = 0;

	// 編集中のカラムID / Column currently in edit mode (NAME_None = not editing)
	FName EditingColumnId;

	// カラム→SWidgetSwitcher マップ / Map of column ID to widget switcher for inline edit
	TMap<FName, TSharedPtr<SWidgetSwitcher>> CellSwitchers;
};

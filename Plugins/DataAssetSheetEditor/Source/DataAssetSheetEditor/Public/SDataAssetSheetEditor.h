// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Dom/JsonObject.h"

class UDataAssetSheet;
class FDataAssetSheetModel;
struct FDataAssetRowData;
class IDetailsView;

/**
 * メインスプレッドシートエディタウィジェット / Main spreadsheet editor widget
 * 左側にテーブル（読み取り専用）、右側に詳細パネル（編集用）を配置
 */
class SDataAssetSheetEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDataAssetSheetEditor)
		: _DataAssetSheet(nullptr)
	{}
		SLATE_ARGUMENT(UDataAssetSheet*, DataAssetSheet)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SDataAssetSheetEditor();

	// タブ用ウィジェット取得 / Get widgets for separate tabs
	TSharedRef<SWidget> GetTableWidget() const;
	TSharedRef<SWidget> GetDetailsWidget() const;

	// 登録変更時にテーブルを再構築 / Rebuild table when registration changes
	void OnSettingsChanged();

	// コマンドリスト設定（Toolkitから呼ばれる）/ Set command list from toolkit
	void BindCommands(const TSharedRef<FUICommandList>& InCommandList);

	// ツールバーから呼ばれるアクション / Actions called from toolkit toolbar
	void CreateNewAsset();
	void SaveAllModifiedAssets();
	bool HasModifiedAssets() const;

private:
	// テーブル構築 / Build table from model data
	void RebuildTable();

	// SHeaderRow構築 / Build header row from column properties
	void RebuildHeaderRow();

	// 行生成コールバック / Row generation callback for SListView
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDataAssetRowData> InRowData, const TSharedRef<STableViewBase>& OwnerTable);

	// 行選択コールバック / Row selection changed callback
	void OnSelectionChanged(TSharedPtr<FDataAssetRowData> InRowData, ESelectInfo::Type SelectInfo);

	// リフレッシュ / Refresh the asset list
	FReply OnRefreshClicked();

	// 非同期ロード開始 / Start async loading
	void StartAsyncLoad();

	// 非同期ロード完了コールバック / Async load completion callback
	void OnAsyncLoadCompleted();

	// ローディングUI表示制御 / Loading UI visibility
	EVisibility GetLoadingVisibility() const;
	EVisibility GetEmptyMessageVisibility() const;
	FText GetEmptyMessageText() const;

	// 詳細パネルでのプロパティ変更時コールバック / Callback for property changes in details panel
	void OnDetailsPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	// テキストフィルタ変更コールバック / Text filter changed callback
	void OnFilterTextChanged(const FText& InFilterText);

	// CSVエクスポート / CSV export
	FReply OnExportCSVClicked();

	// CSVインポート / CSV import
	FReply OnImportCSVClicked();

	// AssetRegistryイベント / AssetRegistry event handlers
	void RegisterAssetRegistryEvents();
	void UnregisterAssetRegistryEvents();
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	// 対象クラスに該当するアセットか判定 / Check if asset belongs to the target class
	bool IsTargetAsset(const FAssetData& AssetData) const;

	// カラムソート / Column sort callbacks
	void OnSortModeChanged(EColumnSortPriority::Type SortPriority, const FName& ColumnId, EColumnSortMode::Type SortMode);
	EColumnSortMode::Type GetSortModeForColumn(FName ColumnId) const;

	// Hot Reload完了時のコールバック / Hot reload completion callback
	void OnReloadComplete(EReloadCompleteReason Reason);

	// コンテキストメニュー構築 / Build context menu for selected rows
	TSharedPtr<SWidget> OnConstructContextMenu();

	// Browse to Asset / Show asset in Content Browser
	void BrowseToSelectedAsset();
	bool HasSelectedLoadedAsset() const;

	// アセット削除 / Delete selected asset (single only)
	void DeleteSelectedAsset();
	bool CanDeleteSelectedAsset() const;

	// アセット複製 / Duplicate selected asset
	void DuplicateSelectedAsset();

	// Find References / Open reference viewer for selected asset
	void FindReferencesForSelectedAsset();

	// Copy/Paste / Clipboard operations for row data
	void CopySelectedRows();
	void PasteOnSelectedRows();
	bool CanPaste() const;

	// ドラッグ&ドロップ / Drag and drop from Content Browser
	FReply HandleDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	FReply HandleDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	// レイアウトデータ / Layout persistence (column widths, hidden columns)
	void LoadLayoutData();
	void SaveLayoutData();

	// カラム表示/非表示 / Column visibility
	TSharedPtr<SWidget> OnConstructHeaderContextMenu();
	void ToggleColumnVisibility(FName ColumnId);
	bool IsColumnVisible(FName ColumnId) const;

	// データモデル / Data model
	TSharedPtr<FDataAssetSheetModel> Model;

	// 編集中のアセット / The DataAssetSheet being viewed
	TWeakObjectPtr<UDataAssetSheet> DataAssetSheet;

	// UIコンポーネント / UI components
	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> AssetListView;
	TSharedPtr<IDetailsView> DetailsView;

	// タブ用ウィジェット / Widgets for separate tabs
	TSharedPtr<SWidget> TableWidget;
	TSharedPtr<SWidget> DetailsWidget;

	// コマンドリスト / Command list for keyboard shortcuts and context menu
	TSharedPtr<FUICommandList> CommandList;

	// レイアウトデータ / Layout data (column widths, hidden columns)
	TSharedPtr<FJsonObject> LayoutData;
	TSet<FName> HiddenColumns;
};

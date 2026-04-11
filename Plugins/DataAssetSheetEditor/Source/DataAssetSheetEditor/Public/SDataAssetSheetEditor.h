// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"

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
};

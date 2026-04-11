// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

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
	EVisibility GetTableVisibility() const;
	EVisibility GetEmptyMessageVisibility() const;

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

	// データモデル / Data model
	TSharedPtr<FDataAssetSheetModel> Model;

	// 編集中のアセット / The DataAssetSheet being viewed
	TWeakObjectPtr<UDataAssetSheet> DataAssetSheet;

	// UIコンポーネント / UI components
	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> AssetListView;
	TSharedPtr<IDetailsView> DetailsView;
};

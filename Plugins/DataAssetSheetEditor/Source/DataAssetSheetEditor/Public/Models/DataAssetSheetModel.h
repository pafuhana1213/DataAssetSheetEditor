// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StreamableManager.h"
#include "Engine/EngineTypes.h"
#include "Widgets/Views/SHeaderRow.h"

// ロード状態 / Asset loading state
enum class EDataAssetSheetLoadingState : uint8
{
	NotStarted,
	Loading,
	Loaded,
};

// 行データ / Row data representing one DataAsset instance
struct FDataAssetRowData
{
	// アセットパス（ロード前でも利用可能）/ Asset path, available before loading
	FSoftObjectPath AssetPath;

	// アセット名（表示用、ロード前でも利用可能）/ Display name, available before loading
	FString AssetName;

	// ロード済みアセット参照 / Loaded asset reference (valid after async load completes)
	TWeakObjectPtr<UDataAsset> Asset;

	// アセットのクラス（ロード前でもアセットレジストリから取得可能）
	// Asset class resolved from AssetRegistry (available before the asset itself is loaded)
	TWeakObjectPtr<UClass> AssetClass;

	// プロパティ表示文字列キャッシュ（フィルタ/ソート高速化用）
	// Cached display text per property column, keyed by FProperty::GetFName()
	TMap<FName, FString> CachedDisplayText;

	bool IsLoaded() const { return Asset.IsValid(); }
};

// 非同期ロード完了デリゲート / Delegate for async load completion
DECLARE_DELEGATE(FOnAssetsLoaded);

/**
 * データモデル / Data model for the DataAsset spreadsheet editor
 * アセット検索、非同期ロード、プロパティ列抽出を担当する
 */
class FDataAssetSheetModel
{
public:
	FDataAssetSheetModel();
	~FDataAssetSheetModel();

	// アセット検索（登録ベース）/ Discover assets based on registration settings
	void DiscoverAssets(UClass* InTargetClass, bool bShowAll,
		const TArray<TSoftObjectPtr<UDataAsset>>& ManualAssets,
		const TArray<FCollectionReference>& Collections);

	// 非同期ロード開始 / Start async loading of discovered assets
	void RequestAsyncLoad(FOnAssetsLoaded OnCompleted);

	// ロード中断 / Cancel ongoing async load
	void CancelLoading();

	// プロパティ列リスト構築 / Build column list from target class properties
	void BuildColumnList(UClass* InTargetClass);

	// プロパティ値をテキストとして取得 / Get property value as display text
	FString GetPropertyValueText(UDataAsset* InAsset, FProperty* InProperty) const;

	// 行の表示文字列キャッシュを再構築 / Rebuild cached display text for a single row
	void RebuildRowCache(const TSharedPtr<FDataAssetRowData>& RowData) const;

	// 行の特定プロパティだけキャッシュを更新 / Rebuild cached display text for a single property on a row
	void RebuildRowCacheForProperty(const TSharedPtr<FDataAssetRowData>& RowData, FProperty* InProperty) const;

	// 全行の表示文字列キャッシュを再構築 / Rebuild cached display text for every row
	void RebuildAllRowCaches() const;

	// アセットが指定プロパティを所有するクラスか判定 / Check if asset's class owns the given property
	bool AssetHasProperty(UDataAsset* InAsset, FProperty* InProperty) const;

	// クラスが指定プロパティを所有するか判定（ロード不要）/ Class-based check (no asset load required)
	bool ClassHasProperty(UClass* InClass, FProperty* InProperty) const;

	// フィルタ適用 / Apply text filter to row data
	void ApplyFilter(const FString& InFilterText);

	// カラムソート / Sort filtered rows by column
	void SortByColumn(const FName& ColumnId, EColumnSortMode::Type InSortMode);

	// アクセサ / Accessors
	const TArray<TSharedPtr<FDataAssetRowData>>& GetRowDataList() const { return RowDataList; }
	TArray<TSharedPtr<FDataAssetRowData>>& GetMutableRowDataList() { return RowDataList; }
	const TArray<TSharedPtr<FDataAssetRowData>>& GetFilteredRowDataList() const { return FilteredRowDataList; }
	const TArray<FProperty*>& GetColumnProperties() const { return ColumnProperties; }
	EDataAssetSheetLoadingState GetLoadingState() const { return LoadingState; }
	bool IsFiltered() const { return !FilterText.IsEmpty(); }
	const FString& GetFilterText() const { return FilterText; }
	FName GetSortColumnId() const { return SortColumnId; }
	EColumnSortMode::Type GetSortMode() const { return SortMode; }

	// 現在のフィルタテキストで再フィルタ（ソートも再適用）/ Re-apply current filter and sort
	void ReapplyFilter() { ApplyFilter(FilterText); }

private:
	// 行データの追加ヘルパー（重複排除付き）/ Add row data with deduplication
	void AddRowDataFromAssetData(const FAssetData& AssetData, TSet<FSoftObjectPath>& AddedPaths);

	// 行データリスト / Row data list (one per discovered asset)
	TArray<TSharedPtr<FDataAssetRowData>> RowDataList;

	// 表示プロパティ列リスト / Property columns to display
	TArray<FProperty*> ColumnProperties;

	// フィルタ済み行データ / Filtered row data list
	TArray<TSharedPtr<FDataAssetRowData>> FilteredRowDataList;

	// 現在のフィルタテキスト / Current filter text
	FString FilterText;

	// ロード状態 / Current loading state
	EDataAssetSheetLoadingState LoadingState = EDataAssetSheetLoadingState::NotStarted;

	// ソート状態 / Current sort state
	FName SortColumnId;
	EColumnSortMode::Type SortMode = EColumnSortMode::None;

	// 非同期ロード管理 / Async load management
	FStreamableManager StreamableManager;
	TSharedPtr<FStreamableHandle> StreamableHandle;
};

// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StreamableManager.h"
#include "Engine/EngineTypes.h"

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

	// フィルタ適用 / Apply text filter to row data
	void ApplyFilter(const FString& InFilterText);

	// アクセサ / Accessors
	const TArray<TSharedPtr<FDataAssetRowData>>& GetRowDataList() const { return RowDataList; }
	TArray<TSharedPtr<FDataAssetRowData>>& GetMutableRowDataList() { return RowDataList; }
	const TArray<TSharedPtr<FDataAssetRowData>>& GetFilteredRowDataList() const { return FilteredRowDataList; }
	const TArray<FProperty*>& GetColumnProperties() const { return ColumnProperties; }
	EDataAssetSheetLoadingState GetLoadingState() const { return LoadingState; }
	bool IsFiltered() const { return !FilterText.IsEmpty(); }

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

	// 非同期ロード管理 / Async load management
	FStreamableManager StreamableManager;
	TSharedPtr<FStreamableHandle> StreamableHandle;
};

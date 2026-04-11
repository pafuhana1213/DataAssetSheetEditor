// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetModel.h"
#include "DataAssetSheetEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/FieldIterator.h"

FDataAssetSheetModel::FDataAssetSheetModel()
{
}

FDataAssetSheetModel::~FDataAssetSheetModel()
{
	CancelLoading();
}

void FDataAssetSheetModel::DiscoverAssets(UClass* InTargetClass)
{
	CancelLoading();
	RowDataList.Empty();
	LoadingState = EDataAssetSheetLoadingState::NotStarted;

	if (!InTargetClass)
	{
		return;
	}

	// AssetRegistryからアセットを検索（ロードなし）/ Discover assets from registry without loading
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(InTargetClass->GetClassPathName(), AssetDataList, true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FDataAssetRowData> RowData = MakeShared<FDataAssetRowData>();
		RowData->AssetPath = AssetData.GetSoftObjectPath();
		RowData->AssetName = AssetData.AssetName.ToString();
		RowDataList.Add(RowData);
	}

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Discovered %d assets of class %s"), RowDataList.Num(), *InTargetClass->GetName());
}

void FDataAssetSheetModel::RequestAsyncLoad(FOnAssetsLoaded OnCompleted)
{
	if (RowDataList.IsEmpty())
	{
		LoadingState = EDataAssetSheetLoadingState::Loaded;
		OnCompleted.ExecuteIfBound();
		return;
	}

	CancelLoading();
	LoadingState = EDataAssetSheetLoadingState::Loading;

	// ロード対象パスを収集 / Collect paths to load
	TArray<FSoftObjectPath> PathsToLoad;
	for (const TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
	{
		PathsToLoad.Add(RowData->AssetPath);
	}

	// 非同期ロード開始 / Start async load
	StreamableHandle = StreamableManager.RequestAsyncLoad(
		PathsToLoad,
		FStreamableDelegate::CreateLambda([this, OnCompleted]()
		{
			// ロード完了：RowDataにアセット参照をセット / Load complete: set asset references
			for (TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
			{
				UObject* LoadedObject = RowData->AssetPath.ResolveObject();
				if (UDataAsset* DataAsset = Cast<UDataAsset>(LoadedObject))
				{
					RowData->Asset = DataAsset;
				}
			}

			LoadingState = EDataAssetSheetLoadingState::Loaded;
			UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Async load completed for %d assets"), RowDataList.Num());
			OnCompleted.ExecuteIfBound();
		})
	);
}

void FDataAssetSheetModel::CancelLoading()
{
	if (StreamableHandle.IsValid() && StreamableHandle->IsActive())
	{
		StreamableHandle->CancelHandle();
		StreamableHandle.Reset();
	}

	if (LoadingState == EDataAssetSheetLoadingState::Loading)
	{
		LoadingState = EDataAssetSheetLoadingState::NotStarted;
	}
}

void FDataAssetSheetModel::BuildColumnList(UClass* InTargetClass)
{
	ColumnProperties.Empty();

	if (!InTargetClass)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(InTargetClass); It; ++It)
	{
		FProperty* Prop = *It;

		// UDataAsset/UObjectのプロパティはスキップ / Skip properties from UDataAsset and parent classes
		UClass* OwnerClass = Prop->GetOwnerClass();
		if (OwnerClass == UDataAsset::StaticClass() || OwnerClass == UObject::StaticClass())
		{
			continue;
		}

		// EditAnywhereのプロパティのみ表示 / Only show editable properties
		if (!Prop->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		ColumnProperties.Add(Prop);
	}

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Built %d columns for class %s"), ColumnProperties.Num(), *InTargetClass->GetName());
}

FString FDataAssetSheetModel::GetPropertyValueText(UDataAsset* InAsset, FProperty* InProperty) const
{
	if (!InAsset || !InProperty)
	{
		return FString();
	}

	const void* ValuePtr = InProperty->ContainerPtrToValuePtr<void>(InAsset);

	// FTextはToString()で人間が読める形式にする / Use ToString() for FText to get human-readable format
	if (const FTextProperty* TextProp = CastField<FTextProperty>(InProperty))
	{
		const FText& TextValue = TextProp->GetPropertyValue(ValuePtr);
		return TextValue.ToString();
	}

	// その他のプロパティはExportTextで文字列化 / Use ExportText for other property types
	FString ValueString;
	InProperty->ExportText_Direct(ValueString, ValuePtr, ValuePtr, nullptr, PPF_None);
	return ValueString;
}

void FDataAssetSheetModel::ApplyFilter(const FString& InFilterText)
{
	FilterText = InFilterText;
	FilteredRowDataList.Empty();

	if (FilterText.IsEmpty())
	{
		// フィルタなし：全行を表示 / No filter: show all rows
		FilteredRowDataList = RowDataList;
		return;
	}

	// アセット名 + 全プロパティ値テキストに対して部分一致検索 / Partial match against name and all property values
	for (const TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
	{
		// アセット名チェック / Check asset name
		if (RowData->AssetName.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			FilteredRowDataList.Add(RowData);
			continue;
		}

		// プロパティ値チェック / Check property values
		if (RowData->IsLoaded())
		{
			bool bFound = false;
			for (FProperty* Prop : ColumnProperties)
			{
				FString ValueText = GetPropertyValueText(RowData->Asset.Get(), Prop);
				if (ValueText.Contains(FilterText, ESearchCase::IgnoreCase))
				{
					bFound = true;
					break;
				}
			}
			if (bFound)
			{
				FilteredRowDataList.Add(RowData);
			}
		}
	}
}

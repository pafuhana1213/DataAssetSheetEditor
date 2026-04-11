// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetModel.h"
#include "DataAssetSheetEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/FieldIterator.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "ICollectionContainer.h"
#include "UObject/UnrealType.h"

FDataAssetSheetModel::FDataAssetSheetModel()
{
}

FDataAssetSheetModel::~FDataAssetSheetModel()
{
	CancelLoading();
}

void FDataAssetSheetModel::AddRowDataFromAssetData(const FAssetData& AssetData, TSet<FSoftObjectPath>& AddedPaths)
{
	FSoftObjectPath Path = AssetData.GetSoftObjectPath();
	if (AddedPaths.Contains(Path))
	{
		return;
	}

	AddedPaths.Add(Path);

	TSharedPtr<FDataAssetRowData> RowData = MakeShared<FDataAssetRowData>();
	RowData->AssetPath = Path;
	RowData->AssetName = AssetData.AssetName.ToString();
	RowDataList.Add(RowData);
}

void FDataAssetSheetModel::DiscoverAssets(UClass* InTargetClass, bool bShowAll,
	const TArray<TSoftObjectPtr<UDataAsset>>& ManualAssets,
	const TArray<FCollectionReference>& Collections)
{
	CancelLoading();
	RowDataList.Empty();
	LoadingState = EDataAssetSheetLoadingState::NotStarted;

	if (!InTargetClass)
	{
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TSet<FSoftObjectPath> AddedPaths;

	// 1. bShowAll=true の場合、全アセットを検索 / If bShowAll, discover all assets via AssetRegistry
	if (bShowAll)
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(InTargetClass->GetClassPathName(), AssetDataList, true);
		for (const FAssetData& AssetData : AssetDataList)
		{
			AddRowDataFromAssetData(AssetData, AddedPaths);
		}
	}

	// 2. 手動登録アセットを追加 / Add manually registered assets
	for (const TSoftObjectPtr<UDataAsset>& SoftPtr : ManualAssets)
	{
		FSoftObjectPath Path = SoftPtr.ToSoftObjectPath();
		if (Path.IsNull() || AddedPaths.Contains(Path))
		{
			continue;
		}

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(Path);
		if (AssetData.IsValid())
		{
			AddRowDataFromAssetData(AssetData, AddedPaths);
		}
	}

	// 3. コレクション経由のアセットを追加 / Add assets from collections
	if (!Collections.IsEmpty() && FModuleManager::Get().IsModuleLoaded("CollectionManager"))
	{
		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		const TSharedRef<ICollectionContainer>& Container = CollectionManager.GetProjectCollectionContainer();

		for (const FCollectionReference& Collection : Collections)
		{
			if (Collection.CollectionName.IsNone())
			{
				continue;
			}

			// コレクション名からShareTypeを解決 / Resolve ShareType from collection name
			TArray<FCollectionNameType> FoundCollections;
			Container->GetCollections(Collection.CollectionName, FoundCollections);

			for (const FCollectionNameType& Found : FoundCollections)
			{
				TArray<FSoftObjectPath> CollectionAssets;
				Container->GetAssetsInCollection(Found.Name, Found.Type, CollectionAssets);

				for (const FSoftObjectPath& AssetPath : CollectionAssets)
				{
					if (AddedPaths.Contains(AssetPath))
					{
						continue;
					}

					// TargetClassとの一致を検証 / Validate against TargetClass
					FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(AssetPath);
					if (AssetData.IsValid())
					{
						UClass* AssetClass = AssetData.GetClass();
						if (AssetClass && AssetClass->IsChildOf(InTargetClass))
						{
							AddRowDataFromAssetData(AssetData, AddedPaths);
						}
					}
				}
			}
		}
	}

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Discovered %d assets of class %s (ShowAll=%d, ManualAssets=%d, Collections=%d)"),
		RowDataList.Num(), *InTargetClass->GetName(), bShowAll, ManualAssets.Num(), Collections.Num());
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

bool FDataAssetSheetModel::AssetHasProperty(UDataAsset* InAsset, FProperty* InProperty) const
{
	if (!InAsset || !InProperty)
	{
		return false;
	}

	UClass* OwnerClass = InProperty->GetOwnerClass();
	return OwnerClass && InAsset->GetClass()->IsChildOf(OwnerClass);
}

FString FDataAssetSheetModel::GetPropertyValueText(UDataAsset* InAsset, FProperty* InProperty) const
{
	if (!InAsset || !InProperty)
	{
		return FString();
	}

	// アセットのクラスがプロパティを所有するクラスの派生でない場合は空文字を返す
	// Avoid invalid memory access when asset doesn't have this property
	if (!AssetHasProperty(InAsset, InProperty))
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

		// ソート状態があれば再適用 / Re-apply sort if active
		if (SortMode != EColumnSortMode::None)
		{
			SortByColumn(SortColumnId, SortMode);
		}
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

	// ソート状態があれば再適用 / Re-apply sort if active
	if (SortMode != EColumnSortMode::None)
	{
		SortByColumn(SortColumnId, SortMode);
	}
}

void FDataAssetSheetModel::SortByColumn(const FName& ColumnId, EColumnSortMode::Type InSortMode)
{
	SortColumnId = ColumnId;
	SortMode = InSortMode;

	if (SortMode == EColumnSortMode::None)
	{
		return;
	}

	// ソート対象プロパティを検索 / Find property for sorting
	FProperty* SortProp = nullptr;
	if (ColumnId != "AssetName")
	{
		for (FProperty* ColProp : ColumnProperties)
		{
			if (ColProp->GetFName() == ColumnId)
			{
				SortProp = ColProp;
				break;
			}
		}
	}

	// 数値プロパティか判定 / Check if numeric property
	bool bIsNumeric = SortProp && SortProp->IsA<FNumericProperty>();

	FilteredRowDataList.Sort([this, ColumnId, InSortMode, SortProp, bIsNumeric](
		const TSharedPtr<FDataAssetRowData>& A, const TSharedPtr<FDataAssetRowData>& B)
	{
		if (ColumnId == "AssetName")
		{
			int32 Result = A->AssetName.Compare(B->AssetName, ESearchCase::IgnoreCase);
			return (InSortMode == EColumnSortMode::Ascending) ? (Result < 0) : (Result > 0);
		}

		if (!SortProp)
		{
			return false;
		}

		FString ValueA, ValueB;
		if (A->IsLoaded()) ValueA = GetPropertyValueText(A->Asset.Get(), SortProp);
		if (B->IsLoaded()) ValueB = GetPropertyValueText(B->Asset.Get(), SortProp);

		// 数値型は数値として比較 / Compare numerics by value
		if (bIsNumeric)
		{
			double NumA = FCString::Atod(*ValueA);
			double NumB = FCString::Atod(*ValueB);
			if (NumA != NumB)
			{
				return (InSortMode == EColumnSortMode::Ascending) ? (NumA < NumB) : (NumA > NumB);
			}
			return false;
		}

		int32 Result = ValueA.Compare(ValueB, ESearchCase::IgnoreCase);
		return (InSortMode == EColumnSortMode::Ascending) ? (Result < 0) : (Result > 0);
	});
}

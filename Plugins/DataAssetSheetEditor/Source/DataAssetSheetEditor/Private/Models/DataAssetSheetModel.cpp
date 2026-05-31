// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetModel.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/FieldIterator.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "ICollectionContainer.h"
#include "UObject/UnrealType.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FDataAssetSheetModel"

DECLARE_CYCLE_STAT(TEXT("ApplyFilter"), STAT_DataAssetSheet_ApplyFilter, STATGROUP_DataAssetSheet);
DECLARE_CYCLE_STAT(TEXT("SortByColumn"), STAT_DataAssetSheet_SortByColumn, STATGROUP_DataAssetSheet);
DECLARE_CYCLE_STAT(TEXT("GetPropertyValueText"), STAT_DataAssetSheet_GetPropertyValueText, STATGROUP_DataAssetSheet);

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

	// クラス情報をレジストリから取得（アセット本体はロードせずクラスのみ解決）
	// Resolve asset class from AssetRegistry without loading the asset itself
	if (UClass* ResolvedClass = AssetData.GetClass())
	{
		RowData->AssetClass = ResolvedClass;
	}

	RowDataList.Add(RowData);
}

void FDataAssetSheetModel::DiscoverAssets(UClass* InTargetClass, bool bShowAll,
	const TArray<TSoftObjectPtr<UDataAsset>>& ManualAssets,
	const TArray<FCollectionReference>& Collections)
{
	CancelLoading();
	RowDataList.Empty();
	LoadingState = EDataAssetSheetLoadingState::NotStarted;

	if (!UDataAssetSheet::IsSupportedDataAssetClass(InTargetClass))
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
	// ManualAssets のインデックスを行データに記録し、並び替え/削除で空行も含め一意に識別できるようにする
	// Record each row's ManualAssets index so reorder/delete can identify rows uniquely, empty rows included.
	for (int32 ManualIndex = 0; ManualIndex < ManualAssets.Num(); ++ManualIndex)
	{
		const TSoftObjectPtr<UDataAsset>& SoftPtr = ManualAssets[ManualIndex];
		FSoftObjectPath Path = SoftPtr.ToSoftObjectPath();
		if (Path.IsNull())
		{
			// 空行（アセット未割り当て）。重複排除せず、各エントリを1行として表示する
			// Empty row (no asset assigned). Not deduped, each entry becomes its own row.
			TSharedPtr<FDataAssetRowData> EmptyRow = MakeShared<FDataAssetRowData>();
			EmptyRow->AssetPath = FSoftObjectPath();
			EmptyRow->AssetName.Reset();
			EmptyRow->AssetClass = InTargetClass;
			EmptyRow->ManualAssetIndex = ManualIndex;
			RowDataList.Add(EmptyRow);
			continue;
		}

		if (AddedPaths.Contains(Path))
		{
			continue;
		}

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(Path);
		if (AssetData.IsValid())
		{
			AddRowDataFromAssetData(AssetData, AddedPaths);
			RowDataList.Last()->ManualAssetIndex = ManualIndex;
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

	// ロード対象パスを収集（空行はアセット未割り当てなのでスキップ）/ Collect paths to load (skip empty rows: no asset assigned)
	TArray<FSoftObjectPath> PathsToLoad;
	for (const TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
	{
		if (RowData->AssetPath.IsNull())
		{
			continue;
		}
		PathsToLoad.Add(RowData->AssetPath);
	}

	// ロード対象が無い（全行が空行）場合は即完了 / Nothing to load (all rows empty): complete immediately
	if (PathsToLoad.IsEmpty())
	{
		RebuildAllRowCaches();
		LoadingState = EDataAssetSheetLoadingState::Loaded;
		OnCompleted.ExecuteIfBound();
		return;
	}

	// 非同期ロード開始 / Start async load
	StreamableHandle = StreamableManager.RequestAsyncLoad(
		PathsToLoad,
		FStreamableDelegate::CreateLambda([this, OnCompleted]()
		{
			// ロード完了：RowDataにアセット参照をセット / Load complete: set asset references
			int32 FailedCount = 0;
			for (TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
			{
				// 空行（アセット未割り当て）はロード対象外なので失敗扱いしない / Empty rows have no asset; not a load failure
				if (RowData->AssetPath.IsNull())
				{
					continue;
				}

				UObject* LoadedObject = RowData->AssetPath.ResolveObject();
				if (UDataAsset* DataAsset = Cast<UDataAsset>(LoadedObject))
				{
					RowData->Asset = DataAsset;
				}
				else
				{
					++FailedCount;
				}
			}

			LoadingState = EDataAssetSheetLoadingState::Loaded;

			// ロード完了直後に表示文字列キャッシュを構築 / Build display text cache once assets are loaded
			RebuildAllRowCaches();

			UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Async load completed for %d assets"), RowDataList.Num());

			// ロードに失敗したアセットがあればユーザーに通知 / Notify user about failed assets
			if (FailedCount > 0)
			{
				UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Failed to resolve %d asset(s) after async load"), FailedCount);
				FNotificationInfo Info(FText::Format(
					LOCTEXT("AsyncLoadFailed", "{0} asset(s) failed to load"),
					FText::AsNumber(FailedCount)));
				Info.ExpireDuration = 5.0f;
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if (Notification.IsValid())
				{
					Notification->SetCompletionState(SNotificationItem::CS_Fail);
				}
			}

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

	if (!UDataAssetSheet::IsSupportedDataAssetClass(InTargetClass))
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
	return InAsset && ClassHasProperty(InAsset->GetClass(), InProperty);
}

bool FDataAssetSheetModel::ClassHasProperty(UClass* InClass, FProperty* InProperty) const
{
	if (!InClass || !InProperty)
	{
		return false;
	}

	UClass* OwnerClass = InProperty->GetOwnerClass();
	return OwnerClass && InClass->IsChildOf(OwnerClass);
}

FString FDataAssetSheetModel::GetPropertyValueText(UDataAsset* InAsset, FProperty* InProperty) const
{
	SCOPE_CYCLE_COUNTER(STAT_DataAssetSheet_GetPropertyValueText);

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

bool FDataAssetSheetModel::SetPropertyValueFromString(const TSharedPtr<FDataAssetRowData>& RowData,
	FProperty* InProperty, const FString& InValue, FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!RowData.IsValid() || !RowData->IsLoaded() || InProperty == nullptr)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("row is not loaded or property is invalid");
		}
		return false;
	}

	UDataAsset* TargetAsset = RowData->Asset.Get();
	if (!TargetAsset || !AssetHasProperty(TargetAsset, InProperty))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("asset does not own the property");
		}
		return false;
	}

	void* ValuePtr = InProperty->ContainerPtrToValuePtr<void>(TargetAsset);
	uint8* TempValue = static_cast<uint8*>(FMemory_Alloca(InProperty->GetSize()));
	InProperty->InitializeValue(TempValue);
	InProperty->CopyCompleteValue(TempValue, ValuePtr);

	bool bParsed = true;
	if (FTextProperty* TextProp = CastField<FTextProperty>(InProperty))
	{
		TextProp->SetPropertyValue(TempValue, FText::FromString(InValue));
	}
	else
	{
		const TCHAR* ImportResult = InProperty->ImportText_Direct(*InValue, TempValue, TargetAsset, PPF_None);
		bParsed = (ImportResult != nullptr);
	}

	if (!bParsed)
	{
		InProperty->DestroyValue(TempValue);
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("failed to parse value");
		}
		return false;
	}

	if (!InProperty->Identical(ValuePtr, TempValue))
	{
		TargetAsset->Modify();
		TargetAsset->PreEditChange(InProperty);
		InProperty->CopyCompleteValue(ValuePtr, TempValue);

		FPropertyChangedEvent PropertyChangedEvent(InProperty, EPropertyChangeType::ValueSet);
		TargetAsset->PostEditChangeProperty(PropertyChangedEvent);
		TargetAsset->MarkPackageDirty();
	}

	InProperty->DestroyValue(TempValue);
	RebuildRowCacheForProperty(RowData, InProperty);
	return true;
}

void FDataAssetSheetModel::RebuildRowCache(const TSharedPtr<FDataAssetRowData>& RowData) const
{
	if (!RowData.IsValid())
	{
		return;
	}

	RowData->CachedDisplayText.Reset();

	if (!RowData->IsLoaded())
	{
		return;
	}

	UDataAsset* Asset = RowData->Asset.Get();
	if (!Asset)
	{
		return;
	}
	for (FProperty* Prop : ColumnProperties)
	{
		if (!ClassHasProperty(Asset->GetClass(), Prop))
		{
			continue;
		}
		RowData->CachedDisplayText.Add(Prop->GetFName(), GetPropertyValueText(Asset, Prop));
	}
}

void FDataAssetSheetModel::RebuildRowCacheForProperty(const TSharedPtr<FDataAssetRowData>& RowData, FProperty* InProperty) const
{
	if (!RowData.IsValid() || InProperty == nullptr || !RowData->IsLoaded())
	{
		return;
	}

	UDataAsset* Asset = RowData->Asset.Get();
	if (!Asset || !ClassHasProperty(Asset->GetClass(), InProperty))
	{
		return;
	}

	RowData->CachedDisplayText.Add(InProperty->GetFName(), GetPropertyValueText(Asset, InProperty));
}

void FDataAssetSheetModel::RebuildAllRowCaches() const
{
	for (const TSharedPtr<FDataAssetRowData>& RowData : RowDataList)
	{
		RebuildRowCache(RowData);
	}
}

void FDataAssetSheetModel::ApplyFilter(const FString& InFilterText)
{
	SCOPE_CYCLE_COUNTER(STAT_DataAssetSheet_ApplyFilter);

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

		// プロパティ値チェック（キャッシュ参照）/ Check cached property values
		bool bFound = false;
		for (const TPair<FName, FString>& Pair : RowData->CachedDisplayText)
		{
			if (Pair.Value.Contains(FilterText, ESearchCase::IgnoreCase))
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

	// ソート状態があれば再適用 / Re-apply sort if active
	if (SortMode != EColumnSortMode::None)
	{
		SortByColumn(SortColumnId, SortMode);
	}
}

void FDataAssetSheetModel::SortByColumn(const FName& ColumnId, EColumnSortMode::Type InSortMode)
{
	SCOPE_CYCLE_COUNTER(STAT_DataAssetSheet_SortByColumn);

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
	const FName SortPropName = SortProp ? SortProp->GetFName() : NAME_None;

	FilteredRowDataList.Sort([ColumnId, InSortMode, SortProp, bIsNumeric, SortPropName](
		const TSharedPtr<FDataAssetRowData>& A, const TSharedPtr<FDataAssetRowData>& B)
	{
		// 空行（アセット未割り当て）はソート方向に関わらず常に末尾へ / Keep empty rows at the bottom regardless of sort direction
		const bool bAEmpty = A->AssetPath.IsNull();
		const bool bBEmpty = B->AssetPath.IsNull();
		if (bAEmpty != bBEmpty)
		{
			return !bAEmpty;
		}
		if (bAEmpty && bBEmpty)
		{
			return false;
		}

		if (ColumnId == "AssetName")
		{
			int32 Result = A->AssetName.Compare(B->AssetName, ESearchCase::IgnoreCase);
			return (InSortMode == EColumnSortMode::Ascending) ? (Result < 0) : (Result > 0);
		}

		if (!SortProp)
		{
			return false;
		}

		const FString* PtrA = A->CachedDisplayText.Find(SortPropName);
		const FString* PtrB = B->CachedDisplayText.Find(SortPropName);
		const FString ValueA = PtrA ? *PtrA : FString();
		const FString ValueB = PtrB ? *PtrB : FString();

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

#undef LOCTEXT_NAMESPACE

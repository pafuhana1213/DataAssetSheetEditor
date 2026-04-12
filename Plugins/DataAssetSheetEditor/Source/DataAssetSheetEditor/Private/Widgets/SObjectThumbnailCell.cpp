// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SObjectThumbnailCell.h"
#include "DataAssetSheetModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

void SObjectThumbnailCell::Construct(const FArguments& InArgs)
{
	WeakRowData = InArgs._RowData;
	WeakModel = InArgs._Model;
	Property = InArgs._Property;
	ThumbnailPool = InArgs._ThumbnailPool;

	SetCanTick(false);

	ChildSlot
	[
		SAssignNew(ContentBox, SBox)
	];

	RebuildContent(ResolveCurrentAssetPath());

	// 毎フレームではなく 5Hz のアクティブタイマーで差し替えを検知 / Poll at 5Hz instead of per-frame Tick
	RegisterActiveTimer(0.2f, FWidgetActiveTimerDelegate::CreateSP(this, &SObjectThumbnailCell::PollForChange));
}

EActiveTimerReturnType SObjectThumbnailCell::PollForChange(double InCurrentTime, float InDeltaTime)
{
	const FSoftObjectPath CurrentPath = ResolveCurrentAssetPath();
	if (CurrentPath != LastPath)
	{
		RebuildContent(CurrentPath);
	}
	return EActiveTimerReturnType::Continue;
}

FSoftObjectPath SObjectThumbnailCell::ResolveCurrentAssetPath() const
{
	TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
	TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
	if (!PinnedRow.IsValid() || !PinnedModel.IsValid() || !PinnedRow->IsLoaded() || Property == nullptr)
	{
		return FSoftObjectPath();
	}
	if (!PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), Property))
	{
		return FSoftObjectPath();
	}

	if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		UObject* Value = ObjectProp->GetObjectPropertyValue_InContainer(PinnedRow->Asset.Get());
		return Value ? FSoftObjectPath(Value) : FSoftObjectPath();
	}
	if (const FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr* SoftPtr = SoftProp->ContainerPtrToValuePtr<FSoftObjectPtr>(PinnedRow->Asset.Get());
		return SoftPtr ? SoftPtr->ToSoftObjectPath() : FSoftObjectPath();
	}
	return FSoftObjectPath();
}

void SObjectThumbnailCell::RebuildContent(const FSoftObjectPath& NewPath)
{
	LastPath = NewPath;

	if (!NewPath.IsValid())
	{
		Thumbnail.Reset();
		ContentBox->SetContent(
			SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("NullAsset", "-"))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
				]
		);
		SetToolTipText(FText::GetEmpty());
		return;
	}

	// AssetRegistry 経由で FAssetData を解決すれば未ロードのソフト参照でもサムネを出せる
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(NewPath);

	Thumbnail = MakeShared<FAssetThumbnail>(AssetData, 24, 24, ThumbnailPool);

	FAssetThumbnailConfig ThumbConfig;
	ThumbConfig.bAllowFadeIn = true;

	ContentBox->SetContent(
		SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
					.WidthOverride(24.0f)
					.HeightOverride(24.0f)
					[
						Thumbnail->MakeThumbnailWidget(ThumbConfig)
					]
			]
	);
	SetToolTipText(FText::FromString(NewPath.ToString()));
}

#undef LOCTEXT_NAMESPACE

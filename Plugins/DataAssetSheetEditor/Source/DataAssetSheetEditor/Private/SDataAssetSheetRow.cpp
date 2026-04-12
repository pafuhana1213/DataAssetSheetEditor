// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetRow.h"
#include "SObjectThumbnailCell.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetModel.h"
#include "AssetThumbnail.h"
#include "Editor.h"
#include "GameplayTagContainer.h"
#include "Math/ColorList.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

void SDataAssetSheetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable,
	TSharedPtr<FDataAssetRowData> InRowData, TSharedPtr<FDataAssetSheetModel> InModel,
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> InListView,
	TSharedPtr<FAssetThumbnailPool> InThumbnailPool)
{
	RowData = InRowData;
	Model = InModel;
	IndexInList = InArgs._IndexInList;
	OwnerListView = InListView;
	ThumbnailPool = InThumbnailPool;
	SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), InOwnerTable);
}

// 交互背景色 / Alternating row background color
const FSlateBrush* SDataAssetSheetRow::GetBorder() const
{
	static const FSlateColorBrush EvenBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	static const FSlateColorBrush OddBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.03f));
	return (IndexInList % 2 == 0) ? &EvenBrush : &OddBrush;
}

// 選択行のテキスト色 / Text color for selected rows
FSlateColor SDataAssetSheetRow::GetRowTextColor() const
{
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> ListView = OwnerListView;
	if (ListView.IsValid() && RowData.IsValid() && ListView->IsItemSelected(RowData))
	{
		return FSlateColor(FColorList::Orange);
	}
	return FSlateColor::UseForeground();
}

TSharedRef<SWidget> SDataAssetSheetRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	// 全セルを一定の最小高でラップして行高を底上げ (Color/Texture 等のリッチセル用)
	return SNew(SBox)
		.MinDesiredHeight(28.0f)
		.VAlign(VAlign_Center)
		[
			GenerateCellContent(ColumnId)
		];
}

TSharedRef<SWidget> SDataAssetSheetRow::GenerateCellContent(const FName& ColumnId)
{
	// アセット名列（未保存時は * 表示）/ Asset name column with unsaved indicator
	if (ColumnId == "AssetName")
	{
		TWeakPtr<FDataAssetRowData> WeakRowData = RowData;

		return SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHyperlink)
					.Text_Lambda([WeakRowData]() -> FText
					{
						TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
						if (!PinnedRow.IsValid())
						{
							return FText::GetEmpty();
						}
						FString DisplayName = PinnedRow->AssetName;
						if (PinnedRow->IsLoaded())
						{
							UPackage* Package = PinnedRow->Asset->GetOutermost();
							if (Package && Package->IsDirty())
							{
								DisplayName = TEXT("* ") + DisplayName;
							}
						}
						return FText::FromString(DisplayName);
					})
					.OnNavigate(this, &SDataAssetSheetRow::OnAssetNameClicked)
			];
	}

	// プロパティ列 / Property value column
	if (Model.IsValid())
	{
		FProperty* Prop = nullptr;
		for (FProperty* ColProp : Model->GetColumnProperties())
		{
			if (ColProp->GetFName() == ColumnId)
			{
				Prop = ColProp;
				break;
			}
		}

		if (Prop)
		{
			TWeakPtr<FDataAssetRowData> WeakRowData = RowData;
			TWeakPtr<FDataAssetSheetModel> WeakModel = Model;
			FProperty* CapturedProp = Prop;

			// アセットがこのプロパティを持たない場合は黒塗りセルを表示
			// Show blacked-out cell when asset doesn't own this property (base class row for derived-class column)
			// 非同期ロード完了前でもレジストリから解決したクラスで判定できるようにする
			UClass* RowClass = RowData->Asset.IsValid()
				? RowData->Asset->GetClass()
				: RowData->AssetClass.Get();
			if (RowClass && !Model->ClassHasProperty(RowClass, Prop))
			{
				static const FSlateColorBrush BlackBrush(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
				return SNew(SBox)
					.Padding(FMargin(0.0f))
					[
						SNew(SBorder)
							.BorderImage(&BlackBrush)
							.Padding(FMargin(4.0f, 2.0f))
							[
								SNew(STextBlock)
									.Text(LOCTEXT("N/A", "-"))
									.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f)))
							]
					];
			}

			// Bool値はチェックボックスで表示（読み取り専用）/ Display bool as read-only checkbox
			if (CastField<FBoolProperty>(Prop))
			{
				return SNew(SBox)
					.Padding(FMargin(4.0f, 2.0f))
					[
						SNew(SCheckBox)
							.IsChecked_Lambda([WeakRowData, WeakModel, CapturedProp]() -> ECheckBoxState
							{
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
								if (PinnedRow.IsValid() && PinnedModel.IsValid() && PinnedRow->IsLoaded()
									&& PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
								{
									const FBoolProperty* BoolProp = CastField<FBoolProperty>(CapturedProp);
									const void* ValuePtr = BoolProp->ContainerPtrToValuePtr<void>(PinnedRow->Asset.Get());
									return BoolProp->GetPropertyValue(ValuePtr)
										? ECheckBoxState::Checked
										: ECheckBoxState::Unchecked;
								}
								return ECheckBoxState::Unchecked;
							})
							.IsEnabled(false)
					];
			}

			// Color/LinearColor: 表示専用の横長カラーバー / Display-only color swatch
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				const bool bIsLinearColor = StructProp->Struct == TBaseStructure<FLinearColor>::Get();
				const bool bIsColor = StructProp->Struct == TBaseStructure<FColor>::Get();
				if (bIsLinearColor || bIsColor)
				{
					return SNew(SBox)
						.Padding(FMargin(4.0f, 4.0f))
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						[
							SNew(SColorBlock)
								.Color_Lambda([WeakRowData, WeakModel, CapturedProp, bIsLinearColor]() -> FLinearColor
								{
									TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
									TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
									if (PinnedRow.IsValid() && PinnedModel.IsValid() && PinnedRow->IsLoaded()
										&& PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
									{
										const FStructProperty* Sp = CastField<FStructProperty>(CapturedProp);
										const void* ValuePtr = Sp->ContainerPtrToValuePtr<void>(PinnedRow->Asset.Get());
										if (bIsLinearColor)
										{
											return *static_cast<const FLinearColor*>(ValuePtr);
										}
										return static_cast<const FColor*>(ValuePtr)->ReinterpretAsLinear();
									}
									return FLinearColor::Transparent;
								})
								.Size(FVector2D(60.0f, 18.0f))
								.ShowBackgroundForAlpha(true)
						];
				}

				// GameplayTag: タグ名のみをテキスト表示 / Show tag name only, empty for None
				if (StructProp->Struct == FGameplayTag::StaticStruct())
				{
					return SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text_Lambda([WeakRowData, WeakModel, CapturedProp]() -> FText
								{
									TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
									TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
									if (!PinnedRow.IsValid() || !PinnedModel.IsValid() || !PinnedRow->IsLoaded()
										|| !PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
									{
										return FText::GetEmpty();
									}
									const FStructProperty* Sp = CastField<FStructProperty>(CapturedProp);
									const FGameplayTag* TagPtr = Sp->ContainerPtrToValuePtr<FGameplayTag>(PinnedRow->Asset.Get());
									if (!TagPtr || !TagPtr->IsValid())
									{
										return FText::GetEmpty();
									}
									return FText::FromName(TagPtr->GetTagName());
								})
								.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor)
						];
				}

				// GameplayTagContainer: 含まれるタグを改行区切りで表示 / Newline-joined tag names
				if (StructProp->Struct == FGameplayTagContainer::StaticStruct())
				{
					return SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text_Lambda([WeakRowData, WeakModel, CapturedProp]() -> FText
								{
									TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
									TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
									if (!PinnedRow.IsValid() || !PinnedModel.IsValid() || !PinnedRow->IsLoaded()
										|| !PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
									{
										return FText::GetEmpty();
									}
									const FStructProperty* Sp = CastField<FStructProperty>(CapturedProp);
									const FGameplayTagContainer* ContainerPtr = Sp->ContainerPtrToValuePtr<FGameplayTagContainer>(PinnedRow->Asset.Get());
									if (!ContainerPtr || ContainerPtr->IsEmpty())
									{
										return FText::GetEmpty();
									}
									TArray<FString> Names;
									Names.Reserve(ContainerPtr->Num());
									for (const FGameplayTag& T : *ContainerPtr)
									{
										Names.Add(T.GetTagName().ToString());
									}
									return FText::FromString(FString::Join(Names, TEXT("\n")));
								})
								.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor)
								.AutoWrapText(false)
						];
				}
			}

			// Object/Texture 参照 (ハード/ソフト両対応): サムネイル表示
			// Asset thumbnail cell — handles FObjectProperty and FSoftObjectProperty, in-place swaps via Tick
			if (CastField<FObjectProperty>(Prop) || CastField<FSoftObjectProperty>(Prop))
			{
				return SNew(SObjectThumbnailCell)
					.RowData(WeakRowData)
					.Model(WeakModel)
					.Property(CapturedProp)
					.ThumbnailPool(ThumbnailPool);
			}

			// Enum: セル表示のみ DisplayName に整形 / Enum cell shows DisplayName (CSV path unchanged)
			{
				UEnum* EnumPtr = nullptr;
				if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
				{
					EnumPtr = EnumProp->GetEnum();
				}
				else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
				{
					EnumPtr = ByteProp->Enum;
				}

				if (EnumPtr)
				{
					UEnum* CapturedEnum = EnumPtr;
					return SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text_Lambda([WeakRowData, WeakModel, CapturedProp, CapturedEnum]() -> FText
								{
									TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
									TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
									if (!PinnedRow.IsValid() || !PinnedModel.IsValid() || !PinnedRow->IsLoaded()
										|| !PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
									{
										return FText::GetEmpty();
									}

									int64 IntValue = 0;
									if (const FEnumProperty* Ep = CastField<FEnumProperty>(CapturedProp))
									{
										const void* ValuePtr = Ep->ContainerPtrToValuePtr<void>(PinnedRow->Asset.Get());
										IntValue = Ep->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
									}
									else if (const FByteProperty* Bp = CastField<FByteProperty>(CapturedProp))
									{
										const void* ValuePtr = Bp->ContainerPtrToValuePtr<void>(PinnedRow->Asset.Get());
										IntValue = static_cast<int64>(Bp->GetPropertyValue(ValuePtr));
									}
									return CapturedEnum->GetDisplayNameTextByValue(IntValue);
								})
								.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor)
						];
				}
			}

			// その他のプロパティはTAttributeでリアルタイム更新 / Other properties with TAttribute for real-time updates
			return SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				[
					SNew(STextBlock)
						.Text_Lambda([WeakRowData, WeakModel, CapturedProp]() -> FText
						{
							TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
							TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
							if (PinnedRow.IsValid() && PinnedModel.IsValid() && PinnedRow->IsLoaded())
							{
								return FText::FromString(
									PinnedModel->GetPropertyValueText(PinnedRow->Asset.Get(), CapturedProp));
							}
							return FText::GetEmpty();
						})
						.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor)
				];
		}
	}

	// プロパティが見つからなかった場合 / Property not found
	return SNew(SBox);
}

// アセット名クリック → アセットエディタを開く / Open asset editor on click
void SDataAssetSheetRow::OnAssetNameClicked()
{
	if (RowData->IsLoaded())
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OpenEditorForAsset(RowData->Asset.Get());
		}
	}
}

#undef LOCTEXT_NAMESPACE

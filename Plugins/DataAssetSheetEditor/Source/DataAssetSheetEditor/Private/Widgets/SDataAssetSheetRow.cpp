// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetRow.h"
#include "SObjectThumbnailCell.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetModel.h"
#include "AssetThumbnail.h"
#include "Editor.h"
#include "GameplayTagContainer.h"
#include "Math/ColorList.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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
							if (UDataAsset* Asset = PinnedRow->Asset.Get())
							{
								UPackage* Package = Asset->GetOutermost();
								if (Package && Package->IsDirty())
								{
									DisplayName = TEXT("* ") + DisplayName;
								}
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

			// Bool値はチェックボックスで表示（クリックでトグル編集可）/ Editable bool checkbox — click to toggle
			if (CastField<FBoolProperty>(Prop))
			{
				SDataAssetSheetRow* Self = this;
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
							.OnCheckStateChanged_Lambda([Self, WeakRowData, WeakModel, CapturedProp](ECheckBoxState NewState)
							{
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
								if (!PinnedRow.IsValid() || !PinnedModel.IsValid() || !PinnedRow->IsLoaded()
									|| !PinnedModel->AssetHasProperty(PinnedRow->Asset.Get(), CapturedProp))
								{
									return;
								}

								// 編集対象の行リストを決定 / Determine target rows
								TArray<TSharedPtr<FDataAssetRowData>> TargetRows;
								if (Self->OwnerListView.IsValid())
								{
									TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = Self->OwnerListView->GetSelectedItems();
									bool bThisRowSelected = SelectedItems.ContainsByPredicate(
										[&PinnedRow](const TSharedPtr<FDataAssetRowData>& Item) { return Item == PinnedRow; });
									if (bThisRowSelected && SelectedItems.Num() > 1)
									{
										TargetRows = MoveTemp(SelectedItems);
									}
								}
								if (TargetRows.IsEmpty())
								{
									TargetRows.Add(PinnedRow);
								}

								const FBoolProperty* BoolProp = CastField<FBoolProperty>(CapturedProp);
								const bool bNewValue = (NewState == ECheckBoxState::Checked);

								FScopedTransaction Transaction(
									FText::Format(LOCTEXT("InlineEditBool", "Edit {0}"), FText::FromString(CapturedProp->GetName())));

								for (const TSharedPtr<FDataAssetRowData>& TargetRow : TargetRows)
								{
									if (!TargetRow.IsValid() || !TargetRow->IsLoaded())
									{
										continue;
									}
									UDataAsset* TargetAsset = TargetRow->Asset.Get();
									if (!TargetAsset || !PinnedModel->AssetHasProperty(TargetAsset, CapturedProp))
									{
										continue;
									}
									TargetAsset->Modify();
									void* ValuePtr = BoolProp->ContainerPtrToValuePtr<void>(TargetAsset);
									BoolProp->SetPropertyValue(ValuePtr, bNewValue);
									TargetAsset->MarkPackageDirty();
									PinnedModel->RebuildRowCacheForProperty(TargetRow, CapturedProp);
								}

								PinnedModel->OnInlineEditCommitted.Broadcast();
							})
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

			// Enum: 表示 + ダブルクリックでコンボボックス編集 / Enum cell with inline combo box editing
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

					// コンボボックス用のオプションリストを構築 / Build options list for combo box
					TSharedPtr<TArray<TSharedPtr<FString>>> EnumOptions = MakeShared<TArray<TSharedPtr<FString>>>();
					const int32 NumEnums = CapturedEnum->NumEnums() - 1; // exclude _MAX
					for (int32 i = 0; i < NumEnums; ++i)
					{
						EnumOptions->Add(MakeShared<FString>(CapturedEnum->GetDisplayNameTextByValue(i).ToString()));
					}

					// 読み取り専用テキスト / Read-only display text
					TSharedRef<STextBlock> DisplayText = SNew(STextBlock)
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
						.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor);

					// コンボボックス / Editable combo box
					SDataAssetSheetRow* Self = this;
					FName CapturedColumnId = ColumnId;
					TSharedRef<SComboBox<TSharedPtr<FString>>> ComboBox = SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(EnumOptions.Get())
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption) -> TSharedRef<SWidget>
						{
							return SNew(STextBlock).Text(FText::FromString(*InOption));
						})
						.OnSelectionChanged_Lambda([Self, WeakRowData, WeakModel, CapturedProp, CapturedEnum, CapturedColumnId, EnumOptions](TSharedPtr<FString> InSelection, ESelectInfo::Type SelectInfo)
						{
							if (SelectInfo == ESelectInfo::Direct || !InSelection.IsValid())
							{
								return;
							}
							// 選択されたインデックスを特定 / Find selected index
							int32 SelectedIndex = INDEX_NONE;
							for (int32 i = 0; i < EnumOptions->Num(); ++i)
							{
								if (*(*EnumOptions)[i] == *InSelection)
								{
									SelectedIndex = i;
									break;
								}
							}
							if (SelectedIndex == INDEX_NONE)
							{
								return;
							}
							// Enum値をExportText形式でコミット / Commit using ExportText-compatible format
							int64 EnumValue = CapturedEnum->GetValueByIndex(SelectedIndex);
							FString ValueStr = CapturedEnum->GetNameStringByValue(EnumValue);
							Self->CommitPropertyEdit(CapturedProp, ValueStr);
							Self->ExitEditMode();
						})
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
						];

					// SWidgetSwitcher: slot 0 = 読み取り, slot 1 = コンボボックス
					TSharedPtr<SWidgetSwitcher> Switcher;
					TSharedRef<SWidget> Result = SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						.VAlign(VAlign_Center)
						[
							SAssignNew(Switcher, SWidgetSwitcher)
								.WidgetIndex(0)
								+ SWidgetSwitcher::Slot()
								[
									DisplayText
								]
								+ SWidgetSwitcher::Slot()
								[
									ComboBox
								]
						];

					CellSwitchers.Add(ColumnId, Switcher);

					// ダブルクリックで編集開始 / Double-click to enter edit mode
					return SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
						.Padding(FMargin(0.0f))
						.OnMouseDoubleClick_Lambda([Self, CapturedColumnId](const FGeometry&, const FPointerEvent&) -> FReply
						{
							Self->EnterEditMode(CapturedColumnId);
							return FReply::Handled();
						})
						[
							Result
						];
				}
			}

			// インライン編集可能な型かチェック / Check if this property type supports inline text editing
			const bool bIsInlineEditable =
				CastField<FStrProperty>(Prop) ||
				CastField<FTextProperty>(Prop) ||
				CastField<FNameProperty>(Prop) ||
				CastField<FIntProperty>(Prop) ||
				CastField<FInt64Property>(Prop) ||
				CastField<FFloatProperty>(Prop) ||
				CastField<FDoubleProperty>(Prop);

			if (bIsInlineEditable)
			{
				// 読み取り専用テキスト / Read-only display
				TSharedRef<STextBlock> DisplayText = SNew(STextBlock)
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
					.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor);

				// 編集用テキストボックス / Editable text box
				SDataAssetSheetRow* Self = this;
				FName CapturedColumnId = ColumnId;
				TSharedRef<SEditableTextBox> EditBox = SNew(SEditableTextBox)
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
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.OnTextCommitted_Lambda([Self, CapturedProp, CapturedColumnId](const FText& NewText, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter)
						{
							Self->CommitPropertyEdit(CapturedProp, NewText.ToString());
						}
						Self->ExitEditMode();
					});

				// SWidgetSwitcher: slot 0 = 読み取り, slot 1 = テキストボックス
				TSharedPtr<SWidgetSwitcher> Switcher;
				TSharedRef<SWidget> Result = SNew(SBox)
					.Padding(FMargin(4.0f, 2.0f))
					[
						SAssignNew(Switcher, SWidgetSwitcher)
							.WidgetIndex(0)
							+ SWidgetSwitcher::Slot()
							[
								DisplayText
							]
							+ SWidgetSwitcher::Slot()
							[
								EditBox
							]
					];

				CellSwitchers.Add(ColumnId, Switcher);

				// ダブルクリックで編集開始 / Double-click to enter edit mode
				return SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f))
					.OnMouseDoubleClick_Lambda([Self, CapturedColumnId](const FGeometry&, const FPointerEvent&) -> FReply
					{
						Self->EnterEditMode(CapturedColumnId);
						return FReply::Handled();
					})
					[
						Result
					];
			}

			// その他のプロパティはTAttributeでリアルタイム更新（編集不可）/ Non-editable properties with TAttribute
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
	if (!RowData.IsValid() || !RowData->IsLoaded())
	{
		return;
	}
	UDataAsset* Asset = RowData->Asset.Get();
	if (!Asset)
	{
		return;
	}
	if (UAssetEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		Subsystem->OpenEditorForAsset(Asset);
	}
}

void SDataAssetSheetRow::CommitPropertyEdit(FProperty* Prop, const FString& NewValue)
{
	if (!RowData.IsValid() || !RowData->IsLoaded() || !Model.IsValid())
	{
		return;
	}

	UDataAsset* Asset = RowData->Asset.Get();
	if (!Asset || !Model->AssetHasProperty(Asset, Prop))
	{
		return;
	}

	// 編集対象の行リストを決定：この行が選択に含まれていれば全選択行、そうでなければこの行のみ
	// Determine target rows: all selected rows if this row is in the selection, otherwise just this row
	TArray<TSharedPtr<FDataAssetRowData>> TargetRows;
	if (OwnerListView.IsValid())
	{
		TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = OwnerListView->GetSelectedItems();
		bool bThisRowSelected = SelectedItems.ContainsByPredicate(
			[this](const TSharedPtr<FDataAssetRowData>& Item) { return Item == RowData; });
		if (bThisRowSelected && SelectedItems.Num() > 1)
		{
			TargetRows = MoveTemp(SelectedItems);
		}
	}
	if (TargetRows.IsEmpty())
	{
		TargetRows.Add(RowData);
	}

	FScopedTransaction Transaction(
		FText::Format(LOCTEXT("InlineEdit", "Edit {0}"), FText::FromString(Prop->GetName())));

	for (const TSharedPtr<FDataAssetRowData>& TargetRow : TargetRows)
	{
		if (!TargetRow.IsValid() || !TargetRow->IsLoaded())
		{
			continue;
		}
		UDataAsset* TargetAsset = TargetRow->Asset.Get();
		if (!TargetAsset || !Model->AssetHasProperty(TargetAsset, Prop))
		{
			continue;
		}

		TargetAsset->Modify();
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetAsset);
		if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			TextProp->SetPropertyValue(ValuePtr, FText::FromString(NewValue));
		}
		else
		{
			Prop->ImportText_Direct(*NewValue, ValuePtr, TargetAsset, PPF_None);
		}
		TargetAsset->MarkPackageDirty();
		Model->RebuildRowCacheForProperty(TargetRow, Prop);
	}

	Model->OnInlineEditCommitted.Broadcast();
}

void SDataAssetSheetRow::EnterEditMode(FName ColumnId)
{
	// 未ロードアセットは編集不可 / Cannot edit unloaded assets
	if (!RowData.IsValid() || !RowData->IsLoaded())
	{
		return;
	}

	// 既に他のカラムを編集中なら終了 / Exit current edit if another column is being edited
	if (EditingColumnId != NAME_None)
	{
		ExitEditMode();
	}

	TSharedPtr<SWidgetSwitcher>* FoundSwitcher = CellSwitchers.Find(ColumnId);
	if (!FoundSwitcher || !FoundSwitcher->IsValid())
	{
		return;
	}

	EditingColumnId = ColumnId;
	(*FoundSwitcher)->SetActiveWidgetIndex(1);

	// 編集ウィジェットにフォーカス / Focus the edit widget
	TSharedPtr<SWidget> EditWidget = (*FoundSwitcher)->GetWidget(1);
	if (EditWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(EditWidget.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SDataAssetSheetRow::ExitEditMode()
{
	if (EditingColumnId == NAME_None)
	{
		return;
	}

	TSharedPtr<SWidgetSwitcher>* FoundSwitcher = CellSwitchers.Find(EditingColumnId);
	if (FoundSwitcher && FoundSwitcher->IsValid())
	{
		(*FoundSwitcher)->SetActiveWidgetIndex(0);
	}

	EditingColumnId = NAME_None;
}

#undef LOCTEXT_NAMESPACE

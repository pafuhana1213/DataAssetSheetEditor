// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetRow.h"
#include "SObjectThumbnailCell.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetModel.h"
#include "DataAssetSheetEditorModule.h"
#include "AssetThumbnail.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "GameplayTagContainer.h"
#include "Math/ColorList.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

// 並び替え用ドラッグ操作の生成 / Build the row-reorder drag operation with a decorator
TSharedRef<FDataAssetSheetRowDragDropOp> FDataAssetSheetRowDragDropOp::New(TArray<TSharedPtr<FDataAssetRowData>> InRows)
{
	TSharedRef<FDataAssetSheetRowDragDropOp> Op = MakeShared<FDataAssetSheetRowDragDropOp>();
	Op->DraggedRows = MoveTemp(InRows);
	Op->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Ok"));
	Op->CurrentHoverText = FText::Format(
		LOCTEXT("DragRowsCount", "Move {0} row(s)"), Op->DraggedRows.Num());
	Op->SetupDefaults();
	Op->Construct();
	return Op;
}

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
	WeakSheet = InArgs._Sheet;
	OnReplaceRowAsset = InArgs._OnReplaceRowAsset;
	OnDeleteRow = InArgs._OnDeleteRow;
	OnReorderRows = InArgs._OnReorderRows;

	// ドラッグ&ドロップ並び替えハンドラを登録 / Wire drag-and-drop reorder handlers into the table row
	FSuperRowType::FArguments RowArgs;
	RowArgs.OnDragDetected(FOnDragDetected::CreateSP(this, &SDataAssetSheetRow::HandleDragDetected));
	RowArgs.OnCanAcceptDrop(FOnCanAcceptDrop::CreateSP(this, &SDataAssetSheetRow::HandleCanAcceptDrop));
	RowArgs.OnAcceptDrop(FOnAcceptDrop::CreateSP(this, &SDataAssetSheetRow::HandleAcceptDrop));
	SMultiColumnTableRow::Construct(RowArgs, InOwnerTable);
}

// この行がドラッグ並び替え可能か / Whether this row can participate in drag reorder
bool SDataAssetSheetRow::CanReorder() const
{
	// 手動登録行（ManualAssetIndex 有効）のみ。ソート中は順序が ManualAssets と一致しないため不可。
	// Manual rows only; disabled while a column sort is active (visual order would not match ManualAssets).
	if (!RowData.IsValid() || RowData->ManualAssetIndex == INDEX_NONE)
	{
		return false;
	}
	if (Model.IsValid() && Model->GetSortMode() != EColumnSortMode::None)
	{
		return false;
	}
	return true;
}

FReply SDataAssetSheetRow::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!CanReorder() || !OnReorderRows.IsBound())
	{
		return FReply::Unhandled();
	}

	// この行が選択に含まれていれば選択全体を、そうでなければこの行のみをドラッグ
	// Drag the whole selection if this row is selected, otherwise just this row.
	TArray<TSharedPtr<FDataAssetRowData>> RowsToDrag;
	if (OwnerListView.IsValid())
	{
		TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = OwnerListView->GetSelectedItems();
		if (SelectedItems.Contains(RowData))
		{
			RowsToDrag = MoveTemp(SelectedItems);
		}
	}
	if (RowsToDrag.IsEmpty())
	{
		RowsToDrag.Add(RowData);
	}

	// 並び替え可能な行（手動行）のみ対象 / Keep only reorderable (manual) rows
	RowsToDrag.RemoveAll([](const TSharedPtr<FDataAssetRowData>& Row)
	{
		return !Row.IsValid() || Row->ManualAssetIndex == INDEX_NONE;
	});
	if (RowsToDrag.IsEmpty())
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().BeginDragDrop(FDataAssetSheetRowDragDropOp::New(MoveTemp(RowsToDrag)));
}

TOptional<EItemDropZone> SDataAssetSheetRow::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDataAssetRowData> TargetItem)
{
	TSharedPtr<FDataAssetSheetRowDragDropOp> Op = DragDropEvent.GetOperationAs<FDataAssetSheetRowDragDropOp>();
	if (!Op.IsValid())
	{
		return TOptional<EItemDropZone>();
	}

	// ドロップ先は手動行かつ未ソートのみ / Drop target must be a manual row, and not while sorted
	if (!TargetItem.IsValid() || TargetItem->ManualAssetIndex == INDEX_NONE)
	{
		return TOptional<EItemDropZone>();
	}
	if (Model.IsValid() && Model->GetSortMode() != EColumnSortMode::None)
	{
		return TOptional<EItemDropZone>();
	}

	// 自分自身（ドラッグ中の行）へはドロップ不可 / Cannot drop onto a row being dragged
	if (Op->DraggedRows.Contains(TargetItem))
	{
		return TOptional<EItemDropZone>();
	}

	// 中央(Onto)は上方向として扱う / Treat "onto" as "above"
	return (DropZone == EItemDropZone::BelowItem) ? EItemDropZone::BelowItem : EItemDropZone::AboveItem;
}

FReply SDataAssetSheetRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDataAssetRowData> TargetItem)
{
	TSharedPtr<FDataAssetSheetRowDragDropOp> Op = DragDropEvent.GetOperationAs<FDataAssetSheetRowDragDropOp>();
	if (!Op.IsValid() || !TargetItem.IsValid() || !OnReorderRows.IsBound())
	{
		return FReply::Unhandled();
	}

	const EItemDropZone ResolvedZone = (DropZone == EItemDropZone::BelowItem)
		? EItemDropZone::BelowItem
		: EItemDropZone::AboveItem;
	OnReorderRows.Execute(Op->DraggedRows, TargetItem, ResolvedZone);
	return FReply::Handled();
}

FReply SDataAssetSheetRow::OnDeleteRowClicked()
{
	if (RowData.IsValid() && OnDeleteRow.IsBound())
	{
		OnDeleteRow.Execute(RowData);
	}
	return FReply::Handled();
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
		return GenerateAssetNameCell();
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

								const bool bNewValue = (NewState == ECheckBoxState::Checked);
								const FString NewValueString = bNewValue ? TEXT("True") : TEXT("False");

								FScopedTransaction Transaction(
									FText::Format(LOCTEXT("InlineEditBool", "Edit {0}"), FText::FromString(CapturedProp->GetName())));

								bool bAnyCommitted = false;
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
									FString FailureReason;
									if (PinnedModel->SetPropertyValueFromString(TargetRow, CapturedProp, NewValueString, &FailureReason))
									{
										bAnyCommitted = true;
									}
									else
									{
										UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Inline bool edit failed for %s.%s: %s"),
											*TargetAsset->GetName(), *CapturedProp->GetName(), *FailureReason);
									}
								}

								if (bAnyCommitted)
								{
									PinnedModel->OnInlineEditCommitted.Broadcast();
								}
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
					TSharedPtr<TArray<int64>> EnumValues = MakeShared<TArray<int64>>();
					for (int32 i = 0; i < CapturedEnum->NumEnums(); ++i)
					{
						const FString NameByIndex = CapturedEnum->GetNameStringByIndex(i);
						if (CapturedEnum->HasMetaData(TEXT("Hidden"), i) || NameByIndex.EndsWith(TEXT("_MAX")))
						{
							continue;
						}

						EnumOptions->Add(MakeShared<FString>(CapturedEnum->GetDisplayNameTextByIndex(i).ToString()));
						EnumValues->Add(CapturedEnum->GetValueByIndex(i));
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
						.OnSelectionChanged_Lambda([Self, WeakRowData, WeakModel, CapturedProp, CapturedEnum, CapturedColumnId, EnumOptions, EnumValues](TSharedPtr<FString> InSelection, ESelectInfo::Type SelectInfo)
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
							if (!EnumValues->IsValidIndex(SelectedIndex))
							{
								return;
							}
							int64 EnumValue = (*EnumValues)[SelectedIndex];
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

// この行がピッカーで編集可能か（ManualAssets由来かつ非bShowAll）
// Whether this row's asset can be swapped via the picker (manual-asset rows only)
bool SDataAssetSheetRow::IsRowEditable() const
{
	UDataAssetSheet* Sheet = WeakSheet.Get();
	if (!Sheet || Sheet->bShowAll || !RowData.IsValid())
	{
		return false;
	}
	const FSoftObjectPath& Path = RowData->AssetPath;
	return Sheet->ManualAssets.ContainsByPredicate(
		[&Path](const TSoftObjectPtr<UDataAsset>& Existing)
		{
			return Existing.ToSoftObjectPath() == Path;
		});
}

// アセット名セルを構築 / Build the AssetName cell
TSharedRef<SWidget> SDataAssetSheetRow::GenerateAssetNameCell()
{
	TWeakPtr<FDataAssetRowData> WeakRowData = RowData;

	// 未保存マーク（* ）/ Dirty indicator widget (kept separately since the picker only shows the name)
	auto MakeDirtyMark = [WeakRowData]() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text_Lambda([WeakRowData]() -> FText
			{
				TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
				if (PinnedRow.IsValid() && PinnedRow->IsLoaded())
				{
					if (UDataAsset* Asset = PinnedRow->Asset.Get())
					{
						UPackage* Package = Asset->GetOutermost();
						if (Package && Package->IsDirty())
						{
							return LOCTEXT("DirtyMark", "*");
						}
					}
				}
				return FText::GetEmpty();
			});
	};

	// 編集可: ChooserTable風アセットピッカー / Editable: ChooserTable-style asset picker
	if (IsRowEditable())
	{
		UDataAssetSheet* Sheet = WeakSheet.Get();
		TWeakObjectPtr<UDataAssetSheet> SheetWeak = WeakSheet;
		TWeakPtr<FDataAssetSheetModel> ModelWeak = Model;
		FOnReplaceRowAsset ReplaceDelegate = OnReplaceRowAsset;

		return SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				// 未保存マーク / Dirty indicator
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
					[
						MakeDirtyMark()
					]

				// アセットピッカー / Asset picker (non-property mode)
				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SObjectPropertyEntryBox)
							.AllowedClass(Sheet ? Sheet->TargetClass.Get() : UDataAsset::StaticClass())
							.DisplayThumbnail(false)
							.DisplayUseSelected(true)
							.DisplayBrowse(true)
							.EnableContentPicker(true)
							.AllowClear(false)
							.ObjectPath_Lambda([WeakRowData]() -> FString
							{
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								return PinnedRow.IsValid() ? PinnedRow->AssetPath.ToString() : FString();
							})
							.OnShouldFilterAsset_Lambda([WeakRowData, SheetWeak, ModelWeak](const FAssetData& AssetData) -> bool
							{
								// true を返すと候補から除外 / Returning true filters the asset OUT
								UDataAssetSheet* PinnedSheet = SheetWeak.Get();
								if (!PinnedSheet)
								{
									return true;
								}
								// 許可クラス（Engineクラス除外等）/ Allowed-class check
								if (!PinnedSheet->IsAllowedDataAssetClass(AssetData.GetClass()))
								{
									return true;
								}
								// 自分の行の現在値は残す / Keep this row's current value selectable
								const FSoftObjectPath Path = AssetData.GetSoftObjectPath();
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								if (PinnedRow.IsValid() && Path == PinnedRow->AssetPath)
								{
									return false;
								}
								// 既に表示中（ManualAssets + Collections等）のアセットは除外 / Exclude assets already shown in the sheet
								TSharedPtr<FDataAssetSheetModel> PinnedModel = ModelWeak.Pin();
								if (PinnedModel.IsValid())
								{
									for (const TSharedPtr<FDataAssetRowData>& Row : PinnedModel->GetRowDataList())
									{
										if (Row.IsValid() && Row->AssetPath == Path)
										{
											return true;
										}
									}
								}
								return false;
							})
							.OnObjectChanged_Lambda([WeakRowData, ReplaceDelegate](const FAssetData& NewAssetData)
							{
								// paste等でNoneが来た場合の保険 / Guard against invalid (e.g. pasted None) selections
								if (!NewAssetData.IsValid())
								{
									return;
								}
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								if (PinnedRow.IsValid() && ReplaceDelegate.IsBound())
								{
									ReplaceDelegate.Execute(PinnedRow->AssetPath, NewAssetData);
								}
							})
					]

				// 削除ボタン（この行を ManualAssets から除外）/ Delete button (remove this row from ManualAssets)
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(FMargin(2.0f, 0.0f))
							.ToolTipText(LOCTEXT("DeleteRowTooltip", "この行をシートから削除 / Remove this row from the sheet"))
							.OnClicked(this, &SDataAssetSheetRow::OnDeleteRowClicked)
							[
								SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Delete"))
									.ColorAndOpacity(FSlateColor::UseForeground())
							]
					]

			];
	}

	// 読み取り専用: 名前 + ブラウズ / Read-only: name + browse (bShowAll or collection-backed rows)
	return SNew(SBox)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			// アセット名（未保存時は * 付き）/ Asset name with unsaved indicator
			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
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
				]

			// コンテンツブラウザで表示ボタン / Find in Content Browser button
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ContentPadding(FMargin(2.0f, 0.0f))
						.ToolTipText(LOCTEXT("FindInContentBrowserRowTooltip", "コンテンツブラウザで表示 / Find in Content Browser"))
						.IsEnabled_Lambda([WeakRowData]() -> bool
						{
							TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
							return PinnedRow.IsValid() && PinnedRow->IsLoaded();
						})
						.OnClicked(this, &SDataAssetSheetRow::OnBrowseToAssetClicked)
						[
							SNew(SImage)
								.Image(FAppStyle::GetBrush("SystemWideCommands.FindInContentBrowser"))
								.ColorAndOpacity(FSlateColor::UseForeground())
						]
				]
		];
}

// コンテンツブラウザでこの行のアセットを表示 / Sync to this row's asset in the Content Browser
FReply SDataAssetSheetRow::OnBrowseToAssetClicked()
{
	if (!RowData.IsValid() || !RowData->IsLoaded())
	{
		return FReply::Handled();
	}
	if (UDataAsset* Asset = RowData->Asset.Get())
	{
		TArray<FAssetData> AssetDatas;
		AssetDatas.Add(FAssetData(Asset));
		FContentBrowserModule& ContentBrowserModule =
			FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetDatas);
	}
	return FReply::Handled();
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

	bool bAnyCommitted = false;
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

		FString FailureReason;
		if (Model->SetPropertyValueFromString(TargetRow, Prop, NewValue, &FailureReason))
		{
			bAnyCommitted = true;
			continue;
		}

		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Inline edit failed for %s.%s value '%s': %s"),
			*TargetAsset->GetName(), *Prop->GetName(), *NewValue, *FailureReason);
	}

	if (bAnyCommitted)
	{
		Model->OnInlineEditCommitted.Broadcast();
	}
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

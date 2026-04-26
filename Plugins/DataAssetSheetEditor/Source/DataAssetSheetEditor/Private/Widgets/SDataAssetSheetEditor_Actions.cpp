// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetEditor.h"
#include "SDataAssetSheetListView.h"
#include "SObjectThumbnailCell.h"
#include "SDataAssetSheetRow.h"
#include "SDropTargetOverlay.h"
#include "Utils/DataAssetSheetCSVUtils.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetModel.h"
#include "DataAssetSheetEditorModule.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SComboButton.h"
#include "UObject/Package.h"
#include "Widgets/SNullWidget.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Input/SSearchBox.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "Editor/EditorEngine.h"
#include "Framework/Commands/GenericCommands.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetThumbnail.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

void SDataAssetSheetEditor::BindCommands(const TSharedRef<FUICommandList>& InCommandList)
{
	CommandList = InCommandList;

	CommandList->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::CopySelectedRows),
		FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::HasSelectedLoadedAsset)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::PasteOnSelectedRows),
		FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::CanPaste)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::DuplicateSelectedAsset),
		FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::HasSelectedLoadedAsset)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::RemoveSelectedFromManualAssets),
		FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::CanRemoveSelectedFromManualAssets)
	);
}


TSharedPtr<SWidget> SDataAssetSheetEditor::OnConstructContextMenu()
{
	if (!HasSelectedLoadedAsset())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection("AssetActions", LOCTEXT("AssetActionsSection", "Asset Actions"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("BrowseToAsset", "Browse to Asset"),
			LOCTEXT("BrowseToAssetTooltip", "Show this asset in the Content Browser"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::BrowseToSelectedAsset),
				FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::HasSelectedLoadedAsset)
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SaveAsset", "Save Asset"),
			LOCTEXT("SaveAssetTooltip", "Save the selected asset"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset"),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
					TArray<UPackage*> PackagesToSave;
					for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
					{
						if (Item.IsValid() && Item->IsLoaded())
						{
							UDataAsset* Asset = Item->Asset.Get();
							if (!Asset)
							{
								continue;
							}
							UPackage* Package = Asset->GetOutermost();
							if (Package && Package->IsDirty())
							{
								PackagesToSave.Add(Package);
							}
						}
					}
					if (PackagesToSave.Num() > 0)
					{
						UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
					}
				}),
				FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::HasSelectedLoadedAsset)
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("EditOperations", LOCTEXT("EditOperationsSection", "Edit"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AssetOperations", LOCTEXT("AssetOperationsSection", "Operations"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveFromManualAssets", "Remove"),
			LOCTEXT("RemoveFromManualAssetsTooltip", "Remove selected assets from Manual Assets list"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::RemoveSelectedFromManualAssets),
				FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::CanRemoveSelectedFromManualAssets)
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("References", LOCTEXT("ReferencesSection", "References"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("FindReferences", "Find References"),
			LOCTEXT("FindReferencesTooltip", "Open the Reference Viewer for this asset"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::FindReferencesForSelectedAsset),
				FCanExecuteAction::CreateSP(this, &SDataAssetSheetEditor::HasSelectedLoadedAsset)
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SDataAssetSheetEditor::BrowseToSelectedAsset()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	TArray<FAssetData> AssetDatas;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid() && Item->IsLoaded())
		{
			AssetDatas.Add(FAssetData(Item->Asset.Get()));
		}
	}

	if (AssetDatas.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetDatas);
	}
}


bool SDataAssetSheetEditor::HasSelectedLoadedAsset() const
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid() && Item->IsLoaded())
		{
			return true;
		}
	}
	return false;
}


void SDataAssetSheetEditor::CreateNewAsset()
{
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet || !Sheet->TargetClass)
	{
		return;
	}

	// UDataAssetFactoryを生成し、TargetClassを事前設定 / Create factory with preset target class
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = Sheet->TargetClass;

	// ダイアログ付きでアセット作成（クラス選択ダイアログはスキップ）
	// Create asset with save dialog, skip class picker since TargetClass is already known
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString DefaultPath = FPackageName::GetLongPackagePath(Sheet->GetPathName());
	AssetTools.CreateAssetWithDialog(Sheet->TargetClass->GetName(), DefaultPath, Sheet->TargetClass, Factory, NAME_None, /*bCallConfigureProperties=*/ false);
	// OnAssetAddedフックで自動的にテーブル更新される / Table updates via OnAssetAdded hook
}


void SDataAssetSheetEditor::RemoveSelectedFromManualAssets()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 0)
	{
		return;
	}

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return;
	}

	// 除外対象パスセットを構築 / Build set of paths to remove
	TSet<FSoftObjectPath> PathsToRemove;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			PathsToRemove.Add(Item->AssetPath);
		}
	}

	// Undo対応 / Undo support
	FScopedTransaction Transaction(LOCTEXT("RemoveFromManualAssetsTransaction", "Remove Assets from Sheet"));
	Sheet->Modify();

	// ManualAssetsから除外 / Remove from ManualAssets
	Sheet->ManualAssets.RemoveAll([&PathsToRemove](const TSoftObjectPtr<UDataAsset>& SoftPtr)
	{
		return PathsToRemove.Contains(SoftPtr.ToSoftObjectPath());
	});
	Sheet->MarkPackageDirty();

	// テーブル行から除外 / Remove from table rows
	TArray<TSharedPtr<FDataAssetRowData>>& RowDataList = Model->GetMutableRowDataList();
	RowDataList.RemoveAll([&PathsToRemove](const TSharedPtr<FDataAssetRowData>& RowData)
	{
		return PathsToRemove.Contains(RowData->AssetPath);
	});

	Model->ReapplyFilter();
	DetailsView->SetObject(nullptr);
	AssetListView->RequestListRefresh();
}


bool SDataAssetSheetEditor::CanRemoveSelectedFromManualAssets() const
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 0)
	{
		return false;
	}

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return false;
	}

	// ManualAssetsのパスセットを構築 / Build set of ManualAssets paths
	TSet<FSoftObjectPath> ManualPaths;
	for (const TSoftObjectPtr<UDataAsset>& SoftPtr : Sheet->ManualAssets)
	{
		ManualPaths.Add(SoftPtr.ToSoftObjectPath());
	}

	// 全選択アイテムがManualAssetsに含まれているか確認 / Check all selected items are in ManualAssets
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (!Item.IsValid() || !ManualPaths.Contains(Item->AssetPath))
		{
			return false;
		}
	}

	return true;
}


void SDataAssetSheetEditor::DuplicateSelectedAsset()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() != 1 || !SelectedItems[0].IsValid() || !SelectedItems[0]->IsLoaded())
	{
		return;
	}

	UObject* OriginalAsset = SelectedItems[0]->Asset.Get();
	FString PackagePath = FPackageName::GetLongPackagePath(OriginalAsset->GetPathName());
	FString AssetName = OriginalAsset->GetName() + TEXT("_Copy");

	// ダイアログ付きで複製 / Duplicate with dialog
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.DuplicateAssetWithDialog(AssetName, PackagePath, OriginalAsset);
	// OnAssetAddedフックで自動的にテーブル更新される / Table updates via OnAssetAdded hook
}


void SDataAssetSheetEditor::SaveAllModifiedAssets()
{
	TArray<UPackage*> DirtyPackages;
	for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
	{
		if (RowData.IsValid() && RowData->IsLoaded())
		{
			UDataAsset* Asset = RowData->Asset.Get();
			if (!Asset)
			{
				continue;
			}
			UPackage* Package = Asset->GetOutermost();
			if (Package && Package->IsDirty())
			{
				DirtyPackages.Add(Package);
			}
		}
	}

	if (DirtyPackages.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(DirtyPackages, true);
	}
}


bool SDataAssetSheetEditor::HasModifiedAssets() const
{
	for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
	{
		if (RowData.IsValid() && RowData->IsLoaded())
		{
			UDataAsset* Asset = RowData->Asset.Get();
			if (!Asset)
			{
				continue;
			}
			UPackage* Package = Asset->GetOutermost();
			if (Package && Package->IsDirty())
			{
				return true;
			}
		}
	}
	return false;
}


void SDataAssetSheetEditor::FindReferencesForSelectedAsset()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 0)
	{
		return;
	}

	TArray<FAssetIdentifier> AssetIdentifiers;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid() && Item->IsLoaded())
		{
			UDataAsset* Asset = Item->Asset.Get();
			if (!Asset)
			{
				continue;
			}
			FName PackageName = Asset->GetOutermost()->GetFName();
			AssetIdentifiers.Add(FAssetIdentifier(PackageName));
		}
	}

	if (AssetIdentifiers.Num() > 0)
	{
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}


FReply SDataAssetSheetEditor::HandleDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (!AssetOp.IsValid() || !AssetOp->HasAssets())
	{
		return FReply::Unhandled();
	}

	// 少なくとも1つのアセットがTargetClassに一致すればドロップ可能 / Accept if at least one asset matches
	for (const FAssetData& AssetData : AssetOp->GetAssets())
	{
		if (IsTargetAsset(AssetData))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}


FReply SDataAssetSheetEditor::HandleDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (!AssetOp.IsValid() || !AssetOp->HasAssets())
	{
		return FReply::Unhandled();
	}

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return FReply::Unhandled();
	}

	int32 AddedCount = 0;

	// Undo対応 / Undo support
	FScopedTransaction Transaction(LOCTEXT("AddDroppedAssets", "Add Dropped Assets to Sheet"));
	Sheet->Modify();

	// ManualAssets内の既存パスセットを構築（重複防止用）/ Build set of existing ManualAsset paths
	TSet<FSoftObjectPath> ExistingManualPaths;
	for (const TSoftObjectPtr<UDataAsset>& Existing : Sheet->ManualAssets)
	{
		ExistingManualPaths.Add(Existing.ToSoftObjectPath());
	}

	// テーブル内の既存パスセットを構築（行の重複追加防止用）/ Build set of existing row paths
	TSet<FSoftObjectPath> ExistingRowPaths;
	for (const TSharedPtr<FDataAssetRowData>& Row : Model->GetRowDataList())
	{
		ExistingRowPaths.Add(Row->AssetPath);
	}

	for (const FAssetData& AssetData : AssetOp->GetAssets())
	{
		if (!IsTargetAsset(AssetData))
		{
			continue;
		}

		FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();

		// ManualAssets内の重複スキップ / Skip if already in ManualAssets
		if (ExistingManualPaths.Contains(AssetPath))
		{
			continue;
		}

		// ManualAssetsに追加 / Add to ManualAssets
		Sheet->ManualAssets.Add(TSoftObjectPtr<UDataAsset>(AssetPath));
		ExistingManualPaths.Add(AssetPath);

		// テーブル未表示の場合のみ行データ追加 / Add row data only if not already in table
		if (!ExistingRowPaths.Contains(AssetPath))
		{
			TSharedPtr<FDataAssetRowData> NewRowData = MakeShared<FDataAssetRowData>();
			NewRowData->AssetPath = AssetPath;
			NewRowData->AssetName = AssetData.AssetName.ToString();

			if (UObject* LoadedObject = AssetPath.ResolveObject())
			{
				if (UDataAsset* DataAsset = Cast<UDataAsset>(LoadedObject))
				{
					NewRowData->Asset = DataAsset;
				}
			}

			Model->GetMutableRowDataList().Add(NewRowData);
			ExistingRowPaths.Add(AssetPath);
		}

		++AddedCount;
	}

	if (AddedCount > 0)
	{
		Sheet->MarkPackageDirty();
		Model->ReapplyFilter();
		AssetListView->RequestListRefresh();
		StartAsyncLoad();

		// トースト通知 / Toast notification
		FNotificationInfo Info(FText::Format(
			LOCTEXT("DragDropAdded", "Added {0} asset(s) to ManualAssets"),
			AddedCount));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Drag-and-drop: added %d asset(s) to ManualAssets"), AddedCount);
	}

	return (AddedCount > 0) ? FReply::Handled() : FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE

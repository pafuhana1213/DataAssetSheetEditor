// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "FDataAssetSheetEditorToolkit.h"
#include "SDataAssetSheetEditor.h"
#include "DataAssetSheet.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FDataAssetSheetEditorToolkit"

const FName FDataAssetSheetEditorToolkit::MainTabId(TEXT("DataAssetSheetEditorMain"));

void FDataAssetSheetEditorToolkit::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDataAssetSheet* InAsset)
{
	EditingAsset = InAsset;

	// タブレイアウト定義 / Define tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("DataAssetSheetEditorLayout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
						->AddTab(MainTabId, ETabState::OpenedTab)
						->SetHideTabWell(true)
				)
		);

	// Toolkit初期化 / Initialize the toolkit
	InitAssetEditor(Mode, InitToolkitHost, TEXT("DataAssetSheetEditorApp"), Layout, true, true, InAsset);
}

FName FDataAssetSheetEditorToolkit::GetToolkitFName() const
{
	return FName("DataAssetSheetEditor");
}

FText FDataAssetSheetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "DataAsset Sheet Editor");
}

FString FDataAssetSheetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DataAssetSheet ").ToString();
}

FLinearColor FDataAssetSheetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.2f, 0.8f, 0.4f);
}

void FDataAssetSheetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(MainTabId, FOnSpawnTab::CreateSP(this, &FDataAssetSheetEditorToolkit::SpawnMainTab))
		.SetDisplayName(LOCTEXT("MainTab", "Sheet Editor"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FDataAssetSheetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(MainTabId);
}

TSharedRef<SDockTab> FDataAssetSheetEditorToolkit::SpawnMainTab(const FSpawnTabArgs& Args)
{
	UDataAssetSheet* Asset = EditingAsset.Get();

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SDataAssetSheetEditor)
				.DataAssetSheet(Asset)
		];
}

#undef LOCTEXT_NAMESPACE

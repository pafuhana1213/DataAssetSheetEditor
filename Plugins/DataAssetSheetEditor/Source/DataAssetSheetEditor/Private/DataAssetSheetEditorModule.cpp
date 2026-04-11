// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "FDataAssetSheetEditorModule"

DEFINE_LOG_CATEGORY(LogDataAssetSheetEditor);

static const FName DataAssetSheetEditorTabName("DataAssetSheetEditor");

void FDataAssetSheetEditorModule::StartupModule()
{
	RegisterTabSpawner();
	RegisterMenuExtensions();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("DataAssetSheetEditor module started"));
}

void FDataAssetSheetEditorModule::ShutdownModule()
{
	UnregisterTabSpawner();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("DataAssetSheetEditor module shut down"));
}

void FDataAssetSheetEditorModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		DataAssetSheetEditorTabName,
		FOnSpawnTab::CreateRaw(this, &FDataAssetSheetEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "DataAsset Sheet Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FDataAssetSheetEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DataAssetSheetEditorTabName);
}

void FDataAssetSheetEditorModule::RegisterMenuExtensions()
{
	// TODO: UToolMenusを使ってWindowメニューにエントリを追加 / Register menu entry via UToolMenus
}

TSharedRef<SDockTab> FDataAssetSheetEditorModule::SpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// TODO: メインのスプレッドシートエディタウィジェットを作成 / Create the main spreadsheet editor widget
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDataAssetSheetEditorModule, DataAssetSheetEditor)

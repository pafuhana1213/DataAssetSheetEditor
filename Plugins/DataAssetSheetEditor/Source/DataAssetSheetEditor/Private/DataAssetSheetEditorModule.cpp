// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetEditorModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDataAssetSheetEditor);

void FDataAssetSheetEditorModule::StartupModule()
{
	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("DataAssetSheetEditor module started"));
}

void FDataAssetSheetEditorModule::ShutdownModule()
{
	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("DataAssetSheetEditor module shut down"));
}

IMPLEMENT_MODULE(FDataAssetSheetEditorModule, DataAssetSheetEditor)

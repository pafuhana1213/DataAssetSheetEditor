// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDataAssetSheetEditor, Log, All);

class FDataAssetSheetEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** タブスポナーの登録・解除 / Register and unregister tab spawner */
	void RegisterTabSpawner();
	void UnregisterTabSpawner();

	/** メニュー拡張の登録 / Register menu extensions */
	void RegisterMenuExtensions();

	/** タブ生成コールバック / Tab spawn callback */
	TSharedRef<class SDockTab> SpawnTab(const class FSpawnTabArgs& SpawnTabArgs);
};

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
};

// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UDataAssetSheet;

/**
 * DataAssetSheetのアセットエディタ / Asset editor toolkit for UDataAssetSheet
 * ダブルクリックで起動し、SDataAssetSheetEditorをホスティングする
 */
class FDataAssetSheetEditorToolkit : public FAssetEditorToolkit
{
public:
	// エディタ初期化 / Initialize the editor with the given asset
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDataAssetSheet* InAsset);

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

private:
	// メインタブ生成コールバック / Main tab spawn callback
	TSharedRef<SDockTab> SpawnMainTab(const FSpawnTabArgs& Args);

	// 編集中のアセット / The asset being edited
	TWeakObjectPtr<UDataAssetSheet> EditingAsset;

	// メインタブID / Main tab identifier
	static const FName MainTabId;
};

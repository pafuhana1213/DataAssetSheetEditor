// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UDataAssetSheet;
class SDataAssetSheetEditor;
class SDataAssetSheetSettingsTab;

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
	virtual void PostRegenerateMenusAndToolbars() override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

private:
	// Target Classハイパーリンククリック / Navigate to the target class definition
	void OnTargetClassHyperlinkClicked();

	// タブ生成コールバック / Tab spawn callbacks
	TSharedRef<SDockTab> SpawnTableTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnSettingsTab(const FSpawnTabArgs& Args);

	// 編集中のアセット / The asset being edited
	TWeakObjectPtr<UDataAssetSheet> EditingAsset;

	// エディタウィジェット / Editor widget (owns table and details)
	TSharedPtr<SDataAssetSheetEditor> EditorWidget;

	// タブID / Tab identifiers
	static const FName TableTabId;
	static const FName DetailsTabId;
	static const FName SettingsTabId;
};

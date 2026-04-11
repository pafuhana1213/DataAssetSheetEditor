// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "FDataAssetSheetEditorToolkit.h"
#include "SDataAssetSheetEditor.h"
#include "SDataAssetSheetSettingsTab.h"
#include "DataAssetSheet.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FDataAssetSheetEditorToolkit"

const FName FDataAssetSheetEditorToolkit::TableTabId(TEXT("DataAssetSheetEditorTable"));
const FName FDataAssetSheetEditorToolkit::DetailsTabId(TEXT("DataAssetSheetEditorDetails"));
const FName FDataAssetSheetEditorToolkit::SettingsTabId(TEXT("DataAssetSheetEditorSettings"));

void FDataAssetSheetEditorToolkit::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDataAssetSheet* InAsset)
{
	EditingAsset = InAsset;

	// エディタウィジェットを先に生成（タブスポーナーから参照される）/ Create editor widget before tab spawning
	EditorWidget = SNew(SDataAssetSheetEditor)
		.DataAssetSheet(InAsset);

	// タブレイアウト定義 / Define tab layout
	// 左：テーブル（常時表示）、右：詳細+設定のタブスタック
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("DataAssetSheetEditorLayout_v3")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// 左：テーブル / Left: table (always visible)
					FTabManager::NewStack()
						->AddTab(TableTabId, ETabState::OpenedTab)
						->SetSizeCoefficient(0.6f)
				)
				->Split
				(
					// 右：詳細+設定のスタック / Right: details + settings stack
					FTabManager::NewStack()
						->AddTab(DetailsTabId, ETabState::OpenedTab)
						->AddTab(SettingsTabId, ETabState::OpenedTab)
						->SetForegroundTab(DetailsTabId)
						->SetSizeCoefficient(0.4f)
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

	InTabManager->RegisterTabSpawner(TableTabId, FOnSpawnTab::CreateSP(this, &FDataAssetSheetEditorToolkit::SpawnTableTab))
		.SetDisplayName(LOCTEXT("TableTab", "Sheet"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FDataAssetSheetEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(SettingsTabId, FOnSpawnTab::CreateSP(this, &FDataAssetSheetEditorToolkit::SpawnSettingsTab))
		.SetDisplayName(LOCTEXT("SettingsTab", "Settings"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));
}

void FDataAssetSheetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(TableTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(SettingsTabId);
}

TSharedRef<SDockTab> FDataAssetSheetEditorToolkit::SpawnTableTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("TableTabLabel", "Sheet"))
		[
			EditorWidget->GetTableWidget()
		];
}

TSharedRef<SDockTab> FDataAssetSheetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("DetailsTabLabel", "Details"))
		[
			EditorWidget->GetDetailsWidget()
		];
}

TSharedRef<SDockTab> FDataAssetSheetEditorToolkit::SpawnSettingsTab(const FSpawnTabArgs& Args)
{
	UDataAssetSheet* Asset = EditingAsset.Get();

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("SettingsTabLabel", "Settings"))
		[
			SNew(SDataAssetSheetSettingsTab)
				.DataAssetSheet(Asset)
				.OnSettingsChanged(FSimpleDelegate::CreateSP(EditorWidget.ToSharedRef(), &SDataAssetSheetEditor::OnSettingsChanged))
		];
}

#undef LOCTEXT_NAMESPACE

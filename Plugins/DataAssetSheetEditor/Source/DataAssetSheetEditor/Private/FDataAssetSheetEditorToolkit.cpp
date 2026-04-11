// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "FDataAssetSheetEditorToolkit.h"
#include "SDataAssetSheetEditor.h"
#include "SDataAssetSheetSettingsTab.h"
#include "DataAssetSheet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SHyperlink.h"
#include "SourceCodeNavigation.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

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

	// キーボードショートカット / Bind keyboard shortcuts
	EditorWidget->BindCommands(GetToolkitCommands());

	// ツールバー拡張 / Extend toolbar with custom buttons
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender());
	ExtendToolbar(ToolbarExtender);
	AddToolbarExtender(ToolbarExtender);

	RegenerateMenusAndToolbars();
}

void FDataAssetSheetEditorToolkit::PostRegenerateMenusAndToolbars()
{
	UDataAssetSheet* Asset = EditingAsset.Get();
	if (Asset && Asset->TargetClass)
	{
		TSharedRef<SHorizontalBox> MenuOverlayBox = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ShadowOffset(FVector2D::UnitVector)
				.Text(LOCTEXT("TargetClassLabel", "Target Class: "))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHyperlink)
				.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
				.OnNavigate(this, &FDataAssetSheetEditorToolkit::OnTargetClassHyperlinkClicked)
				.Text(Asset->TargetClass->GetDisplayNameText())
				.ToolTipText(LOCTEXT("TargetClassToolTip", "Open the target DataAsset class definition"))
			];

		// DisplayClassが設定されている場合は隣に表示 / Show DisplayClass next to TargetClass if set
		if (Asset->DisplayClass && Asset->DisplayClass->IsChildOf(Asset->TargetClass))
		{
			MenuOverlayBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ShadowOffset(FVector2D::UnitVector)
					.Text(LOCTEXT("DisplayClassLabel", "Display Class: "))
				];

			MenuOverlayBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SHyperlink)
					.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
					.OnNavigate(this, &FDataAssetSheetEditorToolkit::OnDisplayClassHyperlinkClicked)
					.Text(Asset->DisplayClass->GetDisplayNameText())
					.ToolTipText(LOCTEXT("DisplayClassToolTip", "Open the display class definition"))
				];
		}

		SetMenuOverlay(MenuOverlayBox);
	}
}

void FDataAssetSheetEditorToolkit::ExtendToolbar(TSharedPtr<FExtender> Extender)
{
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FDataAssetSheetEditorToolkit::FillToolbar)
	);
}

void FDataAssetSheetEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("DataAssetSheetCommands");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateSP(EditorWidget.ToSharedRef(), &SDataAssetSheetEditor::CreateNewAsset)),
			NAME_None,
			LOCTEXT("NewAssetText", "New Asset"),
			LOCTEXT("NewAssetTooltip", "Create a new DataAsset of the target class"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));

		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(EditorWidget.ToSharedRef(), &SDataAssetSheetEditor::SaveAllModifiedAssets),
				FCanExecuteAction::CreateSP(EditorWidget.ToSharedRef(), &SDataAssetSheetEditor::HasModifiedAssets)),
			NAME_None,
			LOCTEXT("SaveModifiedText", "Save"),
			LOCTEXT("SaveModifiedTooltip", "Save all modified assets"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset"));
	}
	ToolbarBuilder.EndSection();
}

void FDataAssetSheetEditorToolkit::OnTargetClassHyperlinkClicked()
{
	UDataAssetSheet* Asset = EditingAsset.Get();
	if (!Asset || !Asset->TargetClass)
	{
		return;
	}

	UClass* TargetClass = Asset->TargetClass;

	// Blueprintクラスの場合はBPエディタを開く / Open Blueprint editor for BP classes
	if (UBlueprint* BP = Cast<UBlueprint>(TargetClass->ClassGeneratedBy))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BP);
	}
	// C++クラスの場合はソースコードに遷移 / Navigate to source code for C++ classes
	else if (FSourceCodeNavigation::CanNavigateToClass(TargetClass))
	{
		FSourceCodeNavigation::NavigateToClass(TargetClass);
	}
}

void FDataAssetSheetEditorToolkit::OnDisplayClassHyperlinkClicked()
{
	UDataAssetSheet* Asset = EditingAsset.Get();
	if (!Asset || !Asset->DisplayClass)
	{
		return;
	}

	UClass* DisplayClass = Asset->DisplayClass;

	// Blueprintクラスの場合はBPエディタを開く / Open Blueprint editor for BP classes
	if (UBlueprint* BP = Cast<UBlueprint>(DisplayClass->ClassGeneratedBy))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BP);
	}
	// C++クラスの場合はソースコードに遷移 / Navigate to source code for C++ classes
	else if (FSourceCodeNavigation::CanNavigateToClass(DisplayClass))
	{
		FSourceCodeNavigation::NavigateToClass(DisplayClass);
	}
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
				.OnSettingsChanged(FSimpleDelegate::CreateLambda([this]()
				{
					EditorWidget->OnSettingsChanged();
					RegenerateMenusAndToolbars();
				}))
		];
}

#undef LOCTEXT_NAMESPACE

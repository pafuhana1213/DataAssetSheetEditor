// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetSettingsTab.h"
#include "DataAssetSheet.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetSettingsTab"

void SDataAssetSheetSettingsTab::Construct(const FArguments& InArgs)
{
	DataAssetSheet = InArgs._DataAssetSheet;
	OnSettingsChanged = InArgs._OnSettingsChanged;

	// DetailsView作成（Settingsカテゴリのみ表示）/ Create DetailsView filtered to Settings category
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bHideSelectionTip = true;
	SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	// Settingsカテゴリ以外のプロパティを非表示 / Hide non-Settings properties
	SettingsDetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateSP(this, &SDataAssetSheetSettingsTab::IsPropertyVisible));

	// プロパティ変更時にテーブル再構築をトリガー / Trigger table rebuild on property change
	SettingsDetailsView->OnFinishedChangingProperties().AddSP(
		this, &SDataAssetSheetSettingsTab::OnPropertyChanged);

	// UDataAssetSheetオブジェクトを設定 / Set the object to edit
	if (UDataAssetSheet* Sheet = DataAssetSheet.Get())
	{
		SettingsDetailsView->SetObject(Sheet);
	}

	ChildSlot
	[
		SettingsDetailsView.ToSharedRef()
	];
}

void SDataAssetSheetSettingsTab::OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// パッケージをダーティに / Mark package dirty
	if (UDataAssetSheet* Sheet = DataAssetSheet.Get())
	{
		Sheet->MarkPackageDirty();
	}

	// テーブル再構築をトリガー / Trigger table rebuild
	OnSettingsChanged.ExecuteIfBound();
}

bool SDataAssetSheetSettingsTab::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	// 自身のカテゴリをチェック / Check this property's category
	const FString Category = PropertyAndParent.Property.GetMetaData(TEXT("Category"));
	if (Category.Contains(TEXT("Settings")))
	{
		return true;
	}

	// TArray要素など、親プロパティのカテゴリもチェック / Also check parent properties (for array elements, etc.)
	for (const FProperty* Parent : PropertyAndParent.ParentProperties)
	{
		const FString ParentCategory = Parent->GetMetaData(TEXT("Category"));
		if (ParentCategory.Contains(TEXT("Settings")))
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

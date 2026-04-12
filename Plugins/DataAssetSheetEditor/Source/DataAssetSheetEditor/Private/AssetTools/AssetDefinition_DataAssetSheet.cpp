// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "AssetDefinition_DataAssetSheet.h"
#include "DataAssetSheet.h"
#include "FDataAssetSheetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_DataAssetSheet"

class UDataAssetSheet;
class FDataAssetSheetEditorToolkit;

FText UAssetDefinition_DataAssetSheet::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "DataAsset Sheet");
}

FLinearColor UAssetDefinition_DataAssetSheet::GetAssetColor() const
{
	// 緑系の色（DataAssetに近い色味）/ Green tone similar to DataAsset
	return FLinearColor(0.2f, 0.8f, 0.4f);
}

TSoftClassPtr<UObject> UAssetDefinition_DataAssetSheet::GetAssetClass() const
{
	return UDataAssetSheet::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DataAssetSheet::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Misc) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_DataAssetSheet::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UDataAssetSheet* Sheet : OpenArgs.LoadObjects<UDataAssetSheet>())
	{
		TSharedRef<FDataAssetSheetEditorToolkit> Toolkit = MakeShared<FDataAssetSheetEditorToolkit>();
		Toolkit->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Sheet);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE

// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_DataAssetSheet.generated.h"

/**
 * UDataAssetSheetのアセット定義 / Asset definition for UDataAssetSheet
 * Content Browserでの表示設定とダブルクリック動作を定義する
 */
UCLASS()
class UAssetDefinition_DataAssetSheet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

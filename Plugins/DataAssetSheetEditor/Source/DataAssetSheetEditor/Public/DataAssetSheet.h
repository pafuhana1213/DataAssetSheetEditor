// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/DataAsset.h"
#include "DataAssetSheet.generated.h"

/**
 * DataAssetのスプレッドシートエディタ設定アセット / Configuration asset for DataAsset spreadsheet editor
 * 対象のDataAssetクラスを保持し、ダブルクリックでスプレッドシートエディタを開く
 */
UCLASS(BlueprintType)
class DATAASSETSHEETEDITOR_API UDataAssetSheet : public UObject
{
	GENERATED_BODY()

public:
	// 対象DataAssetクラス / Target DataAsset class to display in the spreadsheet
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataAssetSheet")
	TSubclassOf<UDataAsset> TargetClass;
};

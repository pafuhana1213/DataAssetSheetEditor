// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DataAssetSheetFactory.generated.h"

/**
 * UDataAssetSheet作成用ファクトリ / Factory for creating UDataAssetSheet assets
 * 作成時にTargetClassを選択するダイアログを表示する
 */
UCLASS()
class UDataAssetSheetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDataAssetSheetFactory();

	// UFactory
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

private:
	// 選択されたDataAssetクラス / Selected DataAsset class from the picker dialog
	UPROPERTY()
	TSubclassOf<UDataAsset> SelectedClass;
};

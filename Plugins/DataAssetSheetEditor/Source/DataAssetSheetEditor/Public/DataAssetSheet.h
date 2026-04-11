// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
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

	// 表示用クラス（派生クラスのプロパティも列に表示したい場合に指定）/ Display class for additional columns from derived class
	// TargetClassの派生クラスを指定すると、派生クラスで追加されたプロパティも列に表示される
	// 未指定の場合はTargetClassのプロパティのみ表示
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataAssetSheet|Settings", meta = (DisplayName = "Display Class"))
	TSubclassOf<UDataAsset> DisplayClass;

	// 全アセット自動表示モード（デフォルトOFF）/ Show all assets of TargetClass automatically
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataAssetSheet|Settings")
	bool bShowAll = false;

	// 手動登録アセットリスト / Manually registered asset list
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataAssetSheet|Settings")
	TArray<TSoftObjectPtr<UDataAsset>> ManualAssets;

	// コレクション参照リスト / Collection references for asset registration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataAssetSheet|Settings")
	TArray<FCollectionReference> RegisteredCollections;
};

// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SampleDataAsset.generated.h"

/**
 * テスト・サンプル用DataAssetクラス / Sample DataAsset class for testing
 * DataAssetSheetEditorの動作確認に使用する基本的なプロパティを持つ
 */
UCLASS(BlueprintType)
class DATAASSETSHEETEDITORSAMPLE_API USampleDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 名前 / Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic")
	FString Name;

	// 説明 / Description
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic")
	FText Description;

	// 識別子 / Identifier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic")
	FName Identifier;

	// 整数値 / Integer value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 IntValue = 0;

	// 浮動小数点値 / Float value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float FloatValue = 0.0f;

	// 有効フラグ / Enabled flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsEnabled = true;
};

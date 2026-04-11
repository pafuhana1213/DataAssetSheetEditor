// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SampleDataAsset.h"
#include "Engine/Texture2D.h"
#include "SampleDataAsset_Child.generated.h"

/**
 * USampleDataAssetの派生クラス / Child class of USampleDataAsset
 * 派生クラスのシート表示テスト用に追加プロパティを持つ
 */
UCLASS(BlueprintType)
class DATAASSETSHEETEDITORSAMPLE_API USampleDataAsset_Child : public USampleDataAsset
{
	GENERATED_BODY()

public:
	// 色 / Color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor Color = FLinearColor::White;

	// アイコン / Icon texture reference
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TSoftObjectPtr<UTexture2D> Icon;

	// レベル / Level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Child Stats")
	int32 Level = 1;

	// 速度 / Speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Child Stats")
	float Speed = 100.0f;

	// タグ / Tag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Child Info")
	FString Tag;
};

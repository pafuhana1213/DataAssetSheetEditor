// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/SoftObjectPath.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class FDataAssetSheetModel;
class FProperty;
class SBox;
struct FDataAssetRowData;

// Object/Texture セル用ウィジェット / Asset thumbnail cell with in-place swap detection.
// 詳細パネルでアセットが差し替わったとき、SListView が行ウィジェットを使い回しても
// Tick で値変化を検知してサムネ/プレースホルダを差し替える。
class SObjectThumbnailCell : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SObjectThumbnailCell) {}
		SLATE_ARGUMENT(TWeakPtr<FDataAssetRowData>, RowData)
		SLATE_ARGUMENT(TWeakPtr<FDataAssetSheetModel>, Model)
		SLATE_ARGUMENT(FProperty*, Property)
		SLATE_ARGUMENT(TSharedPtr<FAssetThumbnailPool>, ThumbnailPool)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EActiveTimerReturnType PollForChange(double InCurrentTime, float InDeltaTime);

	FSoftObjectPath ResolveCurrentAssetPath() const;
	void RebuildContent(const FSoftObjectPath& NewPath);

	TWeakPtr<FDataAssetRowData> WeakRowData;
	TWeakPtr<FDataAssetSheetModel> WeakModel;
	FProperty* Property = nullptr;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> Thumbnail;
	FSoftObjectPath LastPath;
	TSharedPtr<SBox> ContentBox;
};

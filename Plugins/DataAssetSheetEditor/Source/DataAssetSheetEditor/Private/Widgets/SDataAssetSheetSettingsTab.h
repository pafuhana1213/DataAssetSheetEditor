// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UDataAssetSheet;
class IDetailsView;

/**
 * 設定タブウィジェット / Settings tab widget
 * UDataAssetSheetのSettingsカテゴリプロパティをIDetailsViewで表示する
 */
class SDataAssetSheetSettingsTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDataAssetSheetSettingsTab)
		: _DataAssetSheet(nullptr)
	{}
		SLATE_ARGUMENT(UDataAssetSheet*, DataAssetSheet)
		SLATE_EVENT(FSimpleDelegate, OnSettingsChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// プロパティ変更コールバック / Property change callback
	void OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	// プロパティ表示フィルタ / Property visibility filter
	bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;

	TWeakObjectPtr<UDataAssetSheet> DataAssetSheet;
	TSharedPtr<IDetailsView> SettingsDetailsView;
	FSimpleDelegate OnSettingsChanged;
};

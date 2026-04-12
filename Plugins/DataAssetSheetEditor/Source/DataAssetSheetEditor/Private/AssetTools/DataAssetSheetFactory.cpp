// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetFactory.h"
#include "DataAssetSheet.h"
#include "Engine/DataAsset.h"
#include "Kismet2/SClassPickerDialog.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"

// DataAssetのサブクラスのみ表示するフィルタ / Filter to show only DataAsset subclasses
class FDataAssetClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InClass->IsChildOf(UDataAsset::StaticClass())
			&& InClass != UDataAsset::StaticClass()
			&& !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(UDataAsset::StaticClass())
			&& !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	}
};

UDataAssetSheetFactory::UDataAssetSheetFactory()
{
	SupportedClass = UDataAssetSheet::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UDataAssetSheetFactory::ConfigureProperties()
{
	SelectedClass = nullptr;

	// クラス選択ダイアログの設定 / Configure class picker dialog
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.ClassFilters.Add(MakeShared<FDataAssetClassFilter>());

	// ダイアログ表示 / Show the picker dialog
	UClass* ChosenClass = nullptr;
	const FText TitleText = NSLOCTEXT("DataAssetSheetEditor", "PickTargetClass", "Pick Target DataAsset Class");
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UDataAsset::StaticClass());

	if (bPressedOk && ChosenClass)
	{
		SelectedClass = ChosenClass;
		return true;
	}

	return false;
}

UObject* UDataAssetSheetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDataAssetSheet* NewAsset = NewObject<UDataAssetSheet>(InParent, InClass, InName, Flags);
	if (NewAsset && SelectedClass)
	{
		NewAsset->TargetClass = SelectedClass;
	}
	return NewAsset;
}

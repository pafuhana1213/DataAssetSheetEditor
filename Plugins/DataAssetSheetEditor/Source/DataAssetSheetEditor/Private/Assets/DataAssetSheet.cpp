// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheet.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

void UDataAssetSheet::PostLoad()
{
	Super::PostLoad();
	SanitizeClassSettings();
}

namespace
{
	bool IsPathUnderDirectory(FString Path, FString Directory)
	{
		FPaths::NormalizeDirectoryName(Path);
		FPaths::NormalizeDirectoryName(Directory);

		if (!Directory.EndsWith(TEXT("/")))
		{
			Directory += TEXT("/");
		}

		return Path.StartsWith(Directory, ESearchCase::IgnoreCase);
	}

	bool IsEngineModuleName(FName ModuleName)
	{
		FModuleStatus ModuleStatus;
		if (!FModuleManager::Get().QueryModule(ModuleName, ModuleStatus))
		{
			return false;
		}

		return IsPathUnderDirectory(
			FPaths::ConvertRelativePathToFull(ModuleStatus.FilePath),
			FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
	}

	bool IsEngineProvidedClass(const UClass* InClass)
	{
		const UPackage* Package = InClass ? InClass->GetOutermost() : nullptr;
		if (!Package)
		{
			return false;
		}

		const FString PackageName = Package->GetName();
		if (PackageName == TEXT("/Script/Engine"))
		{
			return true;
		}

		FString ModuleNameString = PackageName;
		if (ModuleNameString.RemoveFromStart(TEXT("/Script/")))
		{
			return IsEngineModuleName(FName(*ModuleNameString));
		}

		return false;
	}

	bool HasNonEngineProvidedChildClass(const UClass* InClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* ChildClass = *It;
			if (ChildClass
				&& ChildClass != InClass
				&& ChildClass->IsChildOf(InClass)
				&& !IsEngineProvidedClass(ChildClass))
			{
				return true;
			}
		}

		return false;
	}
}

bool UDataAssetSheet::IsSupportedDataAssetClass(const UClass* InClass, bool bHideEngineClasses)
{
	return InClass
		&& InClass->IsChildOf(UDataAsset::StaticClass())
		&& InClass != UDataAsset::StaticClass()
		&& !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden | CLASS_HideDropDown | CLASS_Transient)
		&& (!bHideEngineClasses || !IsEngineProvidedClass(InClass));
}

bool UDataAssetSheet::IsAllowedDataAssetClass(const UClass* InClass) const
{
	return IsSupportedDataAssetClass(InClass, bHideEngineDataAssetClasses);
}

TArray<UClass*> UDataAssetSheet::GetDisallowedDataAssetClasses() const
{
	TArray<UClass*> DisallowedClasses;
	if (!bHideEngineDataAssetClasses)
	{
		return DisallowedClasses;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class
			&& Class->IsChildOf(UDataAsset::StaticClass())
			&& Class != UDataAsset::StaticClass()
			&& Class != UPrimaryDataAsset::StaticClass()
			&& IsEngineProvidedClass(Class))
		{
			if (!HasNonEngineProvidedChildClass(Class))
			{
				DisallowedClasses.Add(Class);
			}
		}
	}

	return DisallowedClasses;
}

TArray<UClass*> UDataAssetSheet::GetAllowedDisplayClasses() const
{
	TArray<UClass*> AllowedClasses;
	UClass* RequiredBaseClass = TargetClass ? TargetClass.Get() : UDataAsset::StaticClass();
	if (!RequiredBaseClass)
	{
		return AllowedClasses;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class
			&& Class->IsChildOf(RequiredBaseClass)
			&& IsAllowedDataAssetClass(Class))
		{
			AllowedClasses.Add(Class);
		}
	}

	return AllowedClasses;
}

#if WITH_EDITOR
void UDataAssetSheet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SanitizeClassSettings();
}
#endif

void UDataAssetSheet::SanitizeClassSettings()
{
	if (TargetClass && !IsAllowedDataAssetClass(TargetClass.Get()))
	{
		TargetClass = nullptr;
	}

	if (DisplayClass && !IsAllowedDataAssetClass(DisplayClass.Get()))
	{
		DisplayClass = nullptr;
	}

	if (!TargetClass && DisplayClass)
	{
		DisplayClass = nullptr;
	}

	if (TargetClass && DisplayClass && !DisplayClass->IsChildOf(TargetClass))
	{
		DisplayClass = nullptr;
	}
}

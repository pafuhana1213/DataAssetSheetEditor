// Copyright 2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace DataAssetSheetCSV
{
	// CSVのフィールドをエスケープ / Escape a CSV field (handle commas, quotes, newlines)
	FString EscapeField(const FString& InField);

	// CSVコンテンツをレコード（行×フィールド）にパース（クォート内改行対応）
	// Parse CSV content into records (rows of fields), handling multiline quoted fields
	TArray<TArray<FString>> ParseRecords(const FString& InCSVContent);
}

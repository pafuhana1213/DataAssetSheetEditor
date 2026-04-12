// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "DataAssetSheetCSVUtils.h"

namespace DataAssetSheetCSV
{
	FString EscapeField(const FString& InField)
	{
		if (InField.Contains(TEXT(",")) || InField.Contains(TEXT("\"")) || InField.Contains(TEXT("\n")))
		{
			FString Escaped = InField.Replace(TEXT("\""), TEXT("\"\""));
			return FString::Printf(TEXT("\"%s\""), *Escaped);
		}
		return InField;
	}

	TArray<TArray<FString>> ParseRecords(const FString& InCSVContent)
	{
		TArray<TArray<FString>> Records;
		TArray<FString> CurrentRecord;
		FString CurrentField;
		bool bInQuotes = false;

		for (int32 i = 0; i < InCSVContent.Len(); ++i)
		{
			TCHAR Ch = InCSVContent[i];

			if (bInQuotes)
			{
				if (Ch == TEXT('"'))
				{
					// ダブルクォートのエスケープチェック / Check for escaped double quote
					if (i + 1 < InCSVContent.Len() && InCSVContent[i + 1] == TEXT('"'))
					{
						CurrentField += TEXT('"');
						++i;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					CurrentField += Ch;
				}
			}
			else
			{
				if (Ch == TEXT('"'))
				{
					bInQuotes = true;
				}
				else if (Ch == TEXT(','))
				{
					CurrentRecord.Add(CurrentField);
					CurrentField.Empty();
				}
				else if (Ch == TEXT('\r'))
				{
					// CRスキップ / Skip CR
				}
				else if (Ch == TEXT('\n'))
				{
					CurrentRecord.Add(CurrentField);
					CurrentField.Empty();
					Records.Add(MoveTemp(CurrentRecord));
					CurrentRecord.Empty();
				}
				else
				{
					CurrentField += Ch;
				}
			}
		}

		// 最終レコード（末尾改行なしの場合）/ Last record if no trailing newline
		if (!CurrentField.IsEmpty() || !CurrentRecord.IsEmpty())
		{
			CurrentRecord.Add(CurrentField);
			Records.Add(MoveTemp(CurrentRecord));
		}

		return Records;
	}
}

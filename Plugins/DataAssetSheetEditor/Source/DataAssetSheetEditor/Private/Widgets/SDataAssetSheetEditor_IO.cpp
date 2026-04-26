// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetEditor.h"
#include "SDataAssetSheetListView.h"
#include "SObjectThumbnailCell.h"
#include "SDataAssetSheetRow.h"
#include "SDropTargetOverlay.h"
#include "Utils/DataAssetSheetCSVUtils.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "DataAssetSheet.h"
#include "DataAssetSheetModel.h"
#include "DataAssetSheetEditorModule.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SComboButton.h"
#include "UObject/Package.h"
#include "Widgets/SNullWidget.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Input/SSearchBox.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "Editor/EditorEngine.h"
#include "Framework/Commands/GenericCommands.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetThumbnail.h"

DECLARE_CYCLE_STAT(TEXT("ImportCSV"), STAT_DataAssetSheet_ImportCSV, STATGROUP_DataAssetSheet);
DECLARE_CYCLE_STAT(TEXT("ExportCSV"), STAT_DataAssetSheet_ExportCSV, STATGROUP_DataAssetSheet);

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

namespace DataAssetSheetEditorNotify
{
	static void ShowResult(const FText& Message, bool bIsSuccess, float ExpireDuration = 0.0f)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = ExpireDuration > 0.0f ? ExpireDuration : (bIsSuccess ? 3.0f : 6.0f);
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(bIsSuccess
				? SNotificationItem::CS_Success
				: SNotificationItem::CS_Fail);
		}
	}

	static void ShowFailure(const FText& Message)
	{
		ShowResult(Message, false);
	}
}

FReply SDataAssetSheetEditor::OnExportCSVClicked()
{
	SCOPE_CYCLE_COUNTER(STAT_DataAssetSheet_ExportCSV);

	if (Model->GetLoadingState() != EDataAssetSheetLoadingState::Loaded)
	{
		return FReply::Handled();
	}

	// ファイル保存ダイアログ / File save dialog
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	TArray<FString> OutFiles;
	const bool bOpened = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ExportCSVTitle", "Export CSV").ToString(),
		FPaths::ProjectDir(),
		TEXT("DataAssetSheet.csv"),
		TEXT("CSV Files (*.csv)|*.csv"),
		0,
		OutFiles
	);

	if (!bOpened || OutFiles.IsEmpty())
	{
		return FReply::Handled();
	}

	// CSV構築 / Build CSV content
	FString CSVContent;

	// ヘッダー行: AssetPath を一意キーとして第1列に、AssetName は参考列として第2列に出力
	// Header row: AssetPath as unique key (col 0), AssetName as human-readable label (col 1)
	CSVContent += DataAssetSheetCSV::EscapeField(TEXT("AssetPath"));
	CSVContent += TEXT(",");
	CSVContent += DataAssetSheetCSV::EscapeField(TEXT("AssetName"));
	for (FProperty* Prop : Model->GetColumnProperties())
	{
		CSVContent += TEXT(",");
		CSVContent += DataAssetSheetCSV::EscapeField(Prop->GetName());
	}
	CSVContent += TEXT("\n");

	// データ行 / Data rows
	for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
	{
		CSVContent += DataAssetSheetCSV::EscapeField(RowData->AssetPath.ToString());
		CSVContent += TEXT(",");
		CSVContent += DataAssetSheetCSV::EscapeField(RowData->AssetName);

		for (FProperty* Prop : Model->GetColumnProperties())
		{
			CSVContent += TEXT(",");
			const FString* Cached = RowData->CachedDisplayText.Find(Prop->GetFName());
			if (Cached)
			{
				CSVContent += DataAssetSheetCSV::EscapeField(*Cached);
			}
		}
		CSVContent += TEXT("\n");
	}

	// UTF-8 BOM付きで保存（Excel互換）/ Save with UTF-8 BOM for Excel compatibility
	FFileHelper::SaveStringToFile(CSVContent, *OutFiles[0], FFileHelper::EEncodingOptions::ForceUTF8);

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("CSV exported to: %s"), *OutFiles[0]);
	return FReply::Handled();
}


FReply SDataAssetSheetEditor::OnImportCSVClicked()
{
	SCOPE_CYCLE_COUNTER(STAT_DataAssetSheet_ImportCSV);

	if (Model->GetLoadingState() != EDataAssetSheetLoadingState::Loaded)
	{
		return FReply::Handled();
	}

	// ファイル選択ダイアログ / File open dialog
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	TArray<FString> OutFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ImportCSVTitle", "Import CSV").ToString(),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("CSV Files (*.csv)|*.csv"),
		0,
		OutFiles
	);

	if (!bOpened || OutFiles.IsEmpty())
	{
		return FReply::Handled();
	}

	// CSVファイル読み込み / Read CSV file
	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *OutFiles[0]))
	{
		UE_LOG(LogDataAssetSheetEditor, Error, TEXT("Failed to read CSV file: %s"), *OutFiles[0]);
		DataAssetSheetEditorNotify::ShowFailure(FText::Format(
			LOCTEXT("ImportCSVReadFailed", "Failed to read CSV file:\n{0}"),
			FText::FromString(OutFiles[0])));
		return FReply::Handled();
	}

	// レコード単位でパース（クォート内改行対応）/ Parse into records (handles multiline quoted fields)
	TArray<TArray<FString>> Records = DataAssetSheetCSV::ParseRecords(CSVContent);
	if (Records.Num() < 2)
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("CSV file has no data rows"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("ImportCSVNoRows", "CSV file has no data rows"));
		return FReply::Handled();
	}

	// ヘッダー行を取得 / Get header row
	const TArray<FString>& Headers = Records[0];
	if (Headers.Num() < 2)
	{
		UE_LOG(LogDataAssetSheetEditor, Error, TEXT("Invalid CSV header: at least 2 columns required"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("ImportCSVBadHeader", "Invalid CSV header: at least 2 columns required"));
		return FReply::Handled();
	}

	// 第1列で形式を判定 / Detect format from first column
	const bool bPathKeyed = (Headers[0] == TEXT("AssetPath"));
	const bool bLegacyNameKeyed = (Headers[0] == TEXT("AssetName"));
	if (!bPathKeyed && !bLegacyNameKeyed)
	{
		UE_LOG(LogDataAssetSheetEditor, Error, TEXT("Invalid CSV header: first column must be 'AssetPath' or 'AssetName'"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("ImportCSVBadFirstCol", "Invalid CSV header: first column must be 'AssetPath' or 'AssetName'"));
		return FReply::Handled();
	}

	// AssetPath 形式の場合は第2列の AssetName をスキップ / In path-keyed format, skip the AssetName label column
	const int32 PropertyStartCol = bPathKeyed ? 2 : 1;
	if (Headers.Num() < PropertyStartCol)
	{
		UE_LOG(LogDataAssetSheetEditor, Error, TEXT("Invalid CSV header: missing AssetName column"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("ImportCSVMissingName", "Invalid CSV header: missing AssetName column"));
		return FReply::Handled();
	}

	// ヘッダーからプロパティをマッピング / Map headers to properties
	TArray<FProperty*> ImportProperties;
	TArray<FString> SkippedColumns;
	for (int32 i = PropertyStartCol; i < Headers.Num(); ++i)
	{
		FProperty* FoundProp = nullptr;
		for (FProperty* Prop : Model->GetColumnProperties())
		{
			if (Prop->GetName() == Headers[i])
			{
				FoundProp = Prop;
				break;
			}
		}
		ImportProperties.Add(FoundProp); // nullの場合はスキップされる / null entries will be skipped
		if (!FoundProp)
		{
			SkippedColumns.Add(Headers[i]);
		}
	}
	if (!SkippedColumns.IsEmpty())
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("CSV import: skipped %d unknown column(s): %s"),
			SkippedColumns.Num(), *FString::Join(SkippedColumns, TEXT(", ")));
	}

	// 検索用マップを構築 / Build lookup maps for O(1) row matching
	TMap<FSoftObjectPath, TSharedPtr<FDataAssetRowData>> PathToRow;
	TMap<FString, TSharedPtr<FDataAssetRowData>> NameToRow;
	for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
	{
		PathToRow.Add(RowData->AssetPath, RowData);
		if (bLegacyNameKeyed && NameToRow.Contains(RowData->AssetName))
		{
			UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Duplicate asset name '%s' found while importing legacy CSV; first match wins"), *RowData->AssetName);
		}
		else
		{
			NameToRow.Add(RowData->AssetName, RowData);
		}
	}

	// Undo対応のトランザクション開始 / Begin undo transaction
	FScopedTransaction Transaction(LOCTEXT("ImportCSV", "Import CSV"));

	int32 SuccessCount = 0;
	int32 FailCount = 0;
	TArray<TSharedPtr<FDataAssetRowData>> ModifiedRows;

	// データ行をインポート / Import data rows
	for (int32 RecordIndex = 1; RecordIndex < Records.Num(); ++RecordIndex)
	{
		const TArray<FString>& Fields = Records[RecordIndex];
		if (Fields.IsEmpty())
		{
			continue;
		}

		// キーで行を検索 / Find row by key
		TSharedPtr<FDataAssetRowData> FoundRow;
		if (bPathKeyed)
		{
			if (TSharedPtr<FDataAssetRowData>* Ptr = PathToRow.Find(FSoftObjectPath(Fields[0])))
			{
				FoundRow = *Ptr;
			}
		}
		else
		{
			if (TSharedPtr<FDataAssetRowData>* Ptr = NameToRow.Find(Fields[0]))
			{
				FoundRow = *Ptr;
			}
		}

		if (!FoundRow.IsValid() || !FoundRow->IsLoaded())
		{
			UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("CSV import: row '%s' not found or not loaded"), *Fields[0]);
			++FailCount;
			continue;
		}

		UDataAsset* Asset = FoundRow->Asset.Get();
		if (!Asset)
		{
			UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("CSV import: row '%s' has invalid asset reference"), *Fields[0]);
			++FailCount;
			continue;
		}
		Asset->Modify();

		// 各プロパティの値をインポート / Import each property value
		for (int32 PropIndex = 0; PropIndex < ImportProperties.Num(); ++PropIndex)
		{
			int32 FieldIndex = PropIndex + PropertyStartCol;
			if (FieldIndex >= Fields.Num() || !ImportProperties[PropIndex])
			{
				continue;
			}

			FProperty* Prop = ImportProperties[PropIndex];
			// アセットがこのプロパティを所有しない場合はスキップ（基底行 × 派生カラム対策）
			// Skip if asset doesn't own this property (base-class row × derived-class column)
			if (!Model->AssetHasProperty(Asset, Prop))
			{
				continue;
			}
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Asset);
			const FString& ValueStr = Fields[FieldIndex];

			// FTextは特別処理 / Special handling for FText
			if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				TextProp->SetPropertyValue(ValuePtr, FText::FromString(ValueStr));
			}
			else
			{
				Prop->ImportText_Direct(*ValueStr, ValuePtr, Asset, PPF_None);
			}
		}

		Asset->MarkPackageDirty();
		ModifiedRows.Add(FoundRow);
		++SuccessCount;
	}

	// 編集された行のキャッシュを更新 / Refresh display text cache for modified rows
	for (const TSharedPtr<FDataAssetRowData>& Row : ModifiedRows)
	{
		Model->RebuildRowCache(Row);
	}

	// テーブル更新（フィルタ保持）/ Refresh table (preserve filter)
	Model->ReapplyFilter();
	AssetListView->RequestListRefresh();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("CSV import complete: %d succeeded, %d failed"), SuccessCount, FailCount);

	// 結果を通知（失敗・スキップがある場合は警告として表示）/ Notify result, warn on failures or skipped columns
	const bool bHasIssues = FailCount > 0 || !SkippedColumns.IsEmpty();
	const FText ResultText = SkippedColumns.IsEmpty()
		? FText::Format(
			LOCTEXT("ImportCSVResult", "CSV import: {0} succeeded, {1} failed"),
			FText::AsNumber(SuccessCount),
			FText::AsNumber(FailCount))
		: FText::Format(
			LOCTEXT("ImportCSVResultWithSkipped", "CSV import: {0} succeeded, {1} failed ({2} column(s) skipped)"),
			FText::AsNumber(SuccessCount),
			FText::AsNumber(FailCount),
			FText::AsNumber(SkippedColumns.Num()));
	DataAssetSheetEditorNotify::ShowResult(ResultText, !bHasIssues);

	return FReply::Handled();
}


void SDataAssetSheetEditor::CopySelectedRows()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 0 || !Model.IsValid())
	{
		return;
	}

	const TArray<FProperty*>& ColumnProperties = Model->GetColumnProperties();
	FString ClipboardContent;

	// ヘッダー行: AssetPath を一意キーとして第1列に、AssetName は参考列として第2列に出力
	// Header row: AssetPath as unique key (col 0), AssetName as human-readable label (col 1)
	ClipboardContent += TEXT("AssetPath");
	ClipboardContent += TEXT("\t");
	ClipboardContent += TEXT("AssetName");
	for (FProperty* Prop : ColumnProperties)
	{
		ClipboardContent += TEXT("\t");
		ClipboardContent += Prop->GetName();
	}
	ClipboardContent += TEXT("\n");

	// データ行 / Data rows
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (!Item.IsValid() || !Item->IsLoaded())
		{
			continue;
		}

		ClipboardContent += Item->AssetPath.ToString();
		ClipboardContent += TEXT("\t");
		ClipboardContent += Item->AssetName;

		for (FProperty* Prop : ColumnProperties)
		{
			ClipboardContent += TEXT("\t");
			FString ValueText = Model->GetPropertyValueText(Item->Asset.Get(), Prop);
			// タブと改行をエスケープ / Escape tabs and newlines
			ValueText.ReplaceInline(TEXT("\t"), TEXT("\\t"));
			ValueText.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			ValueText.ReplaceInline(TEXT("\r"), TEXT(""));
			ClipboardContent += ValueText;
		}
		ClipboardContent += TEXT("\n");
	}

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardContent);
}


void SDataAssetSheetEditor::PasteOnSelectedRows()
{
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 0 || !Model.IsValid())
	{
		return;
	}

	// クリップボードからTSVを読み取り / Read TSV from clipboard
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	if (ClipboardContent.IsEmpty())
	{
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("PasteEmptyClipboard", "Paste failed: clipboard is empty"));
		return;
	}

	// 行分割 / Split into lines
	TArray<FString> Lines;
	ClipboardContent.ParseIntoArray(Lines, TEXT("\n"), false);
	if (Lines.Num() < 2)
	{
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("PasteNoRows", "Paste failed: clipboard has no data rows"));
		return;
	}

	// ヘッダー行からプロパティをマッピング / Map headers to properties
	TArray<FString> Headers;
	Lines[0].ParseIntoArray(Headers, TEXT("\t"), false);

	// 第1列で形式を判定（ImportCSV と同じパターン）/ Detect format from first column (same pattern as ImportCSV)
	const bool bPathKeyed = (Headers.Num() >= 1) && (Headers[0] == TEXT("AssetPath"));
	const bool bLegacyNameKeyed = (Headers.Num() >= 1) && (Headers[0] == TEXT("AssetName"));
	if (!bPathKeyed && !bLegacyNameKeyed)
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Invalid clipboard data: header must start with 'AssetPath' or 'AssetName'"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("PasteBadHeader", "Paste failed: invalid clipboard header"));
		return;
	}

	// AssetPath 形式の場合は第2列の AssetName をスキップ / In path-keyed format, skip the AssetName label column
	const int32 PropertyStartCol = bPathKeyed ? 2 : 1;
	if (Headers.Num() < PropertyStartCol)
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Invalid clipboard data: missing AssetName column"));
		DataAssetSheetEditorNotify::ShowFailure(LOCTEXT("PasteMissingName", "Paste failed: missing AssetName column"));
		return;
	}

	TArray<FProperty*> PasteProperties;
	for (int32 i = PropertyStartCol; i < Headers.Num(); ++i)
	{
		FProperty* FoundProp = nullptr;
		for (FProperty* Prop : Model->GetColumnProperties())
		{
			if (Prop->GetName() == Headers[i])
			{
				FoundProp = Prop;
				break;
			}
		}
		PasteProperties.Add(FoundProp);
	}

	// 選択行から検索マップを構築 / Build lookup maps from selected items
	TMap<FSoftObjectPath, TSharedPtr<FDataAssetRowData>> PathToSelected;
	TMap<FString, TSharedPtr<FDataAssetRowData>> NameToSelected;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		PathToSelected.Add(Item->AssetPath, Item);
		if (bLegacyNameKeyed && NameToSelected.Contains(Item->AssetName))
		{
			UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("Duplicate asset name '%s' in selection while pasting legacy clipboard; first match wins"), *Item->AssetName);
		}
		else
		{
			NameToSelected.Add(Item->AssetName, Item);
		}
	}

	// Undo対応 / Undo support
	FScopedTransaction Transaction(LOCTEXT("PasteRowData", "Paste Row Data"));

	int32 SuccessCount = 0;

	// データ行を適用 / Apply data rows
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		FString& Line = Lines[LineIndex];
		Line.TrimEndInline();
		if (Line.IsEmpty())
		{
			continue;
		}

		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, TEXT("\t"), false);
		if (Fields.IsEmpty())
		{
			continue;
		}

		// キーで選択行を検索 / Find matching selected row by key
		TSharedPtr<FDataAssetRowData> TargetRow;
		if (bPathKeyed)
		{
			if (TSharedPtr<FDataAssetRowData>* Ptr = PathToSelected.Find(FSoftObjectPath(Fields[0])))
			{
				TargetRow = *Ptr;
			}
		}
		else
		{
			if (TSharedPtr<FDataAssetRowData>* Ptr = NameToSelected.Find(Fields[0]))
			{
				TargetRow = *Ptr;
			}
		}

		// 選択行にマッチしない場合、選択行の順番で適用 / If no key match, apply by selection order
		if (!TargetRow.IsValid())
		{
			int32 SelectionIndex = LineIndex - 1;
			if (SelectionIndex < SelectedItems.Num())
			{
				TargetRow = SelectedItems[SelectionIndex];
			}
		}

		if (!TargetRow.IsValid() || !TargetRow->IsLoaded())
		{
			continue;
		}

		UDataAsset* Asset = TargetRow->Asset.Get();
		if (!Asset)
		{
			continue;
		}
		Asset->Modify();

		for (int32 PropIndex = 0; PropIndex < PasteProperties.Num(); ++PropIndex)
		{
			int32 FieldIndex = PropIndex + PropertyStartCol;
			if (FieldIndex >= Fields.Num() || !PasteProperties[PropIndex])
			{
				continue;
			}

			FProperty* Prop = PasteProperties[PropIndex];
			if (!Model->AssetHasProperty(Asset, Prop))
			{
				continue;
			}
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Asset);
			FString ValueStr = Fields[FieldIndex];

			// エスケープを復元 / Unescape tabs and newlines
			ValueStr.ReplaceInline(TEXT("\\t"), TEXT("\t"));
			ValueStr.ReplaceInline(TEXT("\\n"), TEXT("\n"));

			if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				TextProp->SetPropertyValue(ValuePtr, FText::FromString(ValueStr));
			}
			else
			{
				Prop->ImportText_Direct(*ValueStr, ValuePtr, Asset, PPF_None);
			}
		}

		Asset->MarkPackageDirty();
		Model->RebuildRowCache(TargetRow);
		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		// テーブル更新（フィルタ保持）/ Refresh table (preserve filter)
		Model->ReapplyFilter();
		AssetListView->RequestListRefresh();
	}

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Pasted data to %d asset(s)"), SuccessCount);

	// 結果を通知 / Notify result
	const FText ResultText = FText::Format(
		LOCTEXT("PasteResult", "Paste: {0} succeeded"),
		FText::AsNumber(SuccessCount));
	DataAssetSheetEditorNotify::ShowResult(ResultText, SuccessCount > 0);
}


bool SDataAssetSheetEditor::CanPaste() const
{
	if (!HasSelectedLoadedAsset())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	return !ClipboardContent.IsEmpty();
}


#undef LOCTEXT_NAMESPACE

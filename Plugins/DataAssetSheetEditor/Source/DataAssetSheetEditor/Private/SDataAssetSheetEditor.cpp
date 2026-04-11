// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetEditor.h"
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
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/Package.h"
#include "Widgets/SNullWidget.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Input/SSearchBox.h"
#include "Math/ColorList.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

// テーブル行ウィジェット / Table row widget (private to this translation unit)
class SDataAssetSheetRow : public SMultiColumnTableRow<TSharedPtr<FDataAssetRowData>>
{
public:
	SLATE_BEGIN_ARGS(SDataAssetSheetRow)
		: _IndexInList(0)
	{}
		SLATE_ARGUMENT(int32, IndexInList)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable,
		TSharedPtr<FDataAssetRowData> InRowData, TSharedPtr<FDataAssetSheetModel> InModel,
		TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> InListView)
	{
		RowData = InRowData;
		Model = InModel;
		IndexInList = InArgs._IndexInList;
		OwnerListView = InListView;
		SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), InOwnerTable);
	}

	// 交互背景色 / Alternating row background color
	virtual const FSlateBrush* GetBorder() const override
	{
		static const FSlateColorBrush EvenBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		static const FSlateColorBrush OddBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.03f));
		return (IndexInList % 2 == 0) ? &EvenBrush : &OddBrush;
	}

	// 選択行のテキスト色 / Text color for selected rows
	FSlateColor GetRowTextColor() const
	{
		TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> ListView = OwnerListView;
		if (ListView.IsValid() && RowData.IsValid() && ListView->IsItemSelected(RowData))
		{
			return FSlateColor(FColorList::Orange);
		}
		return FSlateColor::UseForeground();
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override
	{
		// アセット名列（未保存時は * 表示）/ Asset name column with unsaved indicator
		if (ColumnId == "AssetName")
		{
			TWeakPtr<FDataAssetRowData> WeakRowData = RowData;

			return SNew(SBox)
				.Padding(FMargin(4.0f, 2.0f))
				[
					SNew(SHyperlink)
						.Text_Lambda([WeakRowData]() -> FText
						{
							TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
							if (!PinnedRow.IsValid())
							{
								return FText::GetEmpty();
							}
							FString DisplayName = PinnedRow->AssetName;
							if (PinnedRow->IsLoaded())
							{
								UPackage* Package = PinnedRow->Asset->GetOutermost();
								if (Package && Package->IsDirty())
								{
									DisplayName = TEXT("* ") + DisplayName;
								}
							}
							return FText::FromString(DisplayName);
						})
						.OnNavigate(this, &SDataAssetSheetRow::OnAssetNameClicked)
				];
		}

		// プロパティ列 / Property value column
		if (RowData->IsLoaded() && Model.IsValid())
		{
			FProperty* Prop = nullptr;
			for (FProperty* ColProp : Model->GetColumnProperties())
			{
				if (ColProp->GetFName() == ColumnId)
				{
					Prop = ColProp;
					break;
				}
			}

			if (Prop)
			{
				TWeakPtr<FDataAssetRowData> WeakRowData = RowData;
				TWeakPtr<FDataAssetSheetModel> WeakModel = Model;
				FProperty* CapturedProp = Prop;

				// Bool値はチェックボックスで表示（読み取り専用）/ Display bool as read-only checkbox
				if (CastField<FBoolProperty>(Prop))
				{
					return SNew(SBox)
						.Padding(FMargin(4.0f, 2.0f))
						[
							SNew(SCheckBox)
								.IsChecked_Lambda([WeakRowData, CapturedProp]() -> ECheckBoxState
								{
									TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
									if (PinnedRow.IsValid() && PinnedRow->IsLoaded())
									{
										const FBoolProperty* BoolProp = CastField<FBoolProperty>(CapturedProp);
										const void* ValuePtr = BoolProp->ContainerPtrToValuePtr<void>(PinnedRow->Asset.Get());
										return BoolProp->GetPropertyValue(ValuePtr)
											? ECheckBoxState::Checked
											: ECheckBoxState::Unchecked;
									}
									return ECheckBoxState::Unchecked;
								})
								.IsEnabled(false)
						];
				}

				// その他のプロパティはTAttributeでリアルタイム更新 / Other properties with TAttribute for real-time updates
				return SNew(SBox)
					.Padding(FMargin(4.0f, 2.0f))
					[
						SNew(STextBlock)
							.Text_Lambda([WeakRowData, WeakModel, CapturedProp]() -> FText
							{
								TSharedPtr<FDataAssetRowData> PinnedRow = WeakRowData.Pin();
								TSharedPtr<FDataAssetSheetModel> PinnedModel = WeakModel.Pin();
								if (PinnedRow.IsValid() && PinnedModel.IsValid() && PinnedRow->IsLoaded())
								{
									return FText::FromString(
										PinnedModel->GetPropertyValueText(PinnedRow->Asset.Get(), CapturedProp));
								}
								return FText::GetEmpty();
							})
							.ColorAndOpacity(this, &SDataAssetSheetRow::GetRowTextColor)
					];
			}
		}

		// 未ロード時は空表示 / Empty when not loaded
		return SNew(SBox);
	}

	// アセット名クリック → アセットエディタを開く / Open asset editor on click
	void OnAssetNameClicked()
	{
		if (RowData->IsLoaded())
		{
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (AssetEditorSubsystem)
			{
				AssetEditorSubsystem->OpenEditorForAsset(RowData->Asset.Get());
			}
		}
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// 右クリックでコンテキストメニュー / Right-click context menu
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && RowData->IsLoaded())
		{
			FMenuBuilder MenuBuilder(true, nullptr);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("BrowseToAsset", "Browse to Asset"),
				LOCTEXT("BrowseToAssetTooltip", "Show this asset in the Content Browser"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					if (RowData->IsLoaded())
					{
						TArray<FAssetData> AssetDatas;
						AssetDatas.Add(FAssetData(RowData->Asset.Get()));
						FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets(AssetDatas);
					}
				}))
			);

			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked(AsShared(), WidgetPath);
			FSlateApplication::Get().PushMenu(
				AsShared(),
				WidgetPath,
				MenuBuilder.MakeWidget(),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect::ContextMenu
			);

			return FReply::Handled();
		}

		return SMultiColumnTableRow::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

private:
	TSharedPtr<FDataAssetRowData> RowData;
	TSharedPtr<FDataAssetSheetModel> Model;
	TSharedPtr<SListView<TSharedPtr<FDataAssetRowData>>> OwnerListView;
	int32 IndexInList = 0;
};

// --- SDataAssetSheetEditor ---

void SDataAssetSheetEditor::Construct(const FArguments& InArgs)
{
	DataAssetSheet = InArgs._DataAssetSheet;
	Model = MakeShared<FDataAssetSheetModel>();

	// DetailsView作成 / Create the details view panel
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowOptions = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	// プロパティ変更時にテーブルを更新 / Refresh table when properties change in details panel
	DetailsView->OnFinishedChangingProperties().AddSP(this, &SDataAssetSheetEditor::OnDetailsPropertyChanged);

	// HeaderRow初期化 / Initialize header row
	HeaderRow = SNew(SHeaderRow);

	// ListView作成（フィルタ済みリストをソースとする）/ Create list view with filtered list as source
	AssetListView = SNew(SListView<TSharedPtr<FDataAssetRowData>>)
		.ListItemsSource(&Model->GetFilteredRowDataList())
		.OnGenerateRow(this, &SDataAssetSheetEditor::OnGenerateRow)
		.OnSelectionChanged(this, &SDataAssetSheetEditor::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Multi)
		.HeaderRow(HeaderRow);

	// テーブルウィジェット構築（ツールバー + テーブル + オーバーレイ）/ Build table widget
	TableWidget = SNew(SVerticalBox)

		// ツールバー / Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.OnClicked(this, &SDataAssetSheetEditor::OnRefreshClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Text(LOCTEXT("ExportCSV", "Export CSV"))
					.OnClicked(this, &SDataAssetSheetEditor::OnExportCSVClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Text(LOCTEXT("ImportCSV", "Import CSV"))
					.OnClicked(this, &SDataAssetSheetEditor::OnImportCSVClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search..."))
					.OnTextChanged(this, &SDataAssetSheetEditor::OnFilterTextChanged)
			]
		]

		// テーブル + ローディングオーバーレイ / Table with loading overlay
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)

			// テーブル本体 / Table
			+ SOverlay::Slot()
			[
				AssetListView.ToSharedRef()
			]

			// ローディングオーバーレイ / Loading overlay
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
					.Visibility(this, &SDataAssetSheetEditor::GetLoadingVisibility)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(SThrobber)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("Loading", "Loading assets..."))
						]
					]
			]

			// アセット0件メッセージ / Empty state message
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(this, &SDataAssetSheetEditor::GetEmptyMessageText)
					.Visibility(this, &SDataAssetSheetEditor::GetEmptyMessageVisibility)
			]
		];

	// 詳細パネルウィジェット / Details panel widget
	DetailsWidget = DetailsView;

	// ChildSlotは空（タブから参照される）/ ChildSlot empty — widgets accessed via GetTableWidget/GetDetailsWidget
	ChildSlot
	[
		SNullWidget::NullWidget
	];

	// AssetRegistryイベント登録 / Register asset registry events
	RegisterAssetRegistryEvents();

	// Hot Reload対策 / Register hot reload handler to rebuild with fresh FProperty pointers
	FCoreUObjectDelegates::ReloadCompleteDelegate.AddSP(this, &SDataAssetSheetEditor::OnReloadComplete);

	// 初期テーブル構築 / Initial table build
	RebuildTable();
}

SDataAssetSheetEditor::~SDataAssetSheetEditor()
{
	FCoreUObjectDelegates::ReloadCompleteDelegate.RemoveAll(this);
	UnregisterAssetRegistryEvents();

	if (Model.IsValid())
	{
		Model->CancelLoading();
	}
}

void SDataAssetSheetEditor::OnSettingsChanged()
{
	RebuildTable();
}

TSharedRef<SWidget> SDataAssetSheetEditor::GetTableWidget() const
{
	return TableWidget.ToSharedRef();
}

TSharedRef<SWidget> SDataAssetSheetEditor::GetDetailsWidget() const
{
	return DetailsWidget.ToSharedRef();
}

void SDataAssetSheetEditor::RebuildTable()
{
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet || !Sheet->TargetClass)
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("DataAssetSheet or TargetClass is null"));
		return;
	}

	UClass* TargetClass = Sheet->TargetClass;

	// 登録設定に基づいてアセットを検索 / Discover assets based on registration settings
	Model->DiscoverAssets(TargetClass, Sheet->bShowAll, Sheet->ManualAssets, Sheet->RegisteredCollections);
	Model->BuildColumnList(TargetClass);

	// ヘッダー行更新 / Rebuild header
	RebuildHeaderRow();

	// フィルタ適用（空フィルタ = 全行表示）/ Apply filter (empty = show all)
	Model->ApplyFilter(FString());

	// テーブル更新（アセット名のみ表示）/ Refresh table (asset names only at this point)
	AssetListView->RequestListRefresh();

	// 詳細パネルクリア / Clear details view
	DetailsView->SetObject(nullptr);

	// 非同期ロード開始 / Start async loading
	StartAsyncLoad();
}

void SDataAssetSheetEditor::RebuildHeaderRow()
{
	HeaderRow->ClearColumns();

	// アセット名列（常に先頭）/ Asset name column (always first)
	HeaderRow->AddColumn(
		SHeaderRow::Column("AssetName")
		.DefaultLabel(LOCTEXT("AssetName", "Asset Name"))
		.FillWidth(1.0f)
	);

	// プロパティ列を動的に追加 / Add property columns dynamically
	for (FProperty* Prop : Model->GetColumnProperties())
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(Prop->GetFName())
			.DefaultLabel(FText::FromName(Prop->GetFName()))
			.FillWidth(1.0f)
		);
	}
}

TSharedRef<ITableRow> SDataAssetSheetEditor::OnGenerateRow(TSharedPtr<FDataAssetRowData> InRowData, const TSharedRef<STableViewBase>& OwnerTable)
{
	const int32 Index = Model->GetFilteredRowDataList().IndexOfByKey(InRowData);
	return SNew(SDataAssetSheetRow, OwnerTable, InRowData, Model, AssetListView)
		.IndexInList(Index);
}

void SDataAssetSheetEditor::OnSelectionChanged(TSharedPtr<FDataAssetRowData> InRowData, ESelectInfo::Type SelectInfo)
{
	// 選択されたアセットを詳細パネルに表示 / Show selected assets in details panel
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();

	TArray<UObject*> SelectedObjects;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid() && Item->IsLoaded())
		{
			SelectedObjects.Add(Item->Asset.Get());
		}
	}

	if (SelectedObjects.Num() > 0)
	{
		DetailsView->SetObjects(SelectedObjects);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

FReply SDataAssetSheetEditor::OnRefreshClicked()
{
	RebuildTable();
	return FReply::Handled();
}

void SDataAssetSheetEditor::StartAsyncLoad()
{
	Model->RequestAsyncLoad(FOnAssetsLoaded::CreateSP(this, &SDataAssetSheetEditor::OnAsyncLoadCompleted));
}

void SDataAssetSheetEditor::OnAsyncLoadCompleted()
{
	// フィルタ再適用（ロード後のプロパティ値でフィルタ可能に）/ Re-apply filter with loaded property values
	Model->ApplyFilter(FString());

	// テーブルを更新してプロパティ値を表示 / Refresh table to show property values
	AssetListView->RequestListRefresh();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Async load completed, table refreshed"));
}

EVisibility SDataAssetSheetEditor::GetLoadingVisibility() const
{
	return (Model.IsValid() && Model->GetLoadingState() == EDataAssetSheetLoadingState::Loading)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SDataAssetSheetEditor::GetTableVisibility() const
{
	return EVisibility::Visible;
}

EVisibility SDataAssetSheetEditor::GetEmptyMessageVisibility() const
{
	if (!Model.IsValid())
	{
		return EVisibility::Collapsed;
	}

	// ロード完了後かつ行データが空の場合のみ表示 / Show only when loaded and empty
	return (Model->GetLoadingState() == EDataAssetSheetLoadingState::Loaded && Model->GetRowDataList().IsEmpty())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FText SDataAssetSheetEditor::GetEmptyMessageText() const
{
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (Sheet && !Sheet->bShowAll)
	{
		return LOCTEXT("NoAssetsSettings", "No assets registered.\nUse the Settings tab to add assets, or enable Show All.");
	}
	return LOCTEXT("NoAssets", "No assets found. Create DataAssets of the target class in Content Browser.");
}

void SDataAssetSheetEditor::OnDetailsPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// 詳細パネルでの編集をテーブルに即座に反映 / Reflect details panel edits in the table immediately
	AssetListView->RequestListRefresh();
}

void SDataAssetSheetEditor::OnFilterTextChanged(const FText& InFilterText)
{
	Model->ApplyFilter(InFilterText.ToString());
	AssetListView->RequestListRefresh();
}

// CSVのフィールドをエスケープ / Escape a CSV field (handle commas, quotes, newlines)
static FString EscapeCSVField(const FString& InField)
{
	if (InField.Contains(TEXT(",")) || InField.Contains(TEXT("\"")) || InField.Contains(TEXT("\n")))
	{
		FString Escaped = InField.Replace(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}
	return InField;
}

FReply SDataAssetSheetEditor::OnExportCSVClicked()
{
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

	// ヘッダー行 / Header row
	CSVContent += EscapeCSVField(TEXT("AssetName"));
	for (FProperty* Prop : Model->GetColumnProperties())
	{
		CSVContent += TEXT(",");
		CSVContent += EscapeCSVField(Prop->GetName());
	}
	CSVContent += TEXT("\n");

	// データ行 / Data rows
	for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
	{
		CSVContent += EscapeCSVField(RowData->AssetName);

		for (FProperty* Prop : Model->GetColumnProperties())
		{
			CSVContent += TEXT(",");
			if (RowData->IsLoaded())
			{
				FString ValueText = Model->GetPropertyValueText(RowData->Asset.Get(), Prop);
				CSVContent += EscapeCSVField(ValueText);
			}
		}
		CSVContent += TEXT("\n");
	}

	// UTF-8 BOM付きで保存 / Save with UTF-8 BOM for Excel compatibility
	FFileHelper::SaveStringToFile(CSVContent, *OutFiles[0], FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// BOMを手動追加 / Manually prepend BOM
	TArray<uint8> FileData;
	FFileHelper::LoadFileToArray(FileData, *OutFiles[0]);
	TArray<uint8> BOMData;
	BOMData.Add(0xEF);
	BOMData.Add(0xBB);
	BOMData.Add(0xBF);
	BOMData.Append(FileData);
	FFileHelper::SaveArrayToFile(BOMData, *OutFiles[0]);

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("CSV exported to: %s"), *OutFiles[0]);
	return FReply::Handled();
}

// CSVの1行をパース / Parse a single CSV line respecting quoted fields
static TArray<FString> ParseCSVLine(const FString& InLine)
{
	TArray<FString> Fields;
	FString CurrentField;
	bool bInQuotes = false;

	for (int32 i = 0; i < InLine.Len(); ++i)
	{
		TCHAR Ch = InLine[i];

		if (bInQuotes)
		{
			if (Ch == TEXT('"'))
			{
				// ダブルクォートのエスケープチェック / Check for escaped double quote
				if (i + 1 < InLine.Len() && InLine[i + 1] == TEXT('"'))
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
				Fields.Add(CurrentField);
				CurrentField.Empty();
			}
			else
			{
				CurrentField += Ch;
			}
		}
	}
	Fields.Add(CurrentField);
	return Fields;
}

FReply SDataAssetSheetEditor::OnImportCSVClicked()
{
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
		return FReply::Handled();
	}

	// 行に分割 / Split into lines
	TArray<FString> Lines;
	CSVContent.ParseIntoArrayLines(Lines);
	if (Lines.Num() < 2)
	{
		UE_LOG(LogDataAssetSheetEditor, Warning, TEXT("CSV file has no data rows"));
		return FReply::Handled();
	}

	// ヘッダー行をパース / Parse header row
	TArray<FString> Headers = ParseCSVLine(Lines[0]);
	if (Headers.Num() < 2 || Headers[0] != TEXT("AssetName"))
	{
		UE_LOG(LogDataAssetSheetEditor, Error, TEXT("Invalid CSV header: first column must be 'AssetName'"));
		return FReply::Handled();
	}

	// ヘッダーからプロパティをマッピング / Map headers to properties
	TArray<FProperty*> ImportProperties;
	for (int32 i = 1; i < Headers.Num(); ++i)
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
	}

	// Undo対応のトランザクション開始 / Begin undo transaction
	FScopedTransaction Transaction(LOCTEXT("ImportCSV", "Import CSV"));

	int32 SuccessCount = 0;
	int32 FailCount = 0;

	// データ行をインポート / Import data rows
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Fields = ParseCSVLine(Lines[LineIndex]);
		if (Fields.IsEmpty())
		{
			continue;
		}

		FString AssetName = Fields[0];

		// アセット名で行を検索 / Find row by asset name
		TSharedPtr<FDataAssetRowData>* FoundRow = nullptr;
		for (TSharedPtr<FDataAssetRowData>& RowData : Model->GetMutableRowDataList())
		{
			if (RowData->AssetName == AssetName)
			{
				FoundRow = &RowData;
				break;
			}
		}

		if (!FoundRow || !(*FoundRow)->IsLoaded())
		{
			++FailCount;
			continue;
		}

		UDataAsset* Asset = (*FoundRow)->Asset.Get();
		Asset->Modify();

		// 各プロパティの値をインポート / Import each property value
		for (int32 PropIndex = 0; PropIndex < ImportProperties.Num(); ++PropIndex)
		{
			int32 FieldIndex = PropIndex + 1; // +1 for AssetName column
			if (FieldIndex >= Fields.Num() || !ImportProperties[PropIndex])
			{
				continue;
			}

			FProperty* Prop = ImportProperties[PropIndex];
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
		++SuccessCount;
	}

	// テーブル更新 / Refresh table
	Model->ApplyFilter(FString());
	AssetListView->RequestListRefresh();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("CSV import complete: %d succeeded, %d failed"), SuccessCount, FailCount);
	return FReply::Handled();
}

void SDataAssetSheetEditor::OnReloadComplete(EReloadCompleteReason Reason)
{
	// Hot Reload後はFProperty*が無効になるためテーブルを完全再構築 / Rebuild after hot reload to get fresh FProperty pointers
	RebuildTable();
}

void SDataAssetSheetEditor::RegisterAssetRegistryEvents()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnAssetAdded().AddSP(this, &SDataAssetSheetEditor::OnAssetAdded);
	AssetRegistry.OnAssetRemoved().AddSP(this, &SDataAssetSheetEditor::OnAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddSP(this, &SDataAssetSheetEditor::OnAssetRenamed);
}

void SDataAssetSheetEditor::UnregisterAssetRegistryEvents()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.OnAssetAdded().RemoveAll(this);
		AssetRegistry.OnAssetRemoved().RemoveAll(this);
		AssetRegistry.OnAssetRenamed().RemoveAll(this);
	}
}

bool SDataAssetSheetEditor::IsTargetAsset(const FAssetData& AssetData) const
{
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet || !Sheet->TargetClass)
	{
		return false;
	}

	// アセットのクラスが対象クラスまたはそのサブクラスか判定 / Check if the asset class matches
	UClass* AssetClass = AssetData.GetClass();
	if (AssetClass && AssetClass->IsChildOf(Sheet->TargetClass))
	{
		return true;
	}

	return false;
}

void SDataAssetSheetEditor::OnAssetAdded(const FAssetData& AssetData)
{
	if (!IsTargetAsset(AssetData))
	{
		return;
	}

	// bShowAll=false の場合は自動追加しない / Do not auto-add when bShowAll is false
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (Sheet && !Sheet->bShowAll)
	{
		return;
	}

	// 新しいRowDataを追加 / Add new row data
	TSharedPtr<FDataAssetRowData> NewRowData = MakeShared<FDataAssetRowData>();
	NewRowData->AssetPath = AssetData.GetSoftObjectPath();
	NewRowData->AssetName = AssetData.AssetName.ToString();

	// 既にロード済みならアセット参照をセット / Set asset reference if already loaded
	if (UObject* LoadedObject = AssetData.GetSoftObjectPath().ResolveObject())
	{
		if (UDataAsset* DataAsset = Cast<UDataAsset>(LoadedObject))
		{
			NewRowData->Asset = DataAsset;
		}
	}

	Model->GetMutableRowDataList().Add(NewRowData);
	AssetListView->RequestListRefresh();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Asset added: %s"), *AssetData.AssetName.ToString());
}

void SDataAssetSheetEditor::OnAssetRemoved(const FAssetData& AssetData)
{
	if (!IsTargetAsset(AssetData))
	{
		return;
	}

	FSoftObjectPath RemovedPath = AssetData.GetSoftObjectPath();

	// ManualAssetsリストからも自動除外 / Auto-remove from ManualAssets list
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (Sheet)
	{
		Sheet->ManualAssets.RemoveAll([&RemovedPath](const TSoftObjectPtr<UDataAsset>& SoftPtr)
		{
			return SoftPtr.ToSoftObjectPath() == RemovedPath;
		});
	}

	TArray<TSharedPtr<FDataAssetRowData>>& RowDataList = Model->GetMutableRowDataList();

	RowDataList.RemoveAll([&RemovedPath](const TSharedPtr<FDataAssetRowData>& RowData)
	{
		return RowData->AssetPath == RemovedPath;
	});

	// 詳細パネルの選択が削除されたアセットならクリア / Clear details if removed asset was selected
	DetailsView->SetObject(nullptr);
	AssetListView->RequestListRefresh();

	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Asset removed: %s"), *AssetData.AssetName.ToString());
}

void SDataAssetSheetEditor::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	if (!IsTargetAsset(AssetData))
	{
		return;
	}

	// リネームされたアセットのRowDataを更新 / Update row data for renamed asset
	FSoftObjectPath OldPath(OldObjectPath);
	for (TSharedPtr<FDataAssetRowData>& RowData : Model->GetMutableRowDataList())
	{
		if (RowData->AssetPath == OldPath)
		{
			RowData->AssetPath = AssetData.GetSoftObjectPath();
			RowData->AssetName = AssetData.AssetName.ToString();
			break;
		}
	}

	AssetListView->RequestListRefresh();
	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Asset renamed: %s -> %s"), *OldObjectPath, *AssetData.AssetName.ToString());
}

#undef LOCTEXT_NAMESPACE

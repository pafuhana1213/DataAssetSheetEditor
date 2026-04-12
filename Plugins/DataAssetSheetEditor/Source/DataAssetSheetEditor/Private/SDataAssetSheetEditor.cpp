// Copyright 2026 pafuhana1213. All Rights Reserved.

#include "SDataAssetSheetEditor.h"
#include "SDataAssetSheetListView.h"
#include "SObjectThumbnailCell.h"
#include "SDataAssetSheetRow.h"
#include "SDropTargetOverlay.h"
#include "DataAssetSheetCSVUtils.h"
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

// 列幅の既定値と最小値 / Default and minimum column widths (pixels)
static constexpr float DefaultColumnWidth = 150.0f;
static constexpr float MinColumnWidth = 32.0f;

#define LOCTEXT_NAMESPACE "SDataAssetSheetEditor"

// --- SDataAssetSheetEditor ---

void SDataAssetSheetEditor::Construct(const FArguments& InArgs)
{
	DataAssetSheet = InArgs._DataAssetSheet;
	Model = MakeShared<FDataAssetSheetModel>();

	// セル用サムネイルプール / Shared thumbnail pool for Object/Texture cells
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(128);

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

	// 垂直スクロールバーを外部化する / External vertical scrollbar so it stays outside the horizontal scroll area
	TSharedRef<SScrollBar> VScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	// ListView作成（フィルタ済みリストをソースとする）/ Create list view with filtered list as source
	// 行高は各セルの SBox::MinDesiredHeight で底上げする (ItemHeight は Tile 用で非推奨)
	AssetListView = SNew(SDataAssetSheetListView)
		.ListItemsSource(&Model->GetFilteredRowDataList())
		.OnGenerateRow(this, &SDataAssetSheetEditor::OnGenerateRow)
		.OnSelectionChanged(this, &SDataAssetSheetEditor::OnSelectionChanged)
		.OnContextMenuOpening(this, &SDataAssetSheetEditor::OnConstructContextMenu)
		.OnMouseButtonDoubleClick(this, &SDataAssetSheetEditor::OnRowDoubleClicked)
		.SelectionMode(ESelectionMode::Multi)
		.ExternalScrollbar(VScrollBar)
		.HeaderRow(HeaderRow);

	// テーブルウィジェット構築（ドロップターゲット + ツールバー + テーブル + オーバーレイ）/ Build table widget with drop target
	TSharedRef<SDropTargetOverlay> DropTarget = SNew(SDropTargetOverlay)
		.Content()
		[
			SNew(SVerticalBox)

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
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(LOCTEXT("Columns", "Columns"))
					]
					.OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
					{
						TSharedPtr<SWidget> Menu = OnConstructHeaderContextMenu();
						return Menu.IsValid() ? Menu.ToSharedRef() : SNullWidget::NullWidget;
					})
					.ToolTipText(LOCTEXT("ColumnsTooltip", "Show/Hide columns"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search..."))
					.OnTextChanged(this, &SDataAssetSheetEditor::OnFilterTextChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						if (!Model.IsValid())
						{
							return FText::GetEmpty();
						}
						const int32 FilteredCount = Model->GetFilteredRowDataList().Num();
						const int32 TotalCount = Model->GetRowDataList().Num();
						if (Model->IsFiltered())
						{
							return FText::Format(
								LOCTEXT("RowCountFiltered", "{0} / {1}"),
								FilteredCount, TotalCount);
						}
						return FText::Format(
							LOCTEXT("RowCount", "{0} assets"), TotalCount);
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		// テーブル + ローディングオーバーレイ / Table with loading overlay
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)

			// テーブル本体 / Table (horizontal scroll box + external vertical scrollbar)
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(HorizontalScrollBox, SScrollBox)
						.Orientation(Orient_Horizontal)
						.ScrollBarAlwaysVisible(false)
						.ConsumeMouseWheel(EConsumeMouseWheel::Never)
					+ SScrollBox::Slot()
					[
						SNew(SBox)
							.WidthOverride(TAttribute<FOptionalSize>::CreateSP(
								this, &SDataAssetSheetEditor::GetTableContentWidth))
							[
								AssetListView.ToSharedRef()
							]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					VScrollBar
				]
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
		]
	];

	DropTarget->OnDragOverDelegate.BindSP(this, &SDataAssetSheetEditor::HandleDragOver);
	DropTarget->OnDropDelegate.BindSP(this, &SDataAssetSheetEditor::HandleDrop);
	TableWidget = DropTarget;

	// Shift+マウスホイールで水平スクロールするため、ListViewへScrollBoxを紐付け
	// Wire ListView → horizontal SScrollBox so Shift+Wheel can scroll horizontally
	AssetListView->SetHorizontalScrollBox(HorizontalScrollBox);

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

	// レイアウトデータ読み込み / Load layout data (column widths, hidden columns)
	LoadLayoutData();

	// 初期テーブル構築 / Initial table build
	RebuildTable();
}

SDataAssetSheetEditor::~SDataAssetSheetEditor()
{
	// レイアウトデータ保存 / Save layout data on close
	SaveLayoutData();

	FCoreUObjectDelegates::ReloadCompleteDelegate.RemoveAll(this);
	UnregisterAssetRegistryEvents();

	if (Model.IsValid())
	{
		Model->CancelLoading();
	}

	ThumbnailPool.Reset();
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

	// DisplayClassが設定されていればそのクラスのプロパティも列に表示 / Use DisplayClass for columns if set
	UClass* ColumnClass = (Sheet->DisplayClass && Sheet->DisplayClass->IsChildOf(TargetClass))
		? Sheet->DisplayClass.Get()
		: TargetClass;
	Model->BuildColumnList(ColumnClass);

	// ヘッダー行更新 / Rebuild header
	RebuildHeaderRow();

	// フィルタ再適用（SearchBoxのテキストと状態を一致させる）/ Re-apply filter to keep in sync with SearchBox
	Model->ReapplyFilter();

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
	{
		FName AssetNameCol("AssetName");
		SHeaderRow::FColumn::FArguments ColArgs = SHeaderRow::Column(AssetNameCol)
			.DefaultLabel(LOCTEXT("AssetName", "Asset Name"))
			.SortMode(TAttribute<EColumnSortMode::Type>::CreateSP(this, &SDataAssetSheetEditor::GetSortModeForColumn, AssetNameCol))
			.OnSort(FOnSortModeChanged::CreateSP(this, &SDataAssetSheetEditor::OnSortModeChanged))
			.OnWidthChanged(FOnWidthChanged::CreateSP(this, &SDataAssetSheetEditor::OnColumnWidthChanged, AssetNameCol));
		ApplyColumnWidth(ColArgs, AssetNameCol);
		HeaderRow->AddColumn(ColArgs);
	}

	// プロパティ列を動的に追加 / Add property columns dynamically
	for (FProperty* Prop : Model->GetColumnProperties())
	{
		FName ColName = Prop->GetFName();

		// 非表示カラムはスキップ / Skip hidden columns
		if (HiddenColumns.Contains(ColName))
		{
			continue;
		}

		// ツールチップ: UPROPERTYのToolTipメタデータがあればそれを、なければ型名を表示
		FText ColumnTooltip = Prop->GetToolTipText();
		if (ColumnTooltip.IsEmpty())
		{
			ColumnTooltip = FText::FromString(Prop->GetCPPType());
		}

		SHeaderRow::FColumn::FArguments ColArgs = SHeaderRow::Column(ColName)
			.DefaultLabel(FText::FromName(ColName))
			.ToolTipText(ColumnTooltip)
			.SortMode(TAttribute<EColumnSortMode::Type>::CreateSP(this, &SDataAssetSheetEditor::GetSortModeForColumn, ColName))
			.OnSort(FOnSortModeChanged::CreateSP(this, &SDataAssetSheetEditor::OnSortModeChanged))
			.OnWidthChanged(FOnWidthChanged::CreateSP(this, &SDataAssetSheetEditor::OnColumnWidthChanged, ColName));
		ApplyColumnWidth(ColArgs, ColName);
		HeaderRow->AddColumn(ColArgs);
	}
}

FOptionalSize SDataAssetSheetEditor::GetTableContentWidth() const
{
	// 表示中列の幅合計 / Sum of visible column widths
	float Total = 0.0f;
	if (HeaderRow.IsValid())
	{
		for (const SHeaderRow::FColumn& Col : HeaderRow->GetColumns())
		{
			if (HiddenColumns.Contains(Col.ColumnId))
			{
				continue;
			}
			const float* W = ColumnWidths.Find(Col.ColumnId);
			Total += (W ? *W : DefaultColumnWidth);
		}
	}
	return FOptionalSize(Total);
}

void SDataAssetSheetEditor::ApplyColumnWidth(SHeaderRow::FColumn::FArguments& OutArgs, FName ColumnId) const
{
	// 常に Manual モードで TAttribute をバインドし、ドラッグ後の幅変更が
	// 即座に再描画されるようにする（OnWidthChanged が bound だと内部 Width
	// は更新されないため、属性側を動的に読む必要がある）
	// Always use Manual mode with a bound attribute so that dragging updates
	// the visual width (when OnWidthChanged is bound, SHeaderRow does not
	// touch its internal Width — the attribute must be dynamic).
	TWeakPtr<const SDataAssetSheetEditor> WeakSelf = SharedThis(this);
	OutArgs.ManualWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda(
		[WeakSelf, ColumnId]() -> float
		{
			if (TSharedPtr<const SDataAssetSheetEditor> Pinned = WeakSelf.Pin())
			{
				if (const float* W = Pinned->ColumnWidths.Find(ColumnId))
				{
					return *W;
				}
			}
			return DefaultColumnWidth;
		})));
}

void SDataAssetSheetEditor::OnColumnWidthChanged(float NewWidth, FName ColumnId)
{
	// 極端に小さい値は無視（誤操作対策）/ Reject pathological tiny widths
	if (NewWidth < MinColumnWidth)
	{
		NewWidth = MinColumnWidth;
	}
	ColumnWidths.Add(ColumnId, NewWidth);
}

TSharedRef<ITableRow> SDataAssetSheetEditor::OnGenerateRow(TSharedPtr<FDataAssetRowData> InRowData, const TSharedRef<STableViewBase>& OwnerTable)
{
	const int32 Index = Model->GetFilteredRowDataList().IndexOfByKey(InRowData);
	return SNew(SDataAssetSheetRow, OwnerTable, InRowData, Model, AssetListView, ThumbnailPool)
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
	// 保存済みのソート状態を一度だけ復元 / Restore persisted sort state once after first load
	if (SavedSortMode != EColumnSortMode::None && !SavedSortColumnId.IsNone())
	{
		Model->SortByColumn(SavedSortColumnId, SavedSortMode);
		SavedSortColumnId = NAME_None;
		SavedSortMode = EColumnSortMode::None;
	}

	// フィルタ再適用（ロード後のプロパティ値でフィルタ可能に）/ Re-apply filter with loaded property values
	Model->ReapplyFilter();

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

FReply SDataAssetSheetEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Enter: 選択中アセットを既定エディタで開く / Open selected assets in default editor
	if (Key == EKeys::Enter)
	{
		OpenSelectedAssets();
		return FReply::Handled();
	}

	// F2: 詳細パネルにフォーカス / Focus details panel for editing
	if (Key == EKeys::F2)
	{
		if (DetailsView.IsValid())
		{
			FSlateApplication::Get().SetAllUserFocus(DetailsView.ToSharedRef(), EFocusCause::Navigation);
		}
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SDataAssetSheetEditor::OnRowDoubleClicked(TSharedPtr<FDataAssetRowData> /*InRowData*/)
{
	OpenSelectedAssets();
}

void SDataAssetSheetEditor::OpenSelectedAssets()
{
	if (!AssetListView.IsValid())
	{
		return;
	}

	TArray<UObject*> AssetsToOpen;
	for (const TSharedPtr<FDataAssetRowData>& Item : AssetListView->GetSelectedItems())
	{
		if (Item.IsValid() && Item->IsLoaded())
		{
			AssetsToOpen.Add(Item->Asset.Get());
		}
	}

	if (!AssetsToOpen.IsEmpty())
	{
		if (UAssetEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
		{
			Subsystem->OpenEditorForAssets(AssetsToOpen);
		}
	}
}

void SDataAssetSheetEditor::OnDetailsPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// 編集された行のキャッシュを更新（フィルタ/ソート整合性のため）
	// Refresh display text cache for edited rows so filter/sort stay consistent
	if (Model.IsValid())
	{
		for (const TSharedPtr<FDataAssetRowData>& Item : AssetListView->GetSelectedItems())
		{
			Model->RebuildRowCache(Item);
		}
	}

	// 詳細パネルでの編集をテーブルに即座に反映 / Reflect details panel edits in the table immediately
	AssetListView->RequestListRefresh();
}

void SDataAssetSheetEditor::OnFilterTextChanged(const FText& InFilterText)
{
	Model->ApplyFilter(InFilterText.ToString());
	AssetListView->RequestListRefresh();
}

void SDataAssetSheetEditor::OnSortModeChanged(EColumnSortPriority::Type /*SortPriority*/, const FName& ColumnId, EColumnSortMode::Type SortMode)
{
	Model->SortByColumn(ColumnId, SortMode);
	AssetListView->RequestListRefresh();
}

EColumnSortMode::Type SDataAssetSheetEditor::GetSortModeForColumn(FName ColumnId) const
{
	if (Model.IsValid() && Model->GetSortColumnId() == ColumnId)
	{
		return Model->GetSortMode();
	}
	return EColumnSortMode::None;
}

void SDataAssetSheetEditor::OnReloadComplete(EReloadCompleteReason Reason)
{
	// Hot Reload後はFProperty*が無効になるためテーブルを完全再構築 / Rebuild after hot reload to get fresh FProperty pointers
	RebuildTable();
}

void SDataAssetSheetEditor::LoadLayoutData()
{
	LayoutData.Reset();
	HiddenColumns.Empty();
	ColumnWidths.Empty();
	SavedSortColumnId = NAME_None;
	SavedSortMode = EColumnSortMode::None;

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return;
	}

	const FString LayoutFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataAssetSheetLayout") / Sheet->GetName() + TEXT(".json");

	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *LayoutFilename))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonText);
		FJsonSerializer::Deserialize(JsonReader, LayoutData);
	}

	if (!LayoutData.IsValid())
	{
		return;
	}

	// HiddenColumnsを復元 / Restore hidden columns
	if (LayoutData->HasField(TEXT("HiddenColumns")))
	{
		const TArray<TSharedPtr<FJsonValue>>& HiddenArray = LayoutData->GetArrayField(TEXT("HiddenColumns"));
		for (const TSharedPtr<FJsonValue>& Value : HiddenArray)
		{
			HiddenColumns.Add(FName(*Value->AsString()));
		}
	}

	// 列幅を復元（極端に小さい値は破棄）/ Restore column widths, dropping pathological values
	if (LayoutData->HasField(TEXT("ColumnWidths")))
	{
		const TSharedPtr<FJsonObject>& WidthsObj = LayoutData->GetObjectField(TEXT("ColumnWidths"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : WidthsObj->Values)
		{
			const float Width = static_cast<float>(Pair.Value->AsNumber());
			if (Width >= MinColumnWidth)
			{
				ColumnWidths.Add(FName(*Pair.Key), Width);
			}
		}
	}

	// ソート状態を復元 / Restore sort state
	if (LayoutData->HasField(TEXT("SortColumn")))
	{
		SavedSortColumnId = FName(*LayoutData->GetStringField(TEXT("SortColumn")));
	}
	if (LayoutData->HasField(TEXT("SortMode")))
	{
		SavedSortMode = static_cast<EColumnSortMode::Type>(LayoutData->GetIntegerField(TEXT("SortMode")));
	}
}

void SDataAssetSheetEditor::SaveLayoutData()
{
	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return;
	}

	if (!LayoutData.IsValid())
	{
		LayoutData = MakeShareable(new FJsonObject());
	}

	// HiddenColumnsを保存 / Save hidden columns
	TArray<TSharedPtr<FJsonValue>> HiddenArray;
	for (const FName& ColName : HiddenColumns)
	{
		HiddenArray.Add(MakeShareable(new FJsonValueString(ColName.ToString())));
	}
	LayoutData->SetArrayField(TEXT("HiddenColumns"), HiddenArray);

	// 現在の列幅を HeaderRow から取得して保存 / Capture current column widths from HeaderRow
	if (HeaderRow.IsValid())
	{
		for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
		{
			ColumnWidths.Add(Column.ColumnId, Column.GetWidth());
		}
	}

	TSharedPtr<FJsonObject> WidthsObj = MakeShareable(new FJsonObject());
	for (const TPair<FName, float>& Pair : ColumnWidths)
	{
		WidthsObj->SetNumberField(Pair.Key.ToString(), Pair.Value);
	}
	LayoutData->SetObjectField(TEXT("ColumnWidths"), WidthsObj);

	// ソート状態を保存 / Save sort state
	if (Model.IsValid() && Model->GetSortMode() != EColumnSortMode::None)
	{
		LayoutData->SetStringField(TEXT("SortColumn"), Model->GetSortColumnId().ToString());
		LayoutData->SetNumberField(TEXT("SortMode"), static_cast<int32>(Model->GetSortMode()));
	}
	else
	{
		LayoutData->RemoveField(TEXT("SortColumn"));
		LayoutData->RemoveField(TEXT("SortMode"));
	}

	const FString LayoutFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataAssetSheetLayout") / Sheet->GetName() + TEXT(".json");

	FString JsonText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (FJsonSerializer::Serialize(LayoutData.ToSharedRef(), JsonWriter))
	{
		FFileHelper::SaveStringToFile(JsonText, *LayoutFilename);
	}
}


TSharedPtr<SWidget> SDataAssetSheetEditor::OnConstructHeaderContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("ColumnVisibility", LOCTEXT("ColumnVisibilitySection", "Column Visibility"));
	{
		for (FProperty* Prop : Model->GetColumnProperties())
		{
			FName ColName = Prop->GetFName();

			MenuBuilder.AddMenuEntry(
				FText::FromName(ColName),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::ToggleColumnVisibility, ColName),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SDataAssetSheetEditor::IsColumnVisible, ColName)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDataAssetSheetEditor::ToggleColumnVisibility(FName ColumnId)
{
	if (HiddenColumns.Contains(ColumnId))
	{
		HiddenColumns.Remove(ColumnId);
	}
	else
	{
		HiddenColumns.Add(ColumnId);
	}

	RebuildHeaderRow();
	AssetListView->RequestListRefresh();
}

bool SDataAssetSheetEditor::IsColumnVisible(FName ColumnId) const
{
	return !HiddenColumns.Contains(ColumnId);
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
	Model->RebuildRowCache(NewRowData);
	Model->ReapplyFilter();
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

	Model->ReapplyFilter();

	// 削除されたアセットが選択中の場合のみ詳細パネルをクリア / Only clear details if removed asset was selected
	TArray<TSharedPtr<FDataAssetRowData>> SelectedItems = AssetListView->GetSelectedItems();
	bool bWasSelected = false;
	for (const TSharedPtr<FDataAssetRowData>& Item : SelectedItems)
	{
		if (Item.IsValid() && Item->AssetPath == RemovedPath)
		{
			bWasSelected = true;
			break;
		}
	}
	if (bWasSelected)
	{
		DetailsView->SetObject(nullptr);
	}

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

	Model->ReapplyFilter();
	AssetListView->RequestListRefresh();
	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Asset renamed: %s -> %s"), *OldObjectPath, *AssetData.AssetName.ToString());
}

#undef LOCTEXT_NAMESPACE

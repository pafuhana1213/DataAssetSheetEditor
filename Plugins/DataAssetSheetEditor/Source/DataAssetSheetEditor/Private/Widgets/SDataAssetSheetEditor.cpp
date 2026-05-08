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
#include "HAL/FileManager.h"
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
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "GameplayTagContainer.h"

// 列幅の既定値と最小値 / Default and minimum column widths (pixels)
static constexpr float DefaultColumnWidth = 150.0f;
static constexpr float MinColumnWidth = 32.0f;

// Auto-fit 関連の定数 / Auto-fit related constants
static constexpr float MaxColumnWidth = 600.0f;
static constexpr float AutoFitHorizontalPadding = 16.0f;   // セル左右4px×2 + 余裕8px / cell horizontal padding + slack
static constexpr float AutoFitHeaderSortIndicator = 18.0f; // ソート矢印分の余白 / sort arrow allowance

// 文字列の描画幅を測る (改行入りなら最長行を返す) / Measure rendering width, taking the longest line for multiline strings
static float MeasureMaxLineWidth(const FString& InText, const FSlateFontInfo& InFont)
{
	if (InText.IsEmpty())
	{
		return 0.0f;
	}

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	if (!InText.Contains(TEXT("\n")))
	{
		return Measure->Measure(InText, InFont).X;
	}

	float MaxWidth = 0.0f;
	TArray<FString> Lines;
	InText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
	for (const FString& Line : Lines)
	{
		MaxWidth = FMath::Max(MaxWidth, static_cast<float>(Measure->Measure(Line, InFont).X));
	}
	return MaxWidth;
}

static FString GetLayoutFilenameForSheet(const UDataAssetSheet* Sheet)
{
	const FString LayoutDir = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataAssetSheetLayout");
	const FString SheetKey = Sheet ? FPaths::MakeValidFileName(Sheet->GetPathName()) : TEXT("InvalidSheet");
	return LayoutDir / (SheetKey + TEXT(".json"));
}

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

	// インライン編集コミット時に詳細パネルとテーブルを更新 / Sync details panel and table after inline cell edit
	Model->OnInlineEditCommitted.AddLambda([this]()
	{
		if (DetailsView.IsValid())
		{
			DetailsView->ForceRefresh();
		}
		if (AssetListView.IsValid())
		{
			AssetListView->RequestListRefresh();
		}
	});

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
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						if (!AssetListView.IsValid())
						{
							return FText::GetEmpty();
						}
						const int32 SelectedCount = AssetListView->GetNumItemsSelected();
						if (SelectedCount > 1)
						{
							return FText::Format(
								LOCTEXT("SelectedCount", "{0} selected"), SelectedCount);
						}
						return FText::GetEmpty();
					})
					.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f))
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
			.OnWidthChanged(FOnWidthChanged::CreateSP(this, &SDataAssetSheetEditor::OnColumnWidthChanged, AssetNameCol))
			.MenuContent()
			[
				BuildColumnHeaderMenu(AssetNameCol, nullptr)
			];
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
			.OnWidthChanged(FOnWidthChanged::CreateSP(this, &SDataAssetSheetEditor::OnColumnWidthChanged, ColName))
			.MenuContent()
			[
				BuildColumnHeaderMenu(ColName, Prop)
			];
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

float SDataAssetSheetEditor::ComputeAutoFitWidthForColumn(FName ColumnId, FProperty* Property) const
{
	if (!Model.IsValid())
	{
		return DefaultColumnWidth;
	}

	const FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle(TEXT("NormalFont"));

	// ヘッダー幅: ラベル文字列を実測 + ソート矢印分の余白
	// Header width: measured label + sort arrow allowance
	const FString HeaderLabel = Property
		? ColumnId.ToString()
		: LOCTEXT("AssetName", "Asset Name").ToString();
	const float HeaderWidth = MeasureMaxLineWidth(HeaderLabel, Font) + AutoFitHeaderSortIndicator;

	// セル測定戦略を型ごとに切り替える / Switch cell-measure strategy per property type
	float MaxCellWidth = 0.0f;
	float TypeMinWidth = MinColumnWidth;
	bool bMeasureFromCachedText = (Property == nullptr); // AssetName 列は常に CachedDisplayText 相当 (RowData->AssetName) を使う

	if (Property)
	{
		// Aパターン: テキスト描画がない型 → セル測定はスキップしヘッダーと型下限のみで決める
		// A-pattern: cells render no text → only header/type-min decide width
		if (CastField<FBoolProperty>(Property))
		{
			TypeMinWidth = 32.0f; // チェックボックス / checkbox
		}
		else if (CastField<FObjectProperty>(Property) || CastField<FSoftObjectProperty>(Property))
		{
			TypeMinWidth = 80.0f; // サムネイル / thumbnail
		}
		else if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			const UScriptStruct* S = StructProp->Struct;

			if (S == TBaseStructure<FLinearColor>::Get() || S == TBaseStructure<FColor>::Get())
			{
				TypeMinWidth = 80.0f; // カラースウォッチ / color swatch
			}
			// Bパターン: テキストは表示するが ExportText 形式が表示と異なる → セル相当の文字を再構築して測る
			// B-pattern: cell shows text, but ExportText differs from on-screen text → rebuild and measure
			else if (S == FGameplayTag::StaticStruct())
			{
				for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
				{
					if (!RowData.IsValid() || !RowData->IsLoaded()) continue;
					UDataAsset* Asset = RowData->Asset.Get();
					if (!Asset || !Model->AssetHasProperty(Asset, Property)) continue;
					const FGameplayTag* TagPtr = StructProp->ContainerPtrToValuePtr<FGameplayTag>(Asset);
					if (!TagPtr || !TagPtr->IsValid()) continue;
					MaxCellWidth = FMath::Max(MaxCellWidth,
						MeasureMaxLineWidth(TagPtr->GetTagName().ToString(), Font));
				}
			}
			else if (S == FGameplayTagContainer::StaticStruct())
			{
				// セルは \n で結合表示するため、最長 1 タグの幅でフィットする
				// Cell joins tag names with \n → fit the widest single tag
				for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
				{
					if (!RowData.IsValid() || !RowData->IsLoaded()) continue;
					UDataAsset* Asset = RowData->Asset.Get();
					if (!Asset || !Model->AssetHasProperty(Asset, Property)) continue;
					const FGameplayTagContainer* ContainerPtr =
						StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(Asset);
					if (!ContainerPtr || ContainerPtr->IsEmpty()) continue;
					for (const FGameplayTag& T : *ContainerPtr)
					{
						MaxCellWidth = FMath::Max(MaxCellWidth,
							MeasureMaxLineWidth(T.GetTagName().ToString(), Font));
					}
				}
			}
			else
			{
				// その他の構造体型は CachedDisplayText を測る (Cパターン)
				bMeasureFromCachedText = true;
			}
		}
		else
		{
			// Numeric / Name / String / Text / Enum / Array 等
			bMeasureFromCachedText = true;
		}
	}

	// Cパターン: CachedDisplayText (またはAssetName) を測る
	// C-pattern: measure CachedDisplayText (or AssetName for the asset-name column)
	if (bMeasureFromCachedText)
	{
		const bool bIsAssetNameColumn = (Property == nullptr);
		for (const TSharedPtr<FDataAssetRowData>& RowData : Model->GetRowDataList())
		{
			if (!RowData.IsValid())
			{
				continue;
			}

			FString CellText;
			if (bIsAssetNameColumn)
			{
				// Dirty マークの "* " 2 文字分も常に確保しておく / always reserve "* " for dirty marker
				CellText = TEXT("* ") + RowData->AssetName;
			}
			else
			{
				const FString* Cached = RowData->CachedDisplayText.Find(ColumnId);
				if (!Cached)
				{
					continue; // 未ロード行はスキップ / skip rows without cached text
				}
				CellText = *Cached;
			}

			MaxCellWidth = FMath::Max(MaxCellWidth, MeasureMaxLineWidth(CellText, Font));
		}
	}

	const float ContentWidth = FMath::Max(HeaderWidth, MaxCellWidth) + AutoFitHorizontalPadding;
	const float Raw = FMath::Max(ContentWidth, TypeMinWidth);
	return FMath::Clamp(Raw, MinColumnWidth, MaxColumnWidth);
}

void SDataAssetSheetEditor::AutoFitAllColumnWidths()
{
	if (!Model.IsValid())
	{
		return;
	}

	// AssetName 列
	ColumnWidths.Add(FName(TEXT("AssetName")), ComputeAutoFitWidthForColumn(FName(TEXT("AssetName")), nullptr));

	// 全プロパティ列 (非表示列も含む)
	for (FProperty* Prop : Model->GetColumnProperties())
	{
		const FName ColName = Prop->GetFName();
		ColumnWidths.Add(ColName, ComputeAutoFitWidthForColumn(ColName, Prop));
	}

	SaveLayoutData();
}

void SDataAssetSheetEditor::AutoFitColumnWidth(FName ColumnId)
{
	if (!Model.IsValid())
	{
		return;
	}

	FProperty* Prop = nullptr;
	if (ColumnId != FName(TEXT("AssetName")))
	{
		for (FProperty* P : Model->GetColumnProperties())
		{
			if (P && P->GetFName() == ColumnId)
			{
				Prop = P;
				break;
			}
		}
		if (!Prop)
		{
			return;
		}
	}

	ColumnWidths.Add(ColumnId, ComputeAutoFitWidthForColumn(ColumnId, Prop));
	SaveLayoutData();
}

void SDataAssetSheetEditor::ResetAllColumnWidths()
{
	if (!Model.IsValid())
	{
		return;
	}

	// 空にすれば ApplyColumnWidth のラムダが DefaultColumnWidth を返す
	// Empty map → bound attribute returns DefaultColumnWidth for every column
	ColumnWidths.Empty();
	SaveLayoutData();
}

TSharedRef<SWidget> SDataAssetSheetEditor::BuildColumnHeaderMenu(FName ColumnId, FProperty* Property)
{
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.BeginSection(TEXT("ColumnActions"), LOCTEXT("ColumnActionsSection", "Column"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutoFitWidthEntry", "Auto-Fit Width"),
			LOCTEXT("AutoFitWidthTooltip", "Resize this column to fit its content"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDataAssetSheetEditor::AutoFitColumnWidth, ColumnId))
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
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
		// 変更されたトップレベルプロパティのみ再構築し全列再計算を回避
		// Only rebuild the cache entry for the edited top-level property to avoid full per-row recomputation
		FProperty* ChangedProperty = PropertyChangedEvent.MemberProperty
			? PropertyChangedEvent.MemberProperty
			: PropertyChangedEvent.Property;

		for (const TSharedPtr<FDataAssetRowData>& Item : AssetListView->GetSelectedItems())
		{
			if (ChangedProperty)
			{
				Model->RebuildRowCacheForProperty(Item, ChangedProperty);
			}
			else
			{
				Model->RebuildRowCache(Item);
			}
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

	const FString LayoutFilename = GetLayoutFilenameForSheet(Sheet);

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

	const FString LayoutFilename = GetLayoutFilenameForSheet(Sheet);
	const FString LayoutDir = FPaths::GetPath(LayoutFilename);
	IFileManager::Get().MakeDirectory(*LayoutDir, /*Tree=*/true);

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

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (!Sheet)
	{
		return;
	}

	const FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();

	// bShowAll=false の場合は ManualAssets に登録済みのものだけ表示する
	// When bShowAll is false, only show assets already registered in ManualAssets.
	if (!Sheet->bShowAll)
	{
		const bool bRegisteredManually = Sheet->ManualAssets.ContainsByPredicate(
			[&AssetPath](const TSoftObjectPtr<UDataAsset>& SoftPtr)
			{
				return SoftPtr.ToSoftObjectPath() == AssetPath;
			});
		if (!bRegisteredManually)
		{
			return;
		}
	}

	for (const TSharedPtr<FDataAssetRowData>& ExistingRow : Model->GetRowDataList())
	{
		if (ExistingRow.IsValid() && ExistingRow->AssetPath == AssetPath)
		{
			return;
		}
	}

	// 新しいRowDataを追加 / Add new row data
	TSharedPtr<FDataAssetRowData> NewRowData = MakeShared<FDataAssetRowData>();
	NewRowData->AssetPath = AssetPath;
	NewRowData->AssetName = AssetData.AssetName.ToString();
	NewRowData->AssetClass = AssetData.GetClass();

	// 既にロード済みならアセット参照をセット / Set asset reference if already loaded
	if (UObject* LoadedObject = AssetPath.ResolveObject())
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
		Sheet->Modify();
		const int32 RemovedManualCount = Sheet->ManualAssets.RemoveAll([&RemovedPath](const TSoftObjectPtr<UDataAsset>& SoftPtr)
		{
			return SoftPtr.ToSoftObjectPath() == RemovedPath;
		});
		if (RemovedManualCount > 0)
		{
			Sheet->MarkPackageDirty();
		}
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
	const FSoftObjectPath NewPath = AssetData.GetSoftObjectPath();

	UDataAssetSheet* Sheet = DataAssetSheet.Get();
	if (Sheet)
	{
		bool bUpdatedManualAsset = false;
		for (TSoftObjectPtr<UDataAsset>& SoftPtr : Sheet->ManualAssets)
		{
			if (SoftPtr.ToSoftObjectPath() == OldPath)
			{
				if (!bUpdatedManualAsset)
				{
					Sheet->Modify();
					bUpdatedManualAsset = true;
				}
				SoftPtr = TSoftObjectPtr<UDataAsset>(NewPath);
			}
		}
		if (bUpdatedManualAsset)
		{
			Sheet->MarkPackageDirty();
		}
	}

	for (TSharedPtr<FDataAssetRowData>& RowData : Model->GetMutableRowDataList())
	{
		if (RowData->AssetPath == OldPath)
		{
			RowData->AssetPath = NewPath;
			RowData->AssetName = AssetData.AssetName.ToString();
			RowData->AssetClass = AssetData.GetClass();
			break;
		}
	}

	Model->ReapplyFilter();
	AssetListView->RequestListRefresh();
	UE_LOG(LogDataAssetSheetEditor, Log, TEXT("Asset renamed: %s -> %s"), *OldObjectPath, *AssetData.AssetName.ToString());
}

#undef LOCTEXT_NAMESPACE

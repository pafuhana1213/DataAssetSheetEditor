# DataAsset Sheet Editor

![UE Version](https://img.shields.io/badge/Unreal%20Engine-5.7-0e1128?logo=unrealengine&logoColor=white)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**English** | [日本語](#日本語)

A Unreal Engine 5 editor extension that lists multiple DataAssets in a spreadsheet-style sheet so you can browse and edit them efficiently — all without leaving the editor or giving up the flexibility of DataAssets.

<!--
スクリーンショット/GIF をここに配置 / Place a screenshot or GIF here.
例 / e.g.: ![Overview](Docs/Images/overview.png)
-->

---

## English

### Overview

DataAssets are great for customization and iteration speed, but they become hard to compare and manage once you have a lot of them. **DataAsset Sheet Editor** gathers DataAssets of a chosen class into a single table, where each row is an asset and each column is one of its properties. You get a spreadsheet-like overview while still editing real DataAssets through Unreal's own property system.

### Features

- **Spreadsheet view** — list all DataAssets of a target class as rows, with one column per property.
- **Three ways to collect assets**, freely combined:
  - **Show All** — automatically include every asset of the target class in the project.
  - **Manual Assets** — register assets one by one. The asset picker is filtered to the target class.
  - **Collections** — pull assets in from Unreal's Collection system.
- **Target Class / Display Class** — pick the class to edit; optionally set a derived *Display Class* to also show columns for properties added by subclasses.
- **Hybrid editing** — edit simple values (numbers, strings, enums) inline in the cell, and edit any property — including structs, arrays and object references — in the **Details** panel powered by Unreal's `IDetailsView`.
- **Column tools** — drag to resize, auto-fit a single column or all columns to their content (clamped 32–600px), reset widths, and show/hide individual columns. Widths, hidden columns and sort state are saved per sheet.
- **Row tools** — reorder rows with drag & drop, delete rows, and add empty rows to fill in later (for Manual Assets).
- **Search & sort** — filter rows with the search box and sort by clicking a column header. A row counter shows filtered / total.
- **CSV import & export** — round-trip the sheet to CSV for bulk editing or external tools.
- **Thumbnails** — object and texture references are shown as inline thumbnails.
- **Undo/Redo** and asynchronous asset loading.

### Requirements

- Unreal Engine 5.7
- Windows (Win64)
- Currently **Beta** (v0.1.0)

### Installation

1. Download the latest release from [Releases](https://github.com/pafuhana1213/DataAssetSheetEditor/releases).
2. Copy the `DataAssetSheetEditor/` folder into your project's `Plugins/` folder.
3. Restart the Unreal Editor.

### Getting Started

1. In the Content Browser, create a **DataAsset Sheet** asset (right-click → Miscellaneous → DataAsset Sheet) and choose the **Target Class** you want to edit in the dialog.
2. Open the **Settings** tab and choose how to collect assets: enable **Show All**, add **Manual Assets**, and/or register **Collections**. Optionally set a **Display Class** to reveal subclass properties.
3. **Double-click** the DataAsset Sheet asset to open the spreadsheet editor.
4. Edit values directly in cells, or select a row and edit in the **Details** panel.
5. Press **Ctrl+S** to save the edited DataAssets.

A small set of sample assets lives in [`Content/DataAssetSheetEditorSample/`](Content/DataAssetSheetEditorSample/) (open `DAS_Sample`) if you want to try the workflow right away.

<!-- 使い方の GIF をここに / Place a usage GIF here. -->

### License

[MIT License](LICENSE)

### Author

[@pafuhana1213](https://x.com/pafuhana1213)

---

## 日本語

[English](#english) | **日本語**

### 概要

DataAsset Sheet Editorは、Unreal Engine 5向けのエディタ拡張プラグインです。指定したクラスのDataAssetをスプレッドシート形式で一覧し、各行を1アセット・各列をプロパティとして、効率的に閲覧・編集できます。

DataAssetはカスタマイズ性・反復開発の速さに優れる一方、数が増えると比較や管理が難しくなります。本プラグインは、UE標準のプロパティシステムを活かしたまま、表形式での一覧性を両立します。

### 特徴

- **スプレッドシート表示** — ターゲットクラスのDataAssetを行、プロパティを列として一覧表示。
- **3つのアセット収集方法**（併用可）:
  - **Show All** — プロジェクト内の対象クラスの全アセットを自動表示。
  - **Manual Assets** — 個別に登録。アセットピッカーはターゲットクラスのみに絞り込み。
  - **Collections** — UEのコレクション機能から取り込み。
- **Target Class / Display Class** — 編集対象クラスを指定。任意で派生クラス（Display Class）を指定すると、サブクラスで追加されたプロパティの列も表示。
- **ハイブリッド編集** — 単純な値（数値・文字列・Enum）はセル内で直接編集、Struct・配列・オブジェクト参照を含む全プロパティはUE標準の`IDetailsView`による **Details** パネルで編集。
- **列ツール** — ドラッグでリサイズ、単一列／全列を内容に合わせて自動幅調整（32〜600pxにクランプ）、幅リセット、列の表示／非表示切替。幅・非表示列・ソート状態はシートごとに保存。
- **行ツール** — ドラッグ&ドロップで並び替え、行削除、後で割り当てる空行の追加（Manual Assets対象）。
- **検索・ソート** — 検索ボックスで行を絞り込み、列ヘッダークリックでソート。フィルタ後／全体の行数も表示。
- **CSV入出力** — シートをCSVへ書き出し／読み込み。一括編集や外部ツール連携に。
- **サムネイル** — オブジェクト・テクスチャ参照はセル内にサムネイル表示。
- **Undo/Redo** と非同期ロードに対応。

### 動作環境

- Unreal Engine 5.7
- Windows (Win64)
- 現在 **Beta**（v0.1.0）

### インストール

1. [Releases](https://github.com/pafuhana1213/DataAssetSheetEditor/releases)から最新版をダウンロード
2. プロジェクトの`Plugins/`フォルダに`DataAssetSheetEditor/`を配置
3. UEエディタを再起動

### 使い方

1. コンテンツブラウザで **DataAsset Sheet** アセットを作成（右クリック → Miscellaneous → DataAsset Sheet）し、表示されるダイアログで編集したい **Target Class** を選択
2. **Settings** タブでアセットの収集方法を設定（**Show All** を有効化、**Manual Assets** を追加、**Collections** を登録）。必要に応じて **Display Class** を指定
3. DataAsset Sheet アセットを **ダブルクリック** してスプレッドシートエディタを開く
4. セル内で直接、または行を選択して **Details** パネルで編集
5. **Ctrl+S** で編集したDataAssetを保存

すぐに試したい場合は、[`Content/DataAssetSheetEditorSample/`](Content/DataAssetSheetEditorSample/) にサンプルアセットがあります（`DAS_Sample` を開いてください）。

<!-- 使い方の GIF をここに / Place a usage GIF here. -->

### ライセンス

[MIT License](LICENSE)

### 作者

[@pafuhana1213](https://x.com/pafuhana1213)

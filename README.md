# AviUtl2 BoundingBox

AviUtl2のテキストなどの個別オブジェクトに、アルファ情報をもとにしたバウンディングボックスを付加するスクリプトです。

![AviUtl2](https://img.shields.io/badge/AviUtl2-script-blue)

## 概要

現在のオブジェクト画像から不透明な範囲を検出し、その外接矩形を描画します。

以下の設定に対応しています。

- しきい値
- ボックスの透明度
- 透明度の2値化
- ボックスのみ表示
- ボックスの色

## 導入方法

次の2ファイルをAviUtl2のスクリプトフォルダへコピーしてください。

- `BoundingBox.anm2`
- `BoundingBox.mod2`

導入後、AviUtl2の「キャッシュを破棄」を実行するか、AviUtl2を再起動してください。

## 使い方

1. テキストオブジェクトを作成します。
2. テキストを個別オブジェクトとして表示します。
3. アニメーション効果から「Bounding Box」を追加します。
4. 必要に応じて設定を変更します。

### ボックスのみ

「ボックスのみ」を有効にすると、元のテキスト画像を透明にして、検出された外接矩形全体を指定色で塗りつぶします。

無効の場合は、元のテキストを残したまま、透明部分を含む外接矩形を描画します。

## ファイル構成

| ファイル | 内容 |
| --- | --- |
| `BoundingBox.anm2` | AviUtl2用アニメーション効果 |
| `BoundingBox.mod2` | ピクセル処理を行うスクリプトモジュール |
| `BoundingBox.cpp` | `BoundingBox.mod2`のソースコード |
| `build-BoundingBox.ps1` | スクリプトモジュールのビルドスクリプト |

## ビルド

Visual Studio Build Toolsなど、`cl.exe`が利用できる環境で、次のコマンドを実行します。

```powershell
.\build-BoundingBox.ps1
```

ビルドスクリプトは、ユーザーの`Documents`以下から`aviutl2_sdk`フォルダを検索し、生成された`BoundingBox.mod2`をスクリプトフォルダへ配置します。

## 仕組み

`BoundingBox.anm2`からAviUtl2の`obj.getpixeldata()`でオブジェクト画像を取得し、`BoundingBox.mod2`でアルファ値を走査します。

検出した矩形を描画した後、`obj.putpixeldata()`でオブジェクトへ書き戻します。

2値化が有効な場合は、画像全体の走査と外接矩形の検出を同じループで処理することで、不要な走査を減らしています。

## 参考・作成経緯

旧AviUtl向けのもる氏が作成した`BoundingBox.anm`を参考に、AviUtl2のLuaスクリプトおよびスクリプトモジュール形式へ移植しました。

元実装の参考:

- [BoundingBox.anm](https://drive.google.com/drive/folders/1DaBcNEEVqcmz6iUaLLFfUlsjwz39yyz3)

本移植版は、元実装の動作や設定を確認しながら、生成AIを用いてAviUtl2向けに生成・調整したものです。

## 注意事項

- AviUtl2のバージョンやSDKの変更により、動作しなくなる場合があります。
- 生成AIによる実装のため、利用前にソースコードと動作を確認してください。

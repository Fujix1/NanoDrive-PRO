# NanoDrive Pro (YM2608 and YM2151 vgm player)

This project is an attempt to play vgm (and s98) files using the YM2608 and YM2151 chips with the Longan Nano microcontroller.

<img src="https://user-images.githubusercontent.com/13434151/222690840-82f2402a-5d5c-4656-935e-4d4e2f834d90.jpg">

## 特徴 / Characteristics

1. .vgm 形式と.s98 形式に対応（s98 は v3 のみのテストサポート）。
2. I2C 可変周波数生成器 Si5351 を使用して幅広い周波数で動作。
3. I2C デジタルボリュームにより、各種雑音の抑制、指定ループ数後のフェードアウト機能。
4. フォルダ単位で SSG の音量を約 -7dB 下げる「PC98」モード。
5. フォルダ単位で全体の音量を-2～-12dB 調整可能。
6. DRAM、インバータ IC、スイッチ IC を表面実装済み。

<br>

1. supports .vgm and .s98 formats (s98 is test support for v3 only).
2. Operates over a wide range of frequencies using the I2C variable frequency generator Si5351.
3. I2C digital volume controller for minimizing various noises and fade-out function after a specified number of loops.
4. Per-folder "PC98" mode that reduces the SSG volume by approximately -7dB.
5. Per-folder volume adjustment by -2 to -12dB.
6. Pre-mounted SOP DRAM, inverter IC and switch IC.

## 回路図 / Schematics

[Schematics.pdf](https://github.com/Fujix1/NanoDrive-PRO/files/11007896/Schematics.pdf)

## マニュアル / Japanese Manual

[NanoDrivePro_Manual.pdf](https://github.com/Fujix1/NanoDrive-PRO/files/10985764/NanoDrivePro_Manual.pdf)


## 部品表 / Parts List

<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" type="text/css" id="u0" href="https://rakko.tools/tools/129/lib/tinymce/skins/ui/oxide/content.min.css" />
<link rel="stylesheet" type="text/css" id="u1" href="https://rakko.tools/tools/129/lib/tinymce/skins/content/default/content.min.css" />
</head>
<body id="tinymce" class="mce-content-body" data-id="content" contenteditable="true" spellcheck="false" data-dl-input-translation="true">
<table class="mce-item-table">
<tbody>
<tr>
<th>番号</th>
<th>部品</th>
<th>個数</th>
<th>説明</th>
<th>通販コード例</th>
</tr>
<tr>
<th>C1, C2</th>
<td>1000uF</td>
<td>2</td>
<td>電源用。OS-CON 1000uF 6.4V。ピッチ3.5mm。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-08293/" target="_blank" rel="noopener">P-08293</a></td>
</tr>
<tr>
<th>C3</th>
<td>10uF</td>
<td>1</td>
<td>3.3v電源用電解コンデンサ。ピッチ2mm。10uFじゃなくてもOK。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-03116/" target="_blank" rel="noopener">P-03116</a><br>
など</td>
</tr>
<tr>
<th>C15, C26, C41 - C44</th>
<td>10uF</td>
<td>6</td>
<td>オーディオ側の電解コンデンサ。ピッチ2mm。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-04621/">P-04621</a><br>
<a href="https://akizukidenshi.com/catalog/g/gP-04624/">P-04624</a><br>
<a href="https://eleshop.jp/shop/g/gD1U41K/" target="_blank" rel="noopener">D1U41K</a></td>
</tr>
<tr>
<th>C17</th>
<td>68p</td>
<td>1</td>
<td>コンデンサ。ピッチ5mm。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-08048/">P-08048</a></td>
</tr>
<tr>
<th>C18, C19, C23, C24</th>
<td>2200pf</td>
<td>4</td>
<td>フィルムコンデンサ。ピッチ5mm。<br>
【注】メタライズドポリエステルフィルムコンデンサ(P-14589)は大きすぎてはみ出します</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-07686/">P-07686</a></td>
</tr>
<tr>
<th>C31 - C35, C39</th>
<td>4.7uF</td>
<td>6</td>
<td>電解コンデンサ。ピッチ2mm。品薄。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-04623/">P-04623</a><br>
<a href="https://eleshop.jp/shop/g/gD1V411/">D1V411</a></td>
</tr>
<tr>
<th>C36, C37</th>
<td>1000pF</td>
<td>2</td>
<td>ローパスフィルタ用フィルムコンデンサ。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-14587/">P-14587</a><br>
<a href="https://akizukidenshi.com/catalog/g/gP-07673/">P-07673</a><br>
など</td>
</tr>
<tr>
<th>C38</th>
<td>100uF</td>
<td>1</td>
<td>電源中点用電解コンデンサ。ピッチ2.5mm。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-02724/">P-02724</a></td>
</tr>
<tr>
<th>C10 - C14, C20 - C22, C30, C40</th>
<td>0.1uF</td>
<td>10</td>
<td>パスコン。ピッチ5mm。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-15927/">P-15927</a></td>
</tr>
<tr>
<th>R1 - R3, R9, R24</th>
<td>1k</td>
<td>5</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R4</th>
<td>33</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R5, R14, R15, R20, R21</th>
<td>4.7k</td>
<td>5</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R6</th>
<td>2k</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R7</th>
<td>330</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R8</th>
<td>620</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R10</th>
<td>3.3k</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R11</th>
<td>270</td>
<td>1</td>
<td>抵抗。</td>
<td>同梱</td>
</tr>
<tr>
<th>R18</th>
<td>15k</td>
<td>1</td>
<td>抵抗。基板上の値は無視してください。</td>
<td>同梱</td>
</tr>
<tr>
<th>R12, R13, R16, R17, R19, R22, R23</th>
<td>10k</td>
<td>7</td>
<td>抵抗。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gR-25103/">R-25103</a><br>
<a href="https://akizukidenshi.com/catalog/g/gR-16877/">R-16877</a></td>
</tr>
<tr>
<th>D1</th>
<td>LED</td>
<td>1</td>
<td>データ確認用LED。長い足を左にして実装します。</td>
<td>同梱</td>
</tr>
<tr>
<th>J1</th>
<td>MJ-8435</td>
<td>1</td>
<td>3.5mmステレオミニジャック。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gC-09060/">C-09060</a></td>
</tr>
<tr>
<th>JP1</th>
<td>ジャンパ2P</td>
<td>1</td>
<td>SSGスルー用のジャンパ。付けなくても大丈夫。</td>
<td>同梱</td>
</tr>
<tr>
<th rowspan="2">RV1 – RV3<br></th>
<td>半固定抵抗</td>
<td>3</td>
<td>アプルスアルパインRK12L12H000D。</td>
<td>同梱</td>
</tr>
<tr>
<td>つまみ</td>
<td>3</td>
<td>6mm軸。D型シャフト用かイモネジ固定タイプ。直径18mmまででお好きなものを。Chroma CapsはSuper Knob 180°が適合。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-12202/">P-12202</a><br>
<a href="https://akizukidenshi.com/catalog/g/gP-16279/">P-16279</a><br>
<a href="https://www.sengoku.co.jp/mod/sgk_cart/search.php?maker=DJ%20TechTools">Chroma Caps</a>など</td>
</tr>
<tr>
<th>SW1 – SW5</th>
<td>タクトスイッチ</td>
<td>5</td>
<td>4ピンスルーホール。国産メーカーのものが軽くておすすめ。秋月の5本足タイプは1本切って使います。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gP-11243/">P-11243</a><br>
<a href="https://akizukidenshi.com/catalog/g/gP-08075/">P-08075</a><br>
など</td>
</tr>
<tr>
<th>U1, U2</th>
<td>4回路入り<br>
オペアンプ</td>
<td>2</td>
<td>４回路入りでスルーレートが低すぎないもの。<br>
【動作確認済み】NJU7034D, NJM2747D, LME49740NA</td>
<td><a href="https://akizukidenshi.com/catalog/g/gI-14029/">I-14029</a><br>
<a href="https://akizukidenshi.com/catalog/g/gI-09117/">I-09117</a><br>
<a href="https://akizukidenshi.com/catalog/g/gI-03685/">I-03685</a><br>
など</td>
</tr>
<tr>
<th>U3</th>
<td>NJM3414D</td>
<td>1</td>
<td>反転増幅ミキシング用オペアンプ。</td>
<td>同梱</td>
</tr>
<tr>
<th>U4</th>
<td>M11B416256A-35J</td>
<td>1</td>
<td>4Mbit DRAM。実装済み。実際に使用するのは半分の2Mb。</td>
<td>実装済み</td>
</tr>
<tr>
<th>U5</th>
<td>TC7S04F</td>
<td>1</td>
<td>インバータロジックIC。実装済み。DRAMのOEを反転する。</td>
<td>実装済み</td>
</tr>
<tr>
<th>U6</th>
<td>AE-Si5351A</td>
<td>1</td>
<td>I2Cクロック生成モジュール。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gK-10679/">K-10679</a></td>
</tr>
<tr>
<th>U7</th>
<td>PT2257</td>
<td>1</td>
<td>I2Cボリューム制御IC。</td>
<td>同梱</td>
</tr>
<tr>
<th>U8</th>
<td>ADG419BRZ</td>
<td>1</td>
<td>スイッチIC。実装済み。高い。PC98モードの切替え用。</td>
<td>実装済み</td>
</tr>
<tr>
<th>FM1</th>
<td>YM2608B</td>
<td>1</td>
<td>FM音源IC。YM2608（Bなし）は動作しません。</td>
<td>同梱(オプション)</td>
</tr>
<tr>
<th>FM2</th>
<td>YM2151</td>
<td>1</td>
<td>FM音源IC。品薄。</td>
<td>同梱(オプション)</td>
</tr>
<tr>
<th>IC1</th>
<td>YM3016</td>
<td>1</td>
<td>YM2608B用DAC。</td>
<td>同梱(オプション)</td>
</tr>
<tr>
<th>IC2</th>
<td>YM3012</td>
<td>1</td>
<td>YM2151用DAC。</td>
<td>同梱(オプション)</td>
</tr>
<tr>
<th rowspan="3">M1</th>
<td>GD32VF103CBT6</td>
<td>1</td>
<td>Longan Nanoマイコン、Flash128KB／SRAM 32KB版。<br>
【注意】v1.1はピン配列が異なるため使えません。</td>
<td><a href="https://akizukidenshi.com/catalog/g/gK-14678/">K-14678</a></td>
</tr>
<tr>
<td>連結ヘッダ</td>
<td>1</td>
<td>マイコン用分割ピンヘッダ。</td>
<td>同梱</td>
</tr>
<tr>
<td>ソケット</td>
<td>1</td>
<td>マイコン用分割ソケット。</td>
<td>同梱</td>
</tr>
<tr>
<th rowspan="5">ICソケット<br></th>
<td>8ピン</td>
<td>3</td>
<td rowspan="5">64ピンシュリンクのみ板バネ、その他は丸ピン。<br></td>
<td rowspan="5">同梱<br></td>
</tr>
<tr>
<td>14ピン</td>
<td>2</td>
</tr>
<tr>
<td>16ピン</td>
<td>2</td>
</tr>
<tr>
<td>24ピン</td>
<td>1</td>
</tr>
<tr>
<td>64ピン</td>
<td>1</td>
</tr>
</tbody>
</table>
</body>

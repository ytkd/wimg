# wimg
ブートイメージをSDカードに書き込むプログラムです。
Raspberry piなどを使うときに便利です。

## 使い方

    wimg <image file> <drive>

たとえばdisk.imgをドライブX:に接続されているmicroSDカードに書き込む場合には

    wimg disk.img x

とします。

## 途中でエラーになる場合

SDカードに複数のパーティションがあると書き込めません。
もう一度実行すると書き込める場合がありますが、

    diskpart disk clean

などのようにしてパーティションを消してください。

// matrix.h
// Created on : 2025/12/13
// Author : T.Ijiro

#ifndef MATRIX_H
#define MATRIX_H

// 横ドット数
#define MAT_WIDTH  8   

// 縦ドット数
#define MAT_HEIGHT 8   

// ドット総数
#define MAT_PIXELS MAT_WIDTH * MAT_HEIGHT 

enum led_color {
    led_off,     // 0
    led_red,     // 1
    led_green,   // 2
    led_orange   // 3
};

// 入出力初期化
void init_MATRIX(void);

// 指定座標に色を書き込む
void matrix_write(const int x, const int y, const enum led_color c);

// 指定座標の色を読み込む
enum led_color matrix_read(const int x, const int y);

// 指定座標の色を消去
void matrix_delete(const int x, const int y);

// 描画バッファ全消去 
void matrix_clear(void);

// 描画バッファを外部バッファにコピー
void matrix_copy(enum led_color dst[MAT_HEIGHT][MAT_WIDTH]);

// 描画バッファに外部バッファを貼り付け
void matrix_paste(const enum led_color src[MAT_HEIGHT][MAT_WIDTH]);

// 描画バッファ全体を指定した方向に１つずらす
// 上：'u'  下：'d'  左：'l'  右：'r'
void matrix_scroll(const char dir);

// 描画バッファを表示バッファへ反映
void matrix_flush(void);

// 指定列の表示バッファをマトリックスLED送信用16bitデータに変換 
unsigned short int matrix_convert(const int x);

// 指定列の16bitデータをマトリックスLEDに出力 
void matrix_out(const int x, const unsigned short int data);

#endif /* MATRIX_H */

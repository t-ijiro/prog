// =========================================================
// matrix.h
// Created on : 2025/12/13
// Author : T.Ijiro
//
// 使い方
//
// 1. 描画バッファに書き込む
//    matrix_clear();              // バッファをクリア
//    matrix_write(x, y, color);   // 指定座標に色を設定
//
// 2. 描画バッファを表示バッファに反映
//    matrix_flush();              // 描画内容を確定
//
// 3. ダイナミック点灯制御 タイマー割り込みとかでやる
//    col++;
//    col %= 8;
//    data = matrix_convert(col);  // 列データを変換
//    matrix_out(col, data);       // LEDに出力
//
// プログラム例
// 例1：中心に赤いドットを表示
//    matrix_clear();
//    matrix_write(4, 4, led_red);
//    matrix_flush();
//
// 例2：市松模様を表示
//    matrix_clear();
//
//    for(y = 0; y < MAT_HEIGHT; y++)
//    {
//        for(x = 0; x < MAT_WIDTH; x++)
//        {
//            if((x + y) % 2 == 0)
//            {
//                matrix_write(x, y, led_red);
//            } 
//            else
//            {
//                matrix_write(x, y, led_green);
//            }
//        }
//    }
//
//    matrix_flush();
//
// ========================================================= 

#ifndef MATRIX_H
#define MATRIX_H

#define MAT_WIDTH   8   // 横ドット数
#define MAT_HEIGHT  8   // 縦ドット数

enum led_color {
    led_off,     // 0
    led_red,     // 1
    led_green,   // 2
    led_orange   // 3
};

// =========================================================
// 入出力初期化
// ========================================================= 

void init_MATRIX(void);

// =========================================================
// 描画バッファ操作
// ========================================================= 

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

// 描画バッファを表示バッファへ反映
void matrix_flush(void);

// =========================================================
// 表示バッファ操作
// ========================================================= 

// 指定列の表示バッファをLED送信用16bitデータに変換 
unsigned int matrix_convert(const int x);

// 指定列の16bitデータをマトリックスLEDに出力 
void matrix_out(const int x, const unsigned int data);

#endif /* MATRIX_H */

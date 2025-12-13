// matrix.c
// Created on : 2025/12/13
// Author : T.Ijiro

#include <string.h>
#include "iodefine.h"
#include "matrix.h"

// 74HC595シフトレジスタのシリアルデータ送信コマンド
#define SERIAL_SINK    do { PORT1.PODR.BIT.B5 = 0; } while(0)                        // 吸い込み
#define SERIAL_SOURCE  do { PORT1.PODR.BIT.B5 = 1; } while(0)                        // 吐き出し
#define SEND_LATCH_CLK do { PORT1.PODR.BIT.B6 = 1; PORT1.PODR.BIT.B6 = 0; } while(0) // ラッチ
#define LATCH_OUT      do { PORT1.PODR.BIT.B7 = 1; PORT1.PODR.BIT.B7 = 0; } while(0) // ラッチ出力

// マトリックスLED
#define MAT_WIDTH  8            // 横のコマ数
#define MAT_HEIGHT 8            // 縦のコマ数
#define COL_EN PORTE.PODR.BYTE  // 点灯列許可ビット選択

// 色
enum led_color{
    led_off,
    led_red,
    led_green,
    led_orange
};

// 描画用バッファ (読み書き専用)
static enum led_color canvas[MAT_HEIGHT][MAT_WIDTH]  = {led_off};

// 表示用バッファ (読み取り専用)
static enum led_color display[MAT_HEIGHT][MAT_WIDTH] = {led_off};

// 入出力初期化
void init_MATRIX(void)
{
    PORT1.PDR.BYTE = 0xE0;
    PORTE.PDR.BYTE = 0xFF;
}

// x座標チェック
static unsigned char is_out_of_WIDTH(const int x)
{
    return ((MAT_WIDTH - 1 < x) || (x < 0));
}

// y座標チェック
static unsigned char is_out_of_HEIGHT(const int y)
{
    return ((MAT_HEIGHT - 1 < y) || (y < 0));
}

// 描画バッファ書き込み
void matrix_write(const int x, const int y, const enum led_color c)
{
    if(is_out_of_WIDTH(x) || is_out_of_HEIGHT(y))
    {
        return;
    }

    if(led_orange < c || c < led_off)
    {
        return;
    }

    canvas[y][x] = c;
}

// 描画バッファ読み込み
enum led_color matrix_read(const int x, const int y)
{
    if(is_out_of_WIDTH(x) || is_out_of_HEIGHT(y))
    {
        return led_off;
    }

    return canvas[y][x];
}

// 描画バッファを1つ削除
void matrix_delete(const int x, const int y)
{
    if(is_out_of_WIDTH(x) || is_out_of_HEIGHT(y))
    {
        return;
    }

    canvas[y][x] = led_off;
}

// 描画バッファを全削除
void matrix_clear(void)
{
    memset(canvas, led_off, sizeof(canvas));
}

// 描画バッファを外部バッファにコピー
void matrix_copy(enum led_color dst[MAT_HEIGHT][MAT_WIDTH])
{
    memcpy(dst, canvas, sizeof(canvas));
}

// 描画バッファに外部バッファを貼り付け
void matrix_paste(const enum led_color src[MAT_HEIGHT][MAT_WIDTH])
{
    memcpy(canvas, src, sizeof(canvas));
}

// 描画バッファを表示バッファに反映
void matrix_flush(void)
{
    memcpy(display, canvas, sizeof(display));
}

// 表示バッファをマトリックスLED送信用データに変換
unsigned int matrix_convert(const int x)
{
    int y;
    unsigned int data = 0x0000;
    enum led_color c;
    
    if(is_out_of_WIDTH(x))
    {
        return data;
    }
    
    for(y = 0; y < MAT_HEIGHT; y++)
    {
        c = display[y][x];

        if(c & led_red)
        {
            data |= 1 << (y + 8);
        }

        if(c & led_green)
        {
            data |= 1 << y;
        }
    }

    return data;
}

// 列を指定してマトリックスLEDにデータ出力
void matrix_out(const int x, const unsigned int data)
{
    int shift_i;
    
    if(is_out_of_WIDTH(x))
    {
        return;
    }
    
    for(shift_i = 0; shift_i < MAT_WIDTH * 2; shift_i++) 
    {
        
        if(data & (1 << shift_i)) 
        {
            SERIAL_SINK;
        } 
        else 
        {
            SERIAL_SOURCE;
        }

        SEND_LATCH_CLK;
    }
    
    COL_EN = 0x00;

    LATCH_OUT;

    COL_EN = 1 << x;

}


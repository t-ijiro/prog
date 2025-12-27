// matrix.c
// Created on : 2025/12/13
// Author : T.Ijiro

#include <stdbool.h>
#include <string.h>
#include "iodefine.h"
#include "matrix.h"

// 74HC595シフトレジスタのシリアルデータ送信コマンド
#define SERIAL_SINK    do { PORT1.PODR.BIT.B5 = 0; } while(0)                        // 吸い込み
#define SERIAL_SOURCE  do { PORT1.PODR.BIT.B5 = 1; } while(0)                        // 吐き出し
#define SEND_LATCH_CLK do { PORT1.PODR.BIT.B6 = 1; PORT1.PODR.BIT.B6 = 0; } while(0) // ラッチ
#define LATCH_OUT      do { PORT1.PODR.BIT.B7 = 1; PORT1.PODR.BIT.B7 = 0; } while(0) // ラッチ出力

// マトリックスLED
#define COL_EN PORTE.PODR.BYTE  // 点灯列許可ビット選択

// 描画用バッファ (読み書き専用)
static enum led_color canvas[MAT_HEIGHT][MAT_WIDTH]  = {led_off};

// 表示用バッファ (読み取り専用)
static enum led_color display[MAT_HEIGHT][MAT_WIDTH] = {led_off};

// 入出力初期化
void init_MATRIX(void)
{
    PORT1.PDR.BYTE = 0xE0;
    PORTE.PDR.BYTE = 0xFF;
    PORT1.PODR.BIT.B6 = 0;
    PORT1.PODR.BIT.B7 = 0;
    COL_EN = 0x00;
}

// x座標チェック
static bool is_out_of_WIDTH(const int x)
{
    return ((MAT_WIDTH <= x) || (x < 0));
}

// y座標チェック
static bool is_out_of_HEIGHT(const int y)
{
    return ((MAT_HEIGHT <= y) || (y < 0));
}

// 指定座標に色を書き込む
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

// 指定座標の色を読み込む
enum led_color matrix_read(const int x, const int y)
{
    if(is_out_of_WIDTH(x) || is_out_of_HEIGHT(y))
    {
        return led_off;
    }

    return canvas[y][x];
}

// 指定座標の色を削除
void matrix_delete(const int x, const int y)
{
    if(is_out_of_WIDTH(x) || is_out_of_HEIGHT(y))
    {
        return;
    }

    canvas[y][x] = led_off;
}

// 描画バッファ全削除
void matrix_clear(void)
{
    memset(canvas, (int)led_off, sizeof(canvas));
}

// 描画バッファを外部バッファにコピー
void matrix_copy(enum led_color dst[MAT_HEIGHT][MAT_WIDTH])
{
    memmove(dst, canvas, sizeof(canvas));
}

// 描画バッファに外部バッファを貼り付け
void matrix_paste(const enum led_color src[MAT_HEIGHT][MAT_WIDTH])
{
    memmove(canvas, src, sizeof(canvas));
}

// 描画バッファ全体を左に１つずらす
static void matrix_scroll_left(void)
{
    int x, y;

    for(x = 0; x < MAT_WIDTH - 1; x++)
    {
        for(y = 0; y < MAT_HEIGHT; y++)
        {
            canvas[y][x] = canvas[y][x + 1];

            if(x + 1 == MAT_WIDTH - 1)
            {
                canvas[y][x + 1] = led_off;
            }
        }
    }
}

// 描画バッファ全体を右に１つずらす
static void matrix_scroll_right(void)
{
    int x, y;
   
    for(x = MAT_WIDTH - 1; 0 < x; x--)
    {
        for(y = 0; y < MAT_HEIGHT; y++)
        {
            canvas[y][x] = canvas[y][x - 1];

            if(x - 1 == 0)
            {
                canvas[y][x - 1] = led_off;
            }
        }
    }
}

// 描画バッファ全体を下に１つずらす
static void matrix_scroll_down(void)
{
    int x, y;

    for(y = 0; y < MAT_HEIGHT - 1; y++)
    {
        for(x = 0; x < MAT_WIDTH; x++)
        {
            canvas[y][x] = canvas[y + 1][x];

            if(y + 1 == MAT_HEIGHT - 1)
            {
                canvas[y + 1][x] = led_off;
            }
        }
    }
}

// 描画バッファ全体を上に１つずらす
static void matrix_scroll_up(void)
{
    int x, y;

    for(y = MAT_HEIGHT - 1; 0 < y; y--)
    {
        for(x = 0; x < MAT_WIDTH; x++)
        {
            canvas[y][x] = canvas[y - 1][x];

            if(y - 1 == 0)
            {
                canvas[y - 1][x] = led_off;
            }
        }
    }
}

// 描画バッファ全体を指定した方向に１つずらす
// 上：'u'  下：'d'  左：'l'  右：'r'
void matrix_scroll(const char dir)
{
    switch(dir)
    {  
        case 'u' : matrix_scroll_up();    break; // 上
        case 'd' : matrix_scroll_down();  break; // 下
        case 'l' : matrix_scroll_left();  break; // 左
        case 'r' : matrix_scroll_right(); break; // 右
        default  :                        break;
    }
}

// 描画バッファを表示バッファに反映
void matrix_flush(void)
{
    memmove(display, canvas, sizeof(display));
}

// 指定列の表示バッファをマトリックスLED送信用16bitデータに変換
unsigned short int matrix_convert(const int x)
{
    int y;
    unsigned short int data = 0x0000;
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

// 指定列の16bitデータをマトリックスLEDに出力
void matrix_out(const int x, const unsigned short int data)
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






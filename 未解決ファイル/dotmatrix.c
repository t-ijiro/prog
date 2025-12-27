/*
* DotMatrixProgram
* AVR c
*
* ATMEGA 328P
*
* Created: 2021/05/25
*  Author: H.K.
*  Niigata Polytechnic College
*
*	Ver. 1.0 2021/05/25
*	Ver. 1.1 2021/09/01
*
*
*	ポート変更時の修正箇所
*	・#define SCLK_PULSE 以下
*	・#define DYNAMIC_PORT
*	・#define SW_UP_ON 以下
*	・matrix_k[TATE]の配列の値
*	・matrix_a[YOKO]の配列の値
*	・port_initの代入
*	・set_led内の処理
*	・irq_initのポート設定
*	・ISR(PCINT0_vect) の割り込みベクトル
*
*/

#include <avr/io.h>				// IOポートを使用
#include <avr/interrupt.h>		// 割り込みを使用
#include <stdlib.h>				// 乱数を発生する関数を使用
#include <avr/pgmspace.h>		// プログラム領域にデータを置く
#include <avr/eeprom.h>			// EEPROMを使用

/**********************************************************************
マクロ
***********************************************************************/

//ドットマトリクスのLEDの数
#define TATE 16
#define YOKO 8

//表示モード
#define MODES 9			// 表示モード数
#define MOD_DESIGN 0
#define MOD_OCHI1 1
#define MOD_OCHI2 2
#define MOD_SNAKE1 3
#define MOD_SNAKE2 4
#define MOD_SHOOT1 5
#define MOD_SHOOT2 6
#define MOD_MESSAGE1 7
#define MOD_MESSAGE2 8

// IOポート
//TLC5925
#define SCLK_PULSE PORTC|=0x10;PORTC&=~0x10;	// CLK
#define LAT_PULSE  PORTC|=0x08;PORTC&=~0x08;	// LE
#define BLANK_ON   PORTC&=~0x04;				// OE
#define BLANK_OFF  PORTC|=0x04;
#define SIN_ON     PORTC|=0x20;					//SDI
#define SIN_OFF    PORTC&=~0x20;

//COMMON
#define DYNAMIC_PORT PORTD

//押しボタンスイッチON
#define SW_UP_ON     (PINB&(1<<PINB4))==0
#define SW_DOWN_ON   (PINB&(1<<PINB2))==0
#define SW_LEFT_ON   (PINB&(1<<PINB3))==0
#define SW_RIGHT_ON  (PINB&(1<<PINB0))==0
#define SW_SELECT_ON (PINB&(1<<PINB1))==0
#define SW_MODE_ON   (PINB&(1<<PINB5))==0

//チャタリング時間
#define CHATTERING 600		//600*440us = 0.26s

//ドット表示の横幅
#define DOTMATRIX 32

//ゲームの速度
#define OCHI_TIME_PERIOD_SLOW 30
#define OCHI_TIME_PERIOD_FAST 8
#define SNAKE_TIME_PERIOD_SLOW 36
#define SNAKE_TIME_PERIOD_FAST 12
#define SHOOT_TIME_PERIOD_SLOW 5
#define SHOOT_TIME_PERIOD_FAST 2
#define SHOOT_ENEMY_TIME_PERIOD_SLOW 10
#define SHOOT_ENEMY_TIME_PERIOD_FAST 8


/**********************************************************************
構造体
***********************************************************************/

//XY座標
typedef struct {
	int x;
	int y;
} point;


/**********************************************************************
定数宣言
***********************************************************************/

//LEDの配置(カソード)
//TLC5925の順を16から引く
const int matrix_k[TATE]={10,13,14,8,15,9,11,12,5,0,1,3,2,4,6,7};

//コモン端子の配置(アノード)
const int matrix_a[YOKO]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

//数字
const unsigned char digit_led_dot[13][5]=
{
	{0x7E,0x81,0x81,0x7E,0x00},	//0
	{0x00,0x82,0xFF,0x80,0x00},	//1
	{0xC2,0xA1,0x99,0x86,0x00},	//2
	{0x42,0x81,0x89,0x76,0x00},	//3
	{0x38,0x26,0xFF,0x20,0x00},	//4
	{0x4F,0x85,0x85,0x79,0x00},	//5
	{0x7E,0x89,0x89,0x72,0x00},	//6
	{0x03,0xE1,0x19,0x07,0x00},	//7
	{0x76,0x89,0x89,0x76,0x00},	//8
	{0x4E,0x91,0x91,0x7E,0x00},	//9
	{0x00,0x00,0x00,0x00,0x00},	//スペース
	{0xB8,0xA8,0x2F,0xAA,0xBA},	//点
	{0xFE,0x10,0xFE,0x00,0xE8}  //Hi
};


// -----------------------------------------------
//   ドット表示
// -----------------------------------------------
const int design_led_dot[DOTMATRIX]=

/**********************************************************************
**********************************************************************
この下にExcelから張り付ける
***********************************************************************
***********************************************************************/

{0x0000,0x01D4,0x0154,0x01FE,0x0054,0x0194,0x0000,0x0000,0x0000,0x00E4,0x0104,0x0106,0x011C,0x0124,0x0004,0x0000,0x0208,0x0284,0x032A,0x03FA,0x032A,0x0284,0x0208,0x0000,0x0000,0x0204,0x0224,0x0224,0x03FC,0x0224,0x02A4,0x0304};


// -----------------------------------------------
//   落ちものゲーム
// -----------------------------------------------
//コマのパターンは6パターン
const int koma_patterns = 6;

//コマのパターン
const unsigned char koma[6][4][4]={
	{{0,0,0,0},{5,5,5,5},{0,0,0,0},{0,0,0,0}},
	{{0,0,0,0},{5,5,5,0},{5,0,0,0},{0,0,0,0}},
	{{0,0,0,0},{5,5,5,0},{0,0,5,0},{0,0,0,0}},
	{{0,0,0,0},{5,5,0,0},{0,5,5,0},{0,0,0,0}},
	//	{{0,0,0,0},{0,5,5,0},{5,5,0,0},{0,0,0,0}},
	{{0,0,0,0},{5,5,0,0},{5,5,0,0},{0,0,0,0}},
	{{0,0,0,0},{5,5,5,0},{0,5,0,0},{0,0,0,0}}
};


// -----------------------------------------------
//   へびゲーム
// -----------------------------------------------


// -----------------------------------------------
//   撃ちものゲーム
// -----------------------------------------------


// -----------------------------------------------
//   メッセージ表示
// -----------------------------------------------

//メッセージ１
const PROGMEM int message1[][16]={
	{0x0000,0x4288,0x2298,0x1AE8,0xFF8E,0x0ACA,0x32B8,0x8288,0x6000,0x1FFC,0x0088,0x0088,0x0088,0xFF84,0x0086,0x00C4}, // 1   新
	{0x0000,0x0810,0xF862,0x070C,0x00E0,0x4800,0x3400,0x02FC,0x7394,0x0292,0x3280,0x0294,0x4A94,0xD2FC,0x4200,0x3E00}, // 2   潟
	{0x0000,0x2004,0x3FFC,0x1124,0xFFFC,0x0044,0x0048,0x3F78,0x254E,0xA54A,0xBF78,0x4048,0x27FE,0x3842,0x4748,0xB050}, // 3   職
	{0x0000,0x4408,0x444A,0x454C,0x2548,0x2558,0x156E,0x0D4A,0xFFC8,0x0D48,0x156E,0x255A,0x2548,0x454C,0x446A,0x4608}, // 4   業
	{0x0000,0x0020,0xFF60,0x1538,0x1526,0x9522,0x9528,0xFF90,0x0160,0x0000,0x7E7E,0x9292,0x9090,0x8888,0x8C8C,0xE0E0}, // 5
	{0x0000,0x0000,0x8020,0x4020,0x2020,0x1820,0x0620,0x01FE,0x0022,0x4020,0x4020,0xC020,0x6020,0x1FF0,0x0020,0x0000}, // 6   力
	{0x0000,0xFFFC,0x0054,0x0054,0x4954,0x2954,0x1F7E,0x0904,0x0900,0x7F7C,0x0954,0x0954,0x4054,0xC054,0x7FFE,0x0004}, // 7   開
	{0x0000,0x0100,0x8514,0x84A4,0x44C4,0x34A4,0x0F9C,0x0484,0x0480,0x3F86,0x4498,0x44A0,0x4448,0x44A4,0x7510,0x2100}, // 8   発
	{0x0000,0x8130,0x611E,0x1FF2,0x0510,0x1910,0x4004,0x43E4,0x4A24,0x7224,0x4224,0x6224,0x5A24,0x43F4,0x6024,0x4000}, // 9   短
	{0x0000,0x8808,0x6FFE,0x0948,0x0948,0x2948,0xCFFE,0x0808,0x8000,0x6000,0x1FFC,0x0444,0x4444,0xC444,0x7FFE,0x0004}, // 10   期
	{0x0000,0x8020,0x8020,0x4020,0x2020,0x1020,0x0C20,0x0320,0x00FE,0x0322,0x0C20,0x1020,0x2020,0x4020,0x8030,0x8020}, // 11   大
	{0x0000,0x0860,0x0838,0x0812,0x089C,0x0890,0x4890,0xC892,0x7E9C,0x0A90,0x09D0,0x0898,0x0816,0x0852,0x0C38,0x0810}, // 12   学
	{0x0000,0x0810,0x0610,0x01D0,0xFFFE,0x0092,0x8310,0x8088,0x4068,0x4328,0x2408,0x180E,0x2C0A,0x4328,0x804C,0x8188}, // 13   校
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000}, // 14
	{0x0000,0x0030,0x0018,0x000A,0x7EAA,0x2AAA,0x2AAA,0x2A0A,0x7EFE,0xAA0A,0xAAAA,0xAAAA,0xBEAA,0x800A,0xF038,0x4008}, // 15   電
	{0x0000,0x0100,0x0100,0x0104,0x0104,0x4104,0x4104,0xC104,0x7FE4,0x0124,0x0114,0x010C,0x010E,0x0104,0x0180,0x0100}, // 16   子
	{0x0000,0x00F0,0x0000,0xFFFE,0x0012,0x0060,0x0000,0x0044,0xFF54,0x1554,0x1554,0x157E,0x9554,0x9554,0xFF54,0x0044}, // 17   情
	{0x0000,0x0220,0x0A68,0x0BA8,0xFE3E,0x0B2A,0x0AE8,0x0220,0x0000,0xFFFC,0x8104,0x4704,0x3924,0x2924,0x473E,0x8184}, // 18   報
	{0x0000,0x4420,0xC420,0x7FFE,0x0222,0x0220,0x8010,0x8110,0x4310,0x4D10,0x31FE,0x3112,0x4D10,0x4390,0x8118,0x8010}, // 19   技
	{0x0000,0x0420,0x0218,0xFF86,0x0072,0x0810,0x0720,0x0020,0xFFFE,0x0022,0x0124,0x0E08,0x4084,0xC084,0x7F84,0x00C6}, // 20   術
	{0x0000,0x2048,0x1848,0x0748,0xFFFC,0x0146,0x0664,0x0800,0x0840,0x0984,0x0818,0x0800,0x0400,0xFFFE,0x0402,0x0600}, // 21   科
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000}, // 22
};

//メッセージ2
const PROGMEM int message2[][16]={
	{0x0000,0x0038,0x000A,0x3EAA,0x2AAA,0x2AAA,0x2AAA,0x2A0A,0xFEFE,0xAA0A,0xAAAA,0xAAAA,0xAAAA,0xBEAA,0x800A,0xE038}, // 0   電
	{0x0000,0x0100,0x0100,0x0104,0x0104,0x0104,0x8104,0x8104,0xFFE4,0x0124,0x0114,0x0114,0x010C,0x0104,0x0100,0x0100}, // 1   子
	{0x0000,0x0000,0xFFFC,0x8004,0x8004,0x8004,0x8FC4,0x8844,0x8844,0x8844,0x8FC4,0x8004,0x8004,0x8004,0xFFFC,0x0000}, // 2   回
	{0x0000,0x8000,0xFE7C,0x8044,0xFFC4,0x4444,0x447C,0x0000,0x0420,0xFC18,0x8A6E,0x8988,0x8988,0x8A68,0xFC18,0x0400}, // 3   路
	{0x0000,0x0010,0xF954,0x8954,0x8954,0xF954,0x0010,0x8180,0x8160,0x471C,0x5904,0x2104,0x517C,0x4D40,0x8340,0x8070}, // 4   設
	{0x0000,0x0010,0xF954,0x8954,0x8954,0x8954,0xF954,0x0010,0x0040,0x0040,0x0040,0xFFFE,0x0040,0x0040,0x0040,0x0040}, // 5   計
	{0x0000,0x2418,0x25D4,0x9456,0x9454,0xFFFE,0x4C54,0x4554,0x47D4,0x0C10,0x1400,0x247C,0x5400,0x4D00,0x85FE,0x8400}, // 6   製
	{0x0000,0x0080,0x0060,0xFFF8,0x0006,0x0080,0x0040,0x0020,0x0018,0x0016,0xFFF0,0x0910,0x0910,0x0910,0x0910,0x0010}, // 7   作
	{0x0000,0x8838,0x8808,0x8948,0x4948,0x4948,0x2948,0x1948,0x0FEE,0x1948,0x2948,0x4948,0x4948,0x8948,0x8808,0x8838}, // 8   実
	{0x0000,0x0004,0x004C,0xFE54,0x9224,0x9224,0x9284,0x92FC,0x9300,0x9204,0x924C,0x9254,0x9224,0xFE24,0x0084,0x00FC}, // 9   習
	{0x0000,0x0000,0x0008,0x0008,0x0008,0x0008,0x0F08,0x3FCC,0x30E4,0x6034,0x401C,0x400C,0x406C,0x4064,0x0034,0x0034}, // 10   で
	{0x0000,0x2418,0x25D4,0x9456,0x9454,0xFFFE,0x4C54,0x4554,0x47D4,0x0C10,0x1400,0x247C,0x5400,0x4D00,0x85FE,0x8400}, // 11   製
	{0x0000,0x0080,0x0060,0xFFF8,0x0006,0x0080,0x0040,0x0020,0x0018,0x0016,0xFFF0,0x0910,0x0910,0x0910,0x0910,0x0010}, // 12   作
	{0x0000,0x0000,0x0000,0x0000,0x1FFC,0x3FFC,0x6000,0x4000,0x4000,0x4000,0x6000,0x2000,0x3000,0x1800,0x0E00,0x0600}, // 13   し
	{0x0000,0x0000,0x7010,0x7F10,0x0FF0,0x00FE,0x001E,0x0008,0x3808,0x7848,0x4040,0x4040,0x4040,0x4040,0x4040,0x4000}, // 14   た
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x7FFC,0x7FFC,0x0080,0x0180,0x0100,0x0300,0x020C,0x020C,0x000C,0x000C}, // 15   ド
	{0x0000,0x0000,0x0000,0x0000,0x0080,0x4380,0x4300,0x6040,0x21C0,0x3180,0x1800,0x0E00,0x0780,0x0180,0x0000,0x0000}, // 16   ッ
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x7FFC,0x7FFC,0x0080,0x0180,0x0100,0x0300,0x0200,0x0200,0x0000,0x0000}, // 17   ト
	{0x0000,0x0000,0x0010,0x0010,0x0210,0x0210,0x0610,0x0410,0x0C10,0x1C10,0x3610,0x6310,0x4190,0x00D0,0x0070,0x0030}, // 18   マ
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x7FFC,0x7FFC,0x0080,0x0180,0x0100,0x0300,0x0200,0x0200,0x0000,0x0000}, // 19   ト
	{0x0000,0x0000,0x0000,0x0000,0x03FC,0x03FC,0x0000,0x8000,0x8000,0xC000,0x6000,0x3800,0x1FFC,0x07FC,0x0000,0x0000}, // 20   リ
	{0x0000,0x0000,0x0000,0x0100,0x8180,0x80C0,0xC070,0x403E,0x601E,0x3010,0x1810,0x0E10,0x0790,0x01F0,0x0070,0x0000}, // 21   ク
	{0x0000,0x0000,0x4000,0x4008,0x6008,0x2008,0x3008,0x1808,0x0C08,0x0708,0x07C8,0x0CF8,0x1838,0x3000,0x6000,0x4000}, // 22   ス
	{0x0000,0x2100,0xA108,0x9128,0x9128,0xF928,0x8528,0x4328,0x41FE,0x4728,0x1928,0x2128,0x5128,0x4928,0x8508,0x8100}, // 23
	{0x0000,0x2040,0x1040,0x0844,0x0644,0x0044,0x8044,0x8044,0xFFC4,0x0044,0x0044,0x0044,0x0244,0x0444,0x0840,0x3040}, // 24   示
	{0x0000,0x1100,0xF13C,0x9924,0x9924,0x9524,0xF33C,0x0180,0x0140,0x0100,0xF13C,0x9324,0x9524,0x9924,0xF93C,0x1100}, // 25   器
	{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000} // 26
};


/**********************************************************************
変数宣言
***********************************************************************/

// -----------------------------------------------
//   ドットマトリクス
// -----------------------------------------------
//ポート出力
int led_out[TATE];

//ダイナミック点灯
volatile int dynamic=0;

// -----------------------------------------------
//   共通変数
// -----------------------------------------------
//LEDのドット 0:消灯 1:赤 5:赤移動対象
volatile int led_dot[YOKO][TATE];

//モード選択
volatile unsigned int mode=0;

//タイマ割り込みの分周
volatile unsigned int time_count=0;
volatile unsigned int time_period=50;

//チャタリング防止
volatile unsigned int sw_period=0;

//割り込み処理
volatile char sw_flag=0;

//繰り返し時間
volatile unsigned int repeat_time=0;
volatile unsigned int repeat_time_score=10;

//ゲーム動作
volatile int gameplay[MODES];

//得点
volatile unsigned int game_score[MODES];
volatile int game_score_len[MODES];
volatile int game_score_pos[MODES];

//最高得点
volatile unsigned int game_hiscore[MODES];
volatile int game_hiscore_len[MODES];
volatile int game_hiscore_pos[MODES];

//得点の数字データ
int digit_score[20];

//最高得点の数字データ
int digit_hiscore[20];

//EEPROM
static uint16_t  EEMEM EEPROM_MODE;
static uint8_t  EEMEM EEPROM_HISCORE[MODES*2];


// -----------------------------------------------
//   ドット表示
// -----------------------------------------------
// -----------------------------------------------
//  メッセージ表示
// -----------------------------------------------

//メッセージデータ
int message_len[MODES];

//メッセージの表示状態
volatile int message_pos=0;
volatile int message_time=1;
volatile int message_ypos=0;
volatile int message_dir=1;


// -----------------------------------------------
//   落ちものゲーム
// -----------------------------------------------

//4x4の左上
point ochi_koma_pos;

//コマの回転時
int ochi_dot_rot[4][4];

//下ボタンのコマ数
volatile int ochi_down_all=0;

//ゲーム動作タイマー
volatile unsigned int ochi_time_period;


// -----------------------------------------------
//   へびのゲーム
// -----------------------------------------------

//へび
point snake_body[50];
volatile int snake_len;

//方向
point snake_dir;

//餌
point snake_food;
point snake_last;

//ゲーム動作タイマー
volatile unsigned int snake_time_period;


// -----------------------------------------------
//   撃ちものゲーム
// -----------------------------------------------

//ガンの位置
volatile int shoot_pos;

//弾の状態
volatile int shoot_shot_on;
point shoot_shot;

//敵の状態
volatile int shoot_enemy_on;
point shoot_enemy;

//敵の弾の状態
volatile int shoot_enemy_shot_on;
point shoot_enemy_shot;

//ゲーム動作タイマー
volatile unsigned int shoot_time_period;

//敵の弾の動作タイマー
volatile unsigned int shoot_enemy_time;
volatile unsigned int shoot_enemy_time_period;


/**********************************************************************
ポート関連
***********************************************************************/
//ポート初期化
void port_init()
{
	DDRC=0xff;		//TLC5925出力
	DDRD=0xff;		//LEDコモン出力

	PORTC=0x00;		//LED消灯
	PORTD=0xFF;		//LED消灯
	
	PORTB=0x3F;		//PC0-PC5:SW	プルアップ
}


/**********************************************************************
EEPROM関連
***********************************************************************/
//読み込み
void eeprom_read_all()
{
	eeprom_busy_wait();
	mode=eeprom_read_word(&EEPROM_MODE);
	if(mode<0 || mode>=MODES)
	{
		mode=0;
		eeprom_busy_wait();
		eeprom_write_word(&EEPROM_MODE, mode);
	}

	unsigned int score;
	for(int m=0;m<MODES;m++){
		eeprom_busy_wait();
		score=eeprom_read_byte(&EEPROM_HISCORE[2*m])*256;
		
		eeprom_busy_wait();
		score+=eeprom_read_byte(&EEPROM_HISCORE[2*m+1]);
		
		if(score<0 || score>=65000)
		{
			score=0;
			eeprom_busy_wait();
			eeprom_write_byte(&EEPROM_HISCORE[2*m], 0);
	
			eeprom_busy_wait();
			eeprom_write_byte(&EEPROM_HISCORE[2*m+1], 0);
		}
		
		game_hiscore[m]=score;
	}
}


//モード書き込み
void eeprom_write_mode()
{
	eeprom_busy_wait();
	eeprom_write_word(&EEPROM_MODE, mode);
}


//ハイスコア書き込み
void eeprom_write_hiscore()
{
	eeprom_busy_wait();
	eeprom_write_byte(&EEPROM_HISCORE[2*mode], (uint8_t)(game_hiscore[mode]/256));
	
	eeprom_busy_wait();
	eeprom_write_byte(&EEPROM_HISCORE[2*mode+1], (uint8_t)(game_hiscore[mode]%256));
}


/**********************************************************************
LED出力設定
***********************************************************************/
//ドットマトリクスのデータを、LED制御用のデータに変換
void set_led()
{
	int d;
	
	//ドットマトリクスの型番が左側
	for(int y=0;y<8;y++)
	{
		d=0;
		for(int x=0;x<YOKO;x++)
		{
			if(led_dot[x][y])
			{
				d|=1<<(7-x);
			}
		}
		led_out[matrix_k[y]]=d;
	}

	for(int y=8;y<TATE;y++)
	{
		d=0;
		for(int x=0;x<YOKO;x++)
		{
			if(led_dot[x][y])
			{
				d|=1<<(7-x);
			}
		}
		led_out[matrix_k[y]]=d;
	}

/*
	//ドットマトリクスの型番が下側
	for(int x=0;x<YOKO;x++)
	{
		d=0;
		for(int y=0;y<8;y++)
		{
			if(led_dot[x][y])
			{
				d|=1<<y;
			}
		}
		led_out[matrix_k[x]]=d;

		d=0;
		for(int y=8;y<TATE;y++)
		{
			if(led_dot[x][y])
			{
				d|=1<<(y-8);
			}
		}
		led_out[matrix_k[x+8]]=d;
	}
*/

}


/**********************************************************************
モード共通処理
***********************************************************************/

//全ドットクリア
void dot_clear()
{
	for(int x=0;x<YOKO;x++)
	{
		for(int y=0;y<TATE;y++)
		{
			led_dot[x][y]=0;
		}
	}
}


//ゲームオーバーの表示
//点数データ設定
void gameover()
{
	dot_clear();
	set_led();

	//点数データ作成
	game_score_len[mode]=0;
	int op=game_score[mode];

	//スペース
	digit_score[game_score_len[mode]]=10;
	game_score_len[mode]++;
	digit_score[game_score_len[mode]]=10;
	game_score_len[mode]++;
	
	//点
	digit_score[game_score_len[mode]]=11;
	game_score_len[mode]++;
	
	do{
		digit_score[game_score_len[mode]]=op%10;
		game_score_len[mode]++;
		op/=10;
	}
	while(op);

	//スペース
	digit_score[game_score_len[mode]]=10;

	game_score_pos[mode]=0;


	//最高得点の更新
	if(game_score[mode]>=game_hiscore[mode])
	{
		game_hiscore[mode]=game_score[mode];
		
		eeprom_write_hiscore();
	}
		
	//最高点数データ作成
	game_hiscore_len[mode]=0;
	op=game_hiscore[mode];

	//スペース
	digit_hiscore[game_hiscore_len[mode]]=10;
	game_hiscore_len[mode]++;
	digit_hiscore[game_hiscore_len[mode]]=10;
	game_hiscore_len[mode]++;
		
	//点
	digit_hiscore[game_hiscore_len[mode]]=11;
	game_hiscore_len[mode]++;
		
	do{
		digit_hiscore[game_hiscore_len[mode]]=op%10;
		game_hiscore_len[mode]++;
		op/=10;
	}
	while(op);

	//スペース
	digit_hiscore[game_hiscore_len[mode]]=10;
	game_hiscore_len[mode]++;
		
	//Hi
	digit_hiscore[game_hiscore_len[mode]]=12;
	game_hiscore_len[mode]++;

	//スペース
	digit_hiscore[game_hiscore_len[mode]]=10;

	game_hiscore_pos[mode]=0;
}


//点数データ表示
void show_score()
{
	//最高得点の表示
	//左シフトの量
	game_hiscore_pos[mode]++;
	if(game_hiscore_pos[mode]>(game_hiscore_len[mode])*5-4)
	{
		game_hiscore_pos[mode]=0;
	}

	//データ設定
	for(int x=0;x<YOKO;x++)
	{
		for(int y=0;y<8;y++)
		{
			if(digit_led_dot[digit_hiscore[game_hiscore_len[mode]-((x+game_hiscore_pos[mode])/5)]][(x+game_hiscore_pos[mode])%5]&(1<<y))
			{
				led_dot[x][y]=1;
			}
			else
			{
				led_dot[x][y]=0;
			}
		}
	}
	
	//得点の表示
	//左シフトの量
	game_score_pos[mode]++;
	if(game_score_pos[mode]>(game_score_len[mode])*5-4)
	{
		game_score_pos[mode]=0;
	}

	//データ設定
	for(int x=0;x<YOKO;x++)
	{
		for(int y=0;y<8;y++)
		{
			if(digit_led_dot[digit_score[game_score_len[mode]-((x+game_score_pos[mode])/5)]][(x+game_score_pos[mode])%5]&(1<<y))
			{
				led_dot[x][y+8]=1;
			}
			else
			{
				led_dot[x][y+8]=0;
			}
		}
	}
	
	set_led();
}


/**********************************************************************
ドット表示処理
メッセージ表示処理
***********************************************************************/

//メッセージ速度変更
void message_speed(int speed)
{
	if(speed>0){
		if(message_time==0)
		{
			message_time=1;
		}
		else if(message_time<32)
		{
			message_time*=2;
		}
	}
	else
	{
		if(message_time==1)
		{
			message_time=0;
		}
		else
		{
			message_time/=2;
		}
	}
}

//メッセージ上下移動
void message_updown(int updown)
{
	message_ypos+=updown;
	message_ypos%=TATE;
	if(message_ypos<0)
	{
		message_ypos=TATE-1;
	}
}

//メッセージ横移動方向
void message_dir_change()
{
	message_dir*=(-1);
}


//ドットデータ表示
void design_show()
{
	//左シフトの量
	message_pos+=message_dir;
	if(message_pos>DOTMATRIX)
	{
		message_pos=0;
	}
	if(message_pos<0)
	{
		message_pos=DOTMATRIX;
	}

	//データ設定
	for(int x=0;x<YOKO;x++)
	{
		for(int y=0;y<TATE;y++)
		{
			if(design_led_dot[(x+message_pos)%DOTMATRIX]&(1<<((y+message_ypos)%TATE)))
			{
				led_dot[x][y]=1;
			}
			else
			{
				led_dot[x][y]=0;
			}
		}
	}
	set_led();
}


//メッセージ表示
void message_show()
{
	int pgm_word;
	
	//左シフトの量
	message_pos+=message_dir;
	if(message_pos>(message_len[mode]-1)*16)
	{
		message_pos=0;
	}
	if(message_pos<0)
	{
		message_pos=(message_len[mode]-1)*16;
	}

	//データ設定
	for(int x=0;x<YOKO;x++)
	{
		for(int y=0;y<TATE;y++)
		{
			switch(mode)
			{
				case MOD_MESSAGE1:
					pgm_word=pgm_read_word(&message1[(x+message_pos)/TATE][(x+message_pos)%TATE]);
					break;
				default:
					pgm_word=pgm_read_word(&message2[(x+message_pos)/TATE][(x+message_pos)%TATE]);
			}
			
			if(pgm_word&(1<<((y+message_ypos)%TATE)))
			{
				led_dot[x][y]=1;
			}
			else
			{
				led_dot[x][y]=0;
			}
		}
	}
	set_led();
}


//メッセージ初期化
void message_init()
{
	dot_clear();
	
	switch(mode)
	{
		case MOD_MESSAGE1:
			message_len[mode]=sizeof(message1)/sizeof(*message1);
			break;
		case MOD_MESSAGE2:
			message_len[mode]=sizeof(message2)/sizeof(*message2);
			break;
	}
	
	message_pos=0;
	message_time=2;
	message_ypos=0;
	message_dir=1;
	
	set_led();
}


/**********************************************************************
落ちものゲーム処理
***********************************************************************/

//新しいコマの作成
void ochi_new_koma()
{
	//新しいコマを乱数で選ぶ
	int r=(time_count+rand())%koma_patterns;

	ochi_down_all=0;

	//コマの初期位置
	ochi_koma_pos.x=2;
	ochi_koma_pos.y=0;
	
	//新しいコマを設定
	for(int dy=3;dy>=0;dy--)
	{
		for(int dx=0;dx<4;dx++)
		{
			//ゲームオーバーの判定
			if(koma[r][dx][dy]==5 && led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==1)
			{
				gameplay[mode]=0;
			}

			//コマのドットを代入
			if(koma[r][dx][dy]==5)
			{
				led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=koma[r][dx][dy];
			}
		}
	}

	//ゲーム終了
	if(!gameplay[mode])
	{
		gameover();
	}
}


//横一行が揃っているかチェック
int ochi_match(int y)
{
	for(int x=0;x<YOKO;x++)
	{
		if(led_dot[x][y]==0)
		{
			return 0;
		}
	}
	return 1;
}


//次に下げられるかチェック
void ochi_check()
{
	int chk=0;
	
	//チェック
	for(int dy=0;dy<4;dy++)
	{
		for(int dx=0;dx<4;dx++)
		{
			if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5 && (ochi_koma_pos.y+dy+1==TATE || led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy+1]==1))
			{
				chk=1;
				break;
			}
		}
	}
	
	//もう下げられない
	if(chk)
	{
		//確定する
		for(int dy=0;dy<4;dy++)
		{
			for(int dx=0;dx<4;dx++)
			{
				if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
				{
					led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=1;
				}
			}
		}
		
		int lines=0;
		
		//揃っているかの判定
		for(int y=TATE-1;y>=0;y--)
		{
			//複数揃っている間繰り返す
			while(ochi_match(y))
			{
				lines++;
				
				//揃った行の上を全体に１つ下げる
				for(int sy=y;sy>0;sy--)
				{
					for(int x=0;x<YOKO;x++)
					{
						led_dot[x][sy]=led_dot[x][sy-1];
					}
				}
				for(int x=0;x<YOKO;x++)
				{
					led_dot[x][0]=0;
				}
			}
		}
		
		//揃っていた行の点数を加える
		if(lines)
		{
			game_score[mode]+=(1<<(lines-1))*20;
		}

		//次のコマを作る
		ochi_new_koma();
	}
}


//ゲームの初期化
void ochi_init()
{
	dot_clear();

	gameplay[mode]=1;

	switch(mode){
		case MOD_OCHI1:
			ochi_time_period=OCHI_TIME_PERIOD_SLOW;
			break;
		case MOD_OCHI2:
			ochi_time_period=OCHI_TIME_PERIOD_FAST;
			break;
	}
	
	//乱数初期化
	srand((int)time_count);

	game_score[mode]=0;

	ochi_new_koma();
	
	set_led();
}


//左移動
void ochi_left()
{
	if(gameplay[mode])
	{
		int leftctrl=1;
		
		//左にコマがないかをチェック
		for(int dx=0;dx<4;dx++)
		{
			for(int dy=0;dy<4;dy++)
			{
				if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
				{
					if(ochi_koma_pos.x+dx==0 || led_dot[ochi_koma_pos.x+dx-1][ochi_koma_pos.y+dy]==1)
					{
						leftctrl=0;
						break;
					}
				}
			}
		}
		
		//コマを左に動かす
		if(leftctrl)
		{
			for(int dx=0;dx<4;dx++)
			{
				for(int dy=0;dy<4;dy++)
				{
					if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
					{
						led_dot[ochi_koma_pos.x+dx-1][ochi_koma_pos.y+dy]=led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy];
						led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=0;
					}
				}
			}
			ochi_koma_pos.x--;
			ochi_check();
			
			set_led();
		}
	}
}


//右移動
void ochi_right()
{
	if(gameplay[mode])
	{
		int rightctrl=1;
		
		//右にコマがないかをチェック
		for(int dx=3;dx>=0;dx--)
		{
			for(int dy=0;dy<4;dy++)
			{
				if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
				{
					if(ochi_koma_pos.x+dx==YOKO-1 || led_dot[ochi_koma_pos.x+dx+1][ochi_koma_pos.y+dy]==1)
					{
						rightctrl=0;
						break;
					}
				}
			}
		}
		
		//コマを右に動かす
		if(rightctrl)
		{
			for(int dx=3;dx>=0;dx--)
			{
				for(int dy=0;dy<4;dy++){
					if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
					{
						led_dot[ochi_koma_pos.x+dx+1][ochi_koma_pos.y+dy]=led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy];
						led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=0;
					}
				}
			}
			ochi_koma_pos.x++;
			ochi_check();
			
			set_led();
		}
	}
}


//回転
void ochi_rotate()
{
	if(gameplay[mode])
	{
		//横にシフトする量の初期化
		int xl=0;
		int xr=YOKO-1;
		
		//回転して別の配列に代入
		for(int dx=0;dx<4;dx++)
		{
			for(int dy=0;dy<4;dy++)
			{
				if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5){
					ochi_dot_rot[3-dy][dx]=led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy];
					
					if(ochi_koma_pos.x+3-dy<xl)
					{
						xl=ochi_koma_pos.x+3-dy;
					}
					if(ochi_koma_pos.x+3-dy>xr)
					{
						xr=ochi_koma_pos.x+3-dy;
					}
				}
				else
				{
					ochi_dot_rot[3-dy][dx]=0;
				}
			}
		}
		
		//両側を超えていたら、シフトする
		ochi_koma_pos.x+=-xl;
		ochi_koma_pos.x-=(xr-(YOKO-1));
		
		//回転した配列を代入
		for(int dx=0;dx<4;dx++)
		{
			for(int dy=0;dy<4;dy++)
			{
				led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=ochi_dot_rot[dx][dy];
			}
		}
		set_led();
	}
	else
	{
		//ゲーム再開
		ochi_init();
	}
}


//下移動
void ochi_down()
{
	if(gameplay[mode])
	{
		//下げる
		for(int dy=3;dy>=0;dy--)
		{
			for(int dx=0;dx<4;dx++)
			{
				if(led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]==5)
				{
					led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy+1]=led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy];
					led_dot[ochi_koma_pos.x+dx][ochi_koma_pos.y+dy]=0;
				}
			}
		}
		ochi_koma_pos.y++;

		ochi_check();
		set_led();
	}
}


/**********************************************************************
へびのゲーム処理
***********************************************************************/

//新しい餌の追加
void snake_new_food()
{
	//新しい餌を乱数で作る
	snake_food.x=1+(time_count+rand())%(YOKO-2);
	snake_food.y=1+(time_count+rand())%(TATE-2);
}


//餌を取ったかどうかチェック
void snake_food_check()
{
	if(snake_body[0].x==snake_food.x && snake_body[0].y==snake_food.y)
	{
		snake_len++;
		snake_body[snake_len].x=snake_last.x;
		snake_body[snake_len].y=snake_last.y;
		
		snake_new_food();
	}
}


//ゲーム終了のチェック
void snake_game_check()
{
	//境界に移動した
	if(snake_body[0].x<0 || snake_body[0].x>YOKO-1 || snake_body[0].y<0 || snake_body[0].y>TATE-1 )
	{
		gameplay[mode]=0;
	}

	//自分に当たった
	for(int i=1;i<=snake_len;i++)
	{
		if(snake_body[0].x==snake_body[i].x && snake_body[0].y==snake_body[i].y)
		{
			gameplay[mode]=0;
		}
	}

	//終了処理へ
	if(!gameplay[mode])
	{
		game_score[mode]=snake_len;
		gameover();
	}
}


//へびの表示を設定
void snake_dot_set()
{
	dot_clear();

	for(int i=0;i<=snake_len;i++)
	{
		led_dot[snake_body[i].x][snake_body[i].y]=1;
	}

	led_dot[snake_food.x][snake_food.y]=1;

	set_led();
}


//へびの移動
void snake_move()
{
	snake_last.x=snake_body[snake_len].x;
	snake_last.y=snake_body[snake_len].y;

	for(int i=snake_len;i>0;i--)
	{
		snake_body[i].x=snake_body[i-1].x;
		snake_body[i].y=snake_body[i-1].y;
	}

	snake_body[0].x+=snake_dir.x;
	snake_body[0].y+=snake_dir.y;

	snake_food_check();
	snake_dot_set();

	snake_game_check();
}


//左右方向変更
void snake_dir_change(int x,int y)
{
	if(gameplay[mode])
	{
		snake_dir.x=x;
		snake_dir.y=y;
		
		snake_move();
	}
}


//ゲームの初期化
void snake_init()
{
	gameplay[mode]=1;
	
	srand((int)time_count);

	game_score[mode]=0;
	
	switch(mode){
		case MOD_SNAKE1:
			snake_time_period=SNAKE_TIME_PERIOD_SLOW;
			break;
		case MOD_SNAKE2:
			snake_time_period=SNAKE_TIME_PERIOD_FAST;
			break;
	}

	snake_dir.x=0;
	snake_dir.y=1;
	
	snake_len=0;
	snake_body[snake_len].x=3;
	snake_body[snake_len].y=7;
	
	snake_new_food();
	snake_dot_set();
}


/**********************************************************************
撃ちものゲーム処理
***********************************************************************/

//敵を作る
void shoot_enemy_new()
{
	shoot_enemy_on=(time_count+rand())%7+1;
	shoot_enemy.x=shoot_enemy_on-1;
	shoot_enemy.y=0;
}


//敵を動かす
void shoot_enemy_move()
{
	led_dot[shoot_enemy.x][shoot_enemy.y]=0;
	led_dot[shoot_enemy.x+1][shoot_enemy.y]=0;
	
	shoot_enemy_on++;
	if(shoot_enemy_on<8)
	{
		shoot_enemy.x++;
	}
	else if(shoot_enemy_on==8)
	{
		shoot_enemy.y++;
	}
	else if(shoot_enemy_on<15)
	{
		shoot_enemy.x--;
	}
	else if(shoot_enemy_on==15)
	{
		shoot_enemy.y++;
		shoot_enemy_on=1;
	}

	led_dot[shoot_enemy.x][shoot_enemy.y]=1;
	led_dot[shoot_enemy.x+1][shoot_enemy.y]=1;
	
	//敵に侵略された
	if(shoot_enemy.y>=14)
	{
		gameplay[mode]=0;
		gameover();
	}

	set_led();
}


//敵が弾を打つ
void shoot_enemy_shoot()
{
	if(!shoot_enemy_shot_on)
	{
		shoot_enemy_shot_on=1;
		shoot_enemy_shot.x=shoot_enemy.x;
		shoot_enemy_shot.y=shoot_enemy.y;
	}
}


//敵の弾を動かす
void shoot_enemy_shot_move()
{
	led_dot[shoot_enemy_shot.x][shoot_enemy_shot.y]=0;
	
	if(shoot_enemy_shot.y<TATE)
	{
		shoot_enemy_shot.y+=1;
		led_dot[shoot_enemy_shot.x][shoot_enemy_shot.y]=1;
		
		//ガンに当たった
		if((shoot_enemy_shot.x==shoot_pos && shoot_enemy_shot.y==14)
		|| ((shoot_enemy_shot.x==shoot_pos-1 || shoot_enemy_shot.x==shoot_pos+1) && shoot_enemy_shot.y==15))
		{
			gameplay[mode]=0;
			gameover();
		}
	}
	else
	{
		shoot_enemy_shot_on=0;
	}
	
	set_led();
}


//ガンから弾を打つ
void shoot_shoot()
{
	if(!shoot_shot_on)
	{
		shoot_shot_on=1;
		shoot_shot.x=shoot_pos;
		shoot_shot.y=13;
	}
	set_led();
}


//ガンから打った弾を動かす
void shoot_shot_move()
{
	led_dot[shoot_shot.x][shoot_shot.y]=0;
	
	if(shoot_shot.y-1>=0)
	{
		shoot_shot.y-=1;
		led_dot[shoot_shot.x][shoot_shot.y]=1;
		
		//敵に当たった
		if((shoot_shot.x==shoot_enemy.x || shoot_shot.x==shoot_enemy.x+1) && shoot_shot.y==shoot_enemy.y)
		{
			led_dot[shoot_enemy.x][shoot_enemy.y]=0;
			led_dot[shoot_enemy.x+1][shoot_enemy.y]=0;
			
			//下で当たるほど点が高い
			game_score[mode]+=shoot_shot.y;
			shoot_enemy_new();
		}
	}
	else
	{
		shoot_shot_on=0;
	}
	
	set_led();
}


//ガンを描く
void shoot_gun_show()
{
	for(int x=0;x<YOKO;x++)
	{
		led_dot[x][14]=0;
		led_dot[x][15]=0;
	}
	led_dot[shoot_pos][14]=1;
	led_dot[shoot_pos-1][15]=1;
	led_dot[shoot_pos][15]=1;
	led_dot[shoot_pos+1][15]=1;

	set_led();
}


//ガンを動かす
void shoot_gun_move(int x)
{
	if(shoot_pos+x>0 && shoot_pos+x<YOKO-1)
	{
		shoot_pos+=x;
	}
	shoot_gun_show();
}


//初期化
void shoot_init()
{
	dot_clear();
	
	gameplay[mode]=1;
	game_score[mode]=0;

	switch(mode){
		case MOD_SHOOT1:
			shoot_time_period=SHOOT_TIME_PERIOD_SLOW;
			shoot_enemy_time_period=SHOOT_ENEMY_TIME_PERIOD_SLOW;
			break;
		case MOD_SHOOT2:
			shoot_time_period=SHOOT_TIME_PERIOD_FAST;
			shoot_enemy_time_period=SHOOT_ENEMY_TIME_PERIOD_FAST;
			break;
	}

	shoot_pos=3;
	shoot_shot_on=0;
	shoot_enemy_on=0;
	
	shoot_gun_show();
	shoot_enemy_new();
}


/**********************************************************************
モード共通処理
***********************************************************************/
//モード初期化
void mode_init()
{
	switch(mode){
		case MOD_DESIGN:
			message_init();
			break;
		case MOD_OCHI1:
		case MOD_OCHI2:
			ochi_init();
			break;
		case MOD_SNAKE1:
		case MOD_SNAKE2:
			snake_init();
			break;
		case MOD_SHOOT1:
		case MOD_SHOOT2:
			shoot_init();
			break;
		case MOD_MESSAGE1:
		case MOD_MESSAGE2:
			message_init();
			break;
	}
}


/**********************************************************************
タイマ割込み関連
***********************************************************************/
//タイマ初期化
void timer_init()
{
	//タイマ0
	TCCR0A=0x02;	//カウンタタイマ制御レジスタA  CTC動作
	TCCR0B=0x04;	//カウンタタイマ制御レジスタB  256分周	256/8MHz=32usec
	OCR0A=12;		//カウンタタイマ比較Aレジスタ  32usec*12 = 440usec
	TIMSK0=0x02;	//割り込み許可レジスタ OCIE0A
}


//タイマ割り込み
ISR(TIMER0_COMPA_vect)
{
	// LED制御 ----------------------------------
	BLANK_OFF
	
	for(int i=0;i<16;i++)
	{
		if(led_out[i]&matrix_a[dynamic])
		{
			SIN_ON
		}
		else
		{
			SIN_OFF
		}
		SCLK_PULSE
	}
	LAT_PULSE
	
	//ダイナミック点灯
	DYNAMIC_PORT=~(1<<dynamic);
	dynamic++;
	dynamic%=8;

	BLANK_ON


	// チャタリング防止のカウントダウン ----------------------------------
	if(sw_period>0)
	{
		sw_period--;
	}
	

	// 表示更新 ----------------------------------
	time_count++;
	if(!sw_flag && time_count>time_period)		//周期を減らす
	{
		switch(mode){
			case MOD_DESIGN:
				repeat_time++;
				if(repeat_time>message_time)
				{
					design_show();
					repeat_time=0;
				}
				break;
			
			case MOD_OCHI1:
			case MOD_OCHI2:
				if(gameplay[mode])
				{
					//下ボタンで落とす
					if(ochi_down_all)
					{
						ochi_down();
					}
				
					//タイマーで１つ下げる
					repeat_time++;
					if(repeat_time>ochi_time_period)
					{
						ochi_down();
						repeat_time=0;
					}
				}
				else
				{
					//点数データ表示
					repeat_time++;
					if(repeat_time>repeat_time_score)
					{
						show_score();
						repeat_time=0;
					}
				}
				break;
			
			case MOD_SNAKE1:
			case MOD_SNAKE2:
				if(gameplay[mode])
				{
					repeat_time++;
					if(repeat_time>snake_time_period)
					{
						snake_move();
						repeat_time=0;
					}
				}
				else
				{
					//点数データ表示
					repeat_time++;
					if(repeat_time>repeat_time_score)
					{
						show_score();
						repeat_time=0;
					}
				}
				break;
			
			case MOD_SHOOT1:
			case MOD_SHOOT2:
				if(gameplay[mode])
				{
					//弾の動き
					if(shoot_shot_on)
					{
						shoot_shot_move();
					}
				
					//敵の動き
					if(shoot_enemy_on)
					{
						repeat_time++;
						if(repeat_time>shoot_time_period)
						{
							shoot_enemy_move();
							repeat_time=0;
						}
					}

					//敵の弾の動き
					shoot_enemy_time++;
					if(shoot_enemy_time>shoot_enemy_time_period)
					{
						if(!shoot_enemy_shot_on)
						{
							shoot_enemy_shoot();
						}
						else
						{
							shoot_enemy_shot_move();
						}
						shoot_enemy_time=0;
					}
				}
				else
				{
					//点数データ表示
					repeat_time++;
					if(repeat_time>repeat_time_score)
					{
						show_score();
						repeat_time=0;
					}
				}
				break;
			
			case MOD_MESSAGE1:
			case MOD_MESSAGE2:
			
				//メッセージ更新
				repeat_time++;
				if(repeat_time>message_time)
				{
					message_show();
					repeat_time=0;
				}
				break;
		}
		
		time_count=0;
	}
}


/**********************************************************************
外部割込み・スリープ関連
***********************************************************************/
//外部割込み初期化
void irq_init()
{
	PCICR=1<<PCIE0; 		// ピン変化割り込み許可（PCINT0～5）
	PCMSK0|=1<<PCINT0|1<<PCINT1|1<<PCINT2|1<<PCINT3|1<<PCINT4|1<<PCINT5; 	// PCINT0-5で割込み
	PCIFR|=1<<PCIF0;		// ピン変化割り込み0要求フラグ（PCINT0～5）
}


//外部割込み
ISR(PCINT0_vect) 	// INT0割り込み
{
	//チャタリングチェック
	if(!sw_period)
	{
		//チャタリング防止
		sw_period=CHATTERING;
		sw_flag=1;
		
		//スイッチ判定
		if(SW_LEFT_ON) // 左SW
		{
			switch(mode){
				case MOD_DESIGN:
					message_speed(-1);
					break;
				case MOD_OCHI1:
				case MOD_OCHI2:
					ochi_left();
					break;
				case MOD_SNAKE1:
				case MOD_SNAKE2:
					snake_dir_change(-1,0);
					break;
				case MOD_SHOOT1:
				case MOD_SHOOT2:
					shoot_gun_move(-1);
					break;
				case MOD_MESSAGE1:
				case MOD_MESSAGE2:
					message_speed(-1);
					break;
			}
		}

		else if(SW_RIGHT_ON) // 右SW
		{
			switch(mode){
				case MOD_DESIGN:
					message_speed(1);
					break;
				case MOD_OCHI1:
				case MOD_OCHI2:
					ochi_right();
					break;
				case MOD_SNAKE1:
				case MOD_SNAKE2:
					snake_dir_change(1,0);
					break;
				case MOD_SHOOT1:
				case MOD_SHOOT2:
					shoot_gun_move(1);
					break;
				case MOD_MESSAGE1:
				case MOD_MESSAGE2:
					message_speed(1);
					break;
			}
		}
		
		else if(SW_DOWN_ON) // 下SW
		{
			switch(mode){
				case MOD_DESIGN:
					message_updown(-1);
					break;
				case MOD_OCHI1:
				case MOD_OCHI2:
					//下ボタンで点数を加算
					game_score[mode]+=TATE-ochi_koma_pos.y;
					ochi_down_all=1;
					break;
				case MOD_SNAKE1:
				case MOD_SNAKE2:
					snake_dir_change(0,1);
					break;
				case MOD_SHOOT1:
				case MOD_SHOOT2:
					if(!gameplay[mode])
					{
						shoot_init();
					}
					break;
				case MOD_MESSAGE1:
				case MOD_MESSAGE2:
					message_updown(-1);
					break;
			}
		}
		
		else if(SW_UP_ON) // 上SW
		{
			switch(mode){
				case MOD_DESIGN:
					message_updown(1);
					break;
				case MOD_OCHI1:
				case MOD_OCHI2:
					if(!gameplay[mode])
					{
						ochi_init();
					}
					break;
				case MOD_SNAKE1:
				case MOD_SNAKE2:
					snake_dir_change(0,-1);
					break;
				case MOD_SHOOT1:
				case MOD_SHOOT2:
					if(!gameplay[mode])
					{
						shoot_init();
					}
					break;
				case MOD_MESSAGE1:
				case MOD_MESSAGE2:
					message_updown(1);
					break;
			}
		}
		
		else if(SW_SELECT_ON) // 右手SW
		{
			switch(mode){
				case MOD_DESIGN:
					message_dir_change();
					break;
				case MOD_OCHI1:
				case MOD_OCHI2:
					ochi_rotate();
					break;
				case MOD_SNAKE1:
				case MOD_SNAKE2:
					if(!gameplay[mode])
					{
						snake_init();
					}
					break;
				case MOD_SHOOT1:
				case MOD_SHOOT2:
					shoot_shoot();
					break;
				case MOD_MESSAGE1:
				case MOD_MESSAGE2:
					message_dir_change();
					break;
			}
		}
		
		else if(SW_MODE_ON) // モードSW
		{
			mode++;
			mode%=MODES;
			eeprom_write_mode();
			
			mode_init();
		}
		
		sw_flag=0;
	}
}


/**********************************************************************
メイン関数
***********************************************************************/
int main(void)
{
	//各種初期化
	port_init();
	timer_init();
	irq_init();

	//割り込み許可
	sei();

	//EEPROM読み込み
	eeprom_read_all();
	
	//モード初期化
	mode_init();	

	//メインループ
	while(1)
	{
		//空　処理は割込みで行う
	}
}


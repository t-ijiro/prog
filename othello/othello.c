/***************************************************************************************************************/
//
//  FILE        : othello.c
//  DATE        : 2025/11/25 Tue.
//  DESCRIPTION : Main Program
//  CPU TYPE    : RX Family
//
//  Author T.Ijiro
//
//  ビルド
//  ・以下の割り込み関数をintprg.c内でコメントアウトする
//    1.Excep_CMT0_CMI0
//    2.Excep_CMT1_CMI1
//    3.Excep_CMT2_CMI2
//    4.Excep_CMT3_CMI3
//    5.Excep_ICU_IRQ0
//    6.Excep_ICU_IRQ1
//
//  ・stacksct.hのsuを0x1000に変更する
//
//  入力機能
//  ・ロータリーエンコーダー : カーソル移動
//  ・sw5                  : 2〜3秒長押しでリセット. 対戦モード選択画面で押すと AI vs AI エキシビション.
//  ・sw6                  : サウンドオンオフ
//  ・sw7                  : 決定
//  ・sw8                  : 移動方向オプション選択 ONで縦移動モード OFFで横移動モード
/************************************************************************************************/
// #include "typedefine.h"
#ifdef __cplusplus
// #include <ios>                        // Remove the comment when you use ios
//_SINT ios_base::Init::init_cnt;       // Remove the comment when you use ios
#endif

#include <string.h>
#include <stdlib.h>
#include <machine.h>
#include "iodefine.h"
#include "vect.h"
#include "lcd_lib4.h"
#include "onkai.h"

/************************************ マクロ *************************************************/
// ゲーム初期設定オプションマスク
#define OPT_ALL_OFF    0x00 // 全設定クリア
#define OPT_RES        0x01 // リセットフラグ
#define OPT_SND        0x02 // サウンドフラグ
#define OPT_MAN_VS_MAN 0x04 // 人間 対 人間 モードフラグ
#define OPT_MAN_VS_AI  0x08 // 人間 対 AI  モードフラグ
#define OPT_AI_VS_AI   0x10 // AI  対 AI  モードフラグ
#define OPT_AI_TURN    0x20 // AI先攻フラグ
#define OPT_SKIP       0x40 // スキップフラグ

#define OPT_NORMAL     (OPT_SND | OPT_MAN_VS_MAN)             // 通常時初期化オプション
#define OPT_EXHIBITION (OPT_SND | OPT_AI_VS_AI | OPT_AI_TURN) // AI vs AI 時初期化オプション

// 時間、周期
#define MONITOR_CHATTERING_PERIOD_MS 300  // チャタリング監視周期. IRQ用.
#define CURSOR_BLINK_PERIOD_MS       150  // カーソルの点滅周期
#define AI_MOVE_PERIOD_MS            300  // AIの移動周期
#define LINE_UP_RESULT_PERIOD_MS     200  // 結果表示でコマを並べる周期
#define SHOW_RESULT_WAIT_MS          3000 // 結果表示の時間

// ロータリーエンコーダー
#define PULSE_DIFF_PER_CLICK 4 // 1クリックの位相計数

// 74HC595シフトレジスタのシリアルデータ送信コマンド
#define SERIAL_SINK    do { PORT1.PODR.BIT.B5 = 0; } while(0)                        // 吸い込み
#define SERIAL_SOURCE  do { PORT1.PODR.BIT.B5 = 1; } while(0)                        // 吐き出し
#define SEND_LATCH_CLK do { PORT1.PODR.BIT.B6 = 1; PORT1.PODR.BIT.B6 = 0; } while(0) // ラッチ
#define LATCH_OUT      do { PORT1.PODR.BIT.B7 = 1; PORT1.PODR.BIT.B7 = 0; } while(0) // ラッチ出力

// マトリックスLED
#define COL_EN PORTE.PODR.BYTE  // 点灯列許可ビット選択

// 盤面
#define MAT_WIDTH  8 // 横のコマ数
#define MAT_HEIGHT 8 // 縦のコマ数

// リセットボタン オン
#define RESET_BTN_ON (PORTH.PIDR.BIT.B0 == 0)

// 移動オプション
#define MOVE_TYPE_UP_DOWN (PORTH.PIDR.BIT.B3 == 0) // 上下方向移動モード

// AIの先読みの回数
#define AI_DEPTH 3

// 評価関数の重み係数定義. どの要素をどれくらい重要視するか.
#define POS_WEIGHT      10  // 位置評価の重み係数
#define MOBILITY_WEIGHT 2   // 配置可能数評価の重み係数
#define STABLE_WEIGHT   50  // 確定石数（４つ角）評価の重み係数

// 無限大の代わりに使用する大きな値
#define INF 100000
/********************************************************************************************/


/********************************************* 定数 *************************************************/
// 置き判定の時の8方向の移動量
//                        　　　　上       下       左       右      左上      左下     右上     右下
static const int DXDY[8][2] = {{0, 1}, {0, -1}, {-1, 0}, {1, 0}, {-1, 1}, {-1, -1}, {1, 1}, {1, -1}};

// KEY = C majスケール
static const unsigned int C_SCALE[MAT_HEIGHT] = {DO1, RE1, MI1, FA1, SO1, RA1, SI1, DO2};

// 盤面のスコア定義
static const int POSITION_WEIGHTS[MAT_HEIGHT][MAT_WIDTH] =
{
    {120, -40,  20,  10,  10,  20, -40, 120},
    {-40, -50,  -5,  -5,  -5,  -5, -50, -40},
    { 20,  -5,  15,  10,  10,  15,  -5,  20},
    { 10,  -5,  10,   5,   5,  10,  -5,  10},
    { 10,  -5,  10,   5,   5,  10,  -5,  10},
    { 20,  -5,  15,  10,  10,  15,  -5,  20},
    {-40, -50,  -5,  -5,  -5,  -5, -50, -40},
    {120, -40,  20,  10,  10,  20, -40, 120}
};
/*******************************************************************************************/


/**************************************** 型定義 ********************************************/
// オセロの状態管理
enum State {
    // 初期化フェーズ
    INIT_HW,
    INIT_GAME,

	// 対戦モード選択フェーズ
	SELECT_VS,
	SELECT_WAIT,

    // ターン開始フェーズ
    TURN_START,
    TURN_CHECK,

    // AI思考フェーズ
    AI_THINK,

    // 入力フェーズ
    INPUT_WAIT,
    INPUT_READ,

    // AI移動フェーズ
    AI_MOVE,

    // 配置フェーズ
    PLACE_CHECK,
    PLACE_OK,
    PLACE_NG,

    // 反転フェーズ
    FLIP_CALC,
    FLIP_RUN,

    // ターン終了フェーズ
    TURN_SWITCH,
    TURN_COUNT,
    TURN_JUDGE,
    TURN_SHOW,

    // ゲーム終了フェーズ
    END_CALC,
    END_SHOW,
    END_WAIT,
    END_RESET,

	// 未定義状態
	STATE_UNDEFINED
};

// コマが動く方角
enum Direction{
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// コマの色
enum stone_color{
    stone_red,  // 赤コマ
    stone_green,// 緑コマ
    stone_black // 何も置かれていない
};

// ゲーム情報
struct Game{
	unsigned char is_reset         :1; // リセットフラグ
    unsigned char is_buzzer_active :1; // サウンドはオンかオフか？
	unsigned char is_man_vs_man    :1; // 人間 対 人間 モードか？
    unsigned char is_man_vs_AI     :1; // 人間 対 AI  モードか？
	unsigned char is_AI_vs_AI      :1; // AI  対 AI  モードか?
    unsigned char is_AI_turn       :1; // AIのターンか？
	unsigned char is_skip          :1; // スキップか？
};

// プレイヤー情報
struct Player{
	int placeable_count; // 配置可能数
	int result;          // 最終的なコマの保有数
};

// ロータリーエンコーダー
struct Rotary{
    unsigned short int current; // 現在のカウント数を保持
    unsigned short int prev;    // 過去のカウント数を保持
    short int delta;            // 現在と過去のカウント差
};

// カーソル
struct Cursor{
    int x;                  // x座標
    int y;                  // y座標
    int dest_x;             // 目的地のx座標
    int dest_y;             // 目的地のy座標
    enum stone_color color; // カーソルの色
};

// 手の情報を保持する. AI推論用
struct Move{
    int x;     // x座標
    int y;     // y座標
    int score; // 手のスコア
};
/****************************************************************************************/


/************************************* 割り込み使用グローバル変数 ********************************************/
static volatile unsigned long    tc_2ms;                        // 2msタイマーカウンター
static volatile unsigned long    tc_5ms;                        // 5msタイマーカウンター
static volatile unsigned long    tc_10ms;                       // 10msタイマーカウンター
static volatile unsigned long    tc_IRQ;                        // IRQ発生時のタイマカウンター
static volatile unsigned char    select_btn_on;                 // 決定ボタン押下 IRQ1発生フラグ(sw7)
static volatile unsigned int     beep_period_ms;                // ブザーを鳴らす時間(1ms基準)
static volatile unsigned int     count_to_reset;                // リセットボタン押下のカウント数
static volatile enum stone_color screen[MAT_HEIGHT][MAT_WIDTH]; // 割り込みで描画に使用
static volatile struct Game *    g_Game_inst;                   // グローバルアクセスGameインスタンス. ISRとbeep関数で使用.
static volatile struct Cursor    cursor;                        // グローバルアクセスCursorインスタンス
/************************************************************************************************************/


/************************************************** AI推論用グローバル変数 **************************************************/
// グローバル静的バッファ
static enum stone_color ai_buf[AI_DEPTH + 1][MAT_HEIGHT][MAT_WIDTH]; // 深さごとのシミュレーションバッファ
static int              ai_entry_idx[MAT_HEIGHT * MAT_WIDTH];        // ソートに対応させるための座標配列のインデックス
static int              ai_move_counts[AI_DEPTH];                    // 各深さでの候補手数
static struct Move      ai_moves[AI_DEPTH][MAT_HEIGHT * MAT_WIDTH];  // 各深さでの候補手リスト
/***************************************************************************************************************************/


/************************************************** 関数定義 **************************************************/
/********************************************** ハードウェア初期化 *********************************************/
// ポート初期化関数
void init_PORT(void)
{
    // PORTH0ピンを入力に設定
    PORTH.PDR.BIT.B0 = 0;

    // PORTH3ピンを入力に設定
    PORTH.PDR.BIT.B3 = 0;

    // PORT1の上位3ビット(bit5-7)を出力、下位5ビット(bit0-4)を入力に設定
    PORT1.PDR.BYTE = 0xE0;

    // PORTEの全ピンを出力に設定
    PORTE.PDR.BYTE = 0xFF;
}

// クロック初期化関数
void init_CLK(void)
{
    unsigned int i;

    // プロテクトレジスタ解除（クロック関連レジスタへの書き込みを許可）
    SYSTEM.PRCR.WORD = 0xA50F;

    // 電圧監視回路を無効化
    SYSTEM.VRCR = 0x00;

    // サブクロック発振器を停止
    SYSTEM.SOSCCR.BIT.SOSTP = 1;
    while (SYSTEM.SOSCCR.BIT.SOSTP != 1)
        ;  // 停止完了待ち

    // RTCを無効化
    RTC.RCR3.BYTE = 0x0C;
    while (RTC.RCR3.BIT.RTCEN != 0)
        ;  // 無効化完了待ち

    // メインクロック発振器の設定
    SYSTEM.MOFCR.BYTE = 0x0D;

    // メインクロック発振安定待ち時間の設定（約262ms）
    SYSTEM.MOSCWTCR.BYTE = 0x0D;

    // メインクロック発振器を起動
    SYSTEM.MOSCCR.BIT.MOSTP = 0x00;
    while (0x00 != SYSTEM.MOSCCR.BIT.MOSTP)
        ;  // 起動完了待ち

    // 発振安定化のための待機
    for (i = 0; i < 100; i++)
        nop();

    // PLL設定（入力周波数を10分周、出力を10倍 → 結果的に等倍）
    SYSTEM.PLLCR.WORD = 0x0901;

    // PLL発振安定待ち時間の設定（約1.05ms）
    SYSTEM.PLLWTCR.BYTE = 0x09;

    // PLLを起動
    SYSTEM.PLLCR2.BYTE = 0x00;

    // PLL安定化のための待機
    for (i = 0; i < 100; i++)
        nop();

    // 高速オンチップオシレータを停止
    SYSTEM.OPCCR.BYTE = 0x00;
    while (0 != SYSTEM.OPCCR.BIT.OPCMTSF)
        ;  // 停止完了待ち

    // システムクロック分周比設定
    // ICLK: PLL等倍, PCLKA: 1/2, PCLKB: 1/4, PCLKC: 1/4, PCLKD: 1/2, BCLK: 1/2, FCLK: 1/4
    SYSTEM.SCKCR.LONG = 0x21821211;
    while (0x21821211 != SYSTEM.SCKCR.LONG)
        ;  // 設定完了待ち

    // システムクロックソースをPLLに切り替え
    SYSTEM.SCKCR3.WORD = 0x0400;
    while (0x0400 != SYSTEM.SCKCR3.WORD)
        ;  // 切り替え完了待ち

    // プロテクトレジスタを再設定（書き込み禁止）
    SYSTEM.PRCR.WORD = 0xA500;
}

// コンペアマッチタイマ0初期化関数（約1msごとに割り込み）
void init_CMT0(void)
{
    // プロテクトレジスタ解除（モジュールストップ制御用）
    SYSTEM.PRCR.WORD = 0x0A502;

    // CMT0モジュールストップ解除（クロック供給開始）
    MSTP(CMT0) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0x0A500;

    // コンペアマッチ値設定（25000/8 - 1 = 3124、約1ms周期）
    CMT0.CMCOR = 25000 / 8 - 1;

    // コンペアマッチ割り込み有効、クロック分周比1/8
    CMT0.CMCR.WORD |= 0x00C0;

    // CMT0割り込み有効化
    IEN(CMT0, CMI0) = 1;

    // CMT0割り込み優先度設定（レベル1）
    IPR(CMT0, CMI0) = 1;

    // CMT0カウント開始
    CMT.CMSTR0.BIT.STR0 = 1;
}

// コンペアマッチタイマ1初期化関数（約2msごとに割り込み）
void init_CMT1(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0x0A502;

    // CMT1モジュールストップ解除
    MSTP(CMT1) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0x0A500;

    // コンペアマッチ値設定（(25000*2)/8 - 1 = 6249、約2ms周期）
    CMT1.CMCOR = (25000 * 2) / 8 - 1;

    // コンペアマッチ割り込み有効、クロック分周比1/8
    CMT1.CMCR.WORD |= 0x00C0;

    // CMT1割り込み有効化
    IEN(CMT1, CMI1) = 1;

    // CMT1割り込み優先度設定（レベル1）
    IPR(CMT1, CMI1) = 1;

    // CMT1カウント開始
    CMT.CMSTR0.BIT.STR1 = 1;
}

// コンペアマッチタイマ2初期化関数（約5msごとに割り込み）
void init_CMT2(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0x0A502;

    // CMT2モジュールストップ解除
    MSTP(CMT2) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0x0A500;

    // コンペアマッチ値設定（(25000*5)/8 - 1 = 15624、約5ms周期）
    CMT2.CMCOR = (25000 * 5) / 8 - 1;

    // コンペアマッチ割り込み有効、クロック分周比1/8
    CMT2.CMCR.WORD |= 0x00C0;

    // CMT2割り込み有効化
    IEN(CMT2, CMI2) = 1;

    // CMT2割り込み優先度設定（レベル1）
    IPR(CMT2, CMI2) = 1;

    // CMT2カウント開始
    CMT.CMSTR1.BIT.STR2 = 1;
}

// コンペアマッチタイマ3初期化関数（約10msごとに割り込み）
void init_CMT3(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0x0A502;

    // CMT3モジュールストップ解除
    MSTP(CMT3) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0x0A500;

    // コンペアマッチ値設定（(25000*10)/8 - 1 = 31249、約10ms周期）
    CMT3.CMCOR = (25000 * 10) / 8 - 1;

    // コンペアマッチ割り込み有効、クロック分周比1/8
    CMT3.CMCR.WORD |= 0x00C0;

    // CMT3割り込み有効化
    IEN(CMT3, CMI3) = 1;

    // CMT3割り込み優先度設定（レベル1）
    IPR(CMT3, CMI3) = 1;

    // CMT3カウント開始
    CMT.CMSTR1.BIT.STR3 = 1;
}

// 外部割り込み0初期化関数（PORTH1ピン）
void init_IRQ0(void)
{
    // IRQ0割り込み一時無効化
    IEN(ICU, IRQ0) = 0;

    // デジタルフィルタ一時無効化
    ICU.IRQFLTE0.BIT.FLTEN0 = 0;

    // デジタルフィルタのサンプリングクロック設定（PCLK/64）
    ICU.IRQFLTC0.BIT.FCLKSEL0 = 3;

    // PORTH1ピンを入力に設定
    PORTH.PDR.BIT.B1 = 0;

    // PORTH1ピンを周辺機能として使用
    PORTH.PMR.BIT.B1 = 1;

    // ピン機能選択レジスタの書き込み保護解除
    MPC.PWPR.BIT.B0WI = 0;
    MPC.PWPR.BIT.PFSWE = 1;

    // PORTH1をIRQ0入力ピンとして設定
    MPC.PH1PFS.BIT.ISEL = 1;

    // IRQ0を立ち下がりエッジで検出
    ICU.IRQCR[0].BIT.IRQMD = 1;

    // デジタルフィルタ有効化
    ICU.IRQFLTE0.BIT.FLTEN0 = 1;

    // 割り込みフラグクリア
    IR(ICU, IRQ0) = 0;

    // IRQ0割り込み有効化
    IEN(ICU, IRQ0) = 1;

    // IRQ0割り込み優先度設定（レベル1）
    IPR(ICU, IRQ0) = 1;
}

// 外部割り込み1初期化関数（PORTH2ピン）
void init_IRQ1(void)
{
    // IRQ1割り込み一時無効化
    IEN(ICU, IRQ1) = 0;

    // デジタルフィルタ一時無効化
    ICU.IRQFLTE0.BIT.FLTEN1 = 0;

    // デジタルフィルタのサンプリングクロック設定（PCLK/64）
    ICU.IRQFLTC0.BIT.FCLKSEL1 = 3;

    // PORTH2ピンを入力に設定
    PORTH.PDR.BIT.B2 = 0;

    // PORTH2ピンを周辺機能として使用
    PORTH.PMR.BIT.B2 = 1;

    // ピン機能選択レジスタの書き込み保護解除
    MPC.PWPR.BIT.B0WI = 0;
    MPC.PWPR.BIT.PFSWE = 1;

    // PORTH2をIRQ1入力ピンとして設定
    MPC.PH2PFS.BIT.ISEL = 1;

    // IRQ1を立ち下がりエッジで検出
    ICU.IRQCR[1].BIT.IRQMD  = 1;

    // デジタルフィルタ有効化
    ICU.IRQFLTE0.BIT.FLTEN1 = 1;

    // 割り込みフラグクリア
    IR(ICU, IRQ1) = 0;

    // IRQ1割り込み有効化
    IEN(ICU, IRQ1) = 1;

    // IRQ1割り込み優先度設定（レベル1）
    IPR(ICU, IRQ1) = 1;
}

// ブザー（PWM）初期化関数（MTU0使用）
void init_MTU0(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0x0A502;

    // MTU0モジュールストップ解除
    MSTP(MTU0) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0x0A500;

    // PORT3.4を出力に設定
    PORT3.PDR.BIT.B4 = 1;

    // PORT3.4を周辺機能として使用
    PORT3.PMR.BIT.B4 = 1;

    // ピン機能選択レジスタの書き込み保護解除
    MPC.PWPR.BIT.B0WI = 0;
    MPC.PWPR.BIT.PFSWE = 1;

    // PORT3.4をMTIOC0A（MTU0出力A）として設定
    MPC.P34PFS.BIT.PSEL = 1;

    // ピン機能選択レジスタの書き込み保護再設定
    MPC.PWPR.BIT.PFSWE = 0;

    // MTU0カウント停止
    MTU.TSTR.BIT.CST0 = 0x00;

    // タイマプリスケーラ設定（PCLK/4）
    MTU0.TCR.BIT.TPSC = 0x01;

    // TGRAのコンペアマッチでTCNTクリア
    MTU0.TCR.BIT.CCLR = 0x01;

    // PWMモード1に設定
    MTU0.TMDR.BIT.MD = 0x02;

    // MTIOC0A（チャネルA）を初期Low、コンペアマッチでHigh出力
    MTU0.TIORH.BIT.IOA = 0x06;

    // MTIOC0B（チャネルB）を初期High、コンペアマッチでLow出力
    MTU0.TIORH.BIT.IOB = 0x05;

    // カウンタ初期化
    MTU0.TCNT = 0;
}

// マルチファンクションタイマパルスユニット1初期化関数（エンコーダ入力用）
void init_MTU1(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0x0A502;

    // MTU1モジュールストップ解除
    MSTP(MTU1) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0X0A500;

    // PORT2.4とPORT2.5を周辺機能として使用
    PORT2.PMR.BIT.B4 = 1;
    PORT2.PMR.BIT.B5 = 1;

    // ピン機能選択レジスタの書き込み保護解除
    MPC.PWPR.BIT.B0WI = 0;
    MPC.PWPR.BIT.PFSWE = 1;

    // PORT2.4とPORT2.5をMTU1の入力ピンとして設定（位相計数モード用）
    MPC.P24PFS.BIT.PSEL = 2;
    MPC.P25PFS.BIT.PSEL = 2;

    // ピン機能選択レジスタの書き込み保護再設定
    MPC.PWPR.BIT.PFSWE = 0;

    // 位相計数モード1に設定（2相エンコーダ入力）
    MTU1.TMDR.BIT.MD = 4;

    // カウンタ初期化
    MTU1.TCNT = 0;

    // MTU1カウント開始
    MTU.TSTR.BIT.CST1 = 1;
}

// A/Dコンバータ0初期化関数
void init_AD0(void)
{
    // プロテクトレジスタ解除
    SYSTEM.PRCR.WORD = 0xA502;

    // 12ビットA/Dコンバータモジュールストップ解除
    MSTP(S12AD) = 0;

    // プロテクトレジスタを再設定
    SYSTEM.PRCR.WORD = 0xA500;

    // PORT4.0を周辺機能として使用
    PORT4.PMR.BIT.B0 = 1;

    // A/D変換終了割り込み無効
    S12AD.ADCSR.BIT.ADIE = 0;

    // AN000チャンネルをA/D変換対象に設定
    S12AD.ADANSA.BIT.ANSA0 = 1;

    // シングルスキャンモードに設定
    S12AD.ADCSR.BIT.ADCS = 0;

    // ピン機能選択レジスタの書き込み保護解除
    MPC.PWPR.BIT.B0WI = 0;
    MPC.PWPR.BIT.PFSWE = 1;

    // PORT4.0をアナログ入力として設定
    MPC.P40PFS.BIT.ASEL = 1;

    // ピン機能選択レジスタの書き込み保護再設定
    MPC.PWPR.BIT.PFSWE = 0;
}

// ハードウェア初期化
void init_RX210(void)
{
    init_CLK();    // 動作クロック設定
    init_LCD();    // LCD表示
    init_PORT();   // IO
    init_CMT0();   // ブザー時間管理
    init_CMT1();   // マトリックスled描画
    init_CMT2();   // スイッチ入力監視
    init_CMT3();   // 時間調整
    init_IRQ0();   // サウンドオンオフ
    init_IRQ1();   // 決定ボタン
    init_MTU0();   // ブザー
    init_MTU1();   // ロータリーエンコーダ
    init_AD0();    // 温度センサ
    setpsw_i();    // 割り込み許可
}
/***********************************************************************************/
/*********************************** ブザー ******************************************/
// ビープ音を鳴らす
void beep(unsigned int tone, unsigned int interval)
{
	// ブザーが無効の場合リターン
    if(！g_Game_inst->is_buzzer_active) return;
    
    if(tone)
    {
		//　矩形波生成
        MTU.TSTR.BIT.CST0 = 0;
        MTU0.TGRA = tone;
        MTU0.TGRB = tone / 2;
        MTU.TSTR.BIT.CST0 = 1;
    }
	// ブザー停止
    else
    {
        MTU.TSTR.BIT.CST0 = 0;
    }

	// CMT0でデクリメント. 0になったらCMT0内でブザー停止（MTU.TSTR.BIT.CST0 = 0;）
    beep_period_ms = interval;
}

/********************************* LCD表示 ******************************************/
// ターン表示
void lcd_show_whose_turn(enum stone_color sc)
{
    lcd_xy(1, 2);
    lcd_puts("                ");
    lcd_xy(1, 2);
    lcd_puts("TURN : ");
    lcd_puts((sc == stone_red) ? "RED" : "GREEN");
    flush_lcd();
}

// スキップメッセージ表示
void lcd_show_skip_msg(void)
{
    lcd_xy(1, 2);
    lcd_puts("                ");
    lcd_xy(1, 2);
    lcd_puts("SKIP PUSH SW7");
    flush_lcd();
}

// LCDをクリアして勝者発表メッセージ準備
void lcd_show_result_ready(void)
{
	lcd_clear();
	lcd_puts("Winner is ...");
	flush_lcd();
}

// 最終結果を元に勝者を表示
void lcd_show_winner(int red_stone_count, int green_stone_count)
{
    char *winner;

    lcd_xy(2, 2);

    if(red_stone_count > green_stone_count)
    {
        winner = "RED!";
    }
    else if(red_stone_count < green_stone_count)
    {
        winner = "GREEN!";
    }
    else
    {
        winner = "RED & GREEN!";
    }

    lcd_puts(winner);

    flush_lcd();
}

// ニューゲームに誘導
void lcd_show_confirm(void)
{
    lcd_clear();
    lcd_xy(5, 1);
    lcd_puts("othello");
    lcd_xy(1, 2);
    lcd_puts("NEW -> PUSH SW7");
    flush_lcd();
}

// 未定義状態を通知
void lcd_show_state_err(void)
{
	lcd_clear();
	lcd_puts("Undefine state");
	flush_lcd();
}
/*************************************************************************************/


/********************************** マトリックスLED ************************************/
// 指定した列の赤緑データをマトリックスLEDに出力
void col_out(int col, unsigned int rg_data)
{
	int i;

    for(i = 0; i < MAT_WIDTH * 2; i++)
	{
		if(rg_data & (1 << i))
		{
			SERIAL_SINK;    // 点灯(カソード側吸い込み)
		}
		else
		{
			SERIAL_SOURCE;  // 消灯(カソード側吐き出し)
		}

		SEND_LATCH_CLK;     // ラッチ
	}

	COL_EN = 0;             // 全消灯

	LATCH_OUT;              // ラッチ出力

	COL_EN = 1 << col;      // 点灯列指定
}
/******************************************************************************************/


/*********************************** ロータリーエンコーダ ***********************************/
// 位相計数用レジスタからカウント数を読み取る
unsigned short int read_rotary(void)
{
    return MTU1.TCNT;
}

// 位相計数の現在値と過去値の値の差を計算
// ノイズは丸め込む
short int get_rotary_delta(struct Rotary *r)
{
    return ( ((short int)(r->current - r->prev)) / PULSE_DIFF_PER_CLICK) * PULSE_DIFF_PER_CLICK;
}

// 左回りしたか
int is_rotary_turned_left(struct Rotary *r)
{           
    return (get_rotary_delta(r) >= PULSE_DIFF_PER_CLICK);
}

// 右回りしたか
int is_rotary_turned_right(struct Rotary *r)
{          
    return (get_rotary_delta(r) <= -PULSE_DIFF_PER_CLICK);
}
/**************************************************************************************/


/************************************** コマ/盤面 ********************************************* */
// 何も置かれてないか, または何色が置かれているか
enum stone_color read_stone_at(enum stone_color brd[][MAT_WIDTH], int x, int y)
{
   return brd[y][x];
}

// 指定した色のコマを置く
void place(enum stone_color brd[][MAT_WIDTH], int x, int y, enum stone_color sc)
{
    brd[y][x] = sc;
}

// 指定した座標のコマを消す
void delete(enum stone_color brd[][MAT_WIDTH], int x, int y)
{
    brd[y][x] = stone_black;
}

// ローカルボードの内容を割込み用表示ボードにコピー（フラッシュ）
void flush_board(enum stone_color brd[][MAT_WIDTH])
{
    memcpy(screen, brd, sizeof(screen));
}
/*****************************************************************************/


/****************************** カーソル **************************************/
// 上下左右を指定してカーソルの座標を更新
void move_cursor(enum Direction dir)
{
    int cx = cursor.x;
    int cy = cursor.y;

    switch (dir)
    {
        case LEFT:
            // 左シフト
            cx--;

            // 左端まで行ったら一つ上の行の右端にワープ
            if (cx < 0)
            {
                cx = MAT_WIDTH - 1;
                cy++;

                // 上端を超えたら一番下の行にワープ
                if (cy > MAT_HEIGHT - 1)
                {
                    cy = 0;
                }
            }

            break;

        case RIGHT:
            // 右シフト
            cx++;

            // 右端まで行ったら一つ下の行の左端にワープ
            if (cx > MAT_WIDTH - 1)
            {
                cx = 0;
                cy--;

                // 下端を超えたら一番上の行にワープ
                if (cy < 0)
                {
                    cy = MAT_HEIGHT - 1;
                }
            }

            break;

        case UP:
            // 上シフト
            cy++;

            // 上端まで行ったら一つの右の列の下端にワープ
            if (cy > MAT_HEIGHT - 1)
            {
                cx++;
                cy = 0;

                // 右端を超えたら一番左の列にワープ
                if (cx > MAT_WIDTH - 1)
                {
                    cx = 0;
                }
            }

            break;

        case DOWN:
            // 下シフト
            cy--;

            // 下端まで行ったら一つ左の行の上端にワープ
            if (cy < 0)
            {
                cy = MAT_HEIGHT - 1;
                cx--;

                // 左端を超えたら一番右の列にワープ
                if (cx < 0)
                {
                    cx = MAT_WIDTH - 1;
                }
            }

            break;

        default:
            break;
    }

    cursor.x = cx;
    cursor.y = cy;
}
/**********************************************************************************/


/************************************ ゲームロジック *********************************/
// タイミング調整
void wait_10ms(int period)
{
    tc_10ms = 0;
    while(tc_10ms < period)
        nop();
}

// AD変換値を取得. 乱数のシード値に利用.
unsigned int get_AD0_val(void)
{
    S12AD.ADCSR.BIT.ADST = 1;
    while (1 == S12AD.ADCSR.BIT.ADST)
        ;

    return (unsigned int)S12AD.ADDR0;
}

// 座標範囲外か
int is_out_of_board(int x, int y)
{
    return ((x < 0) || (y < 0) || ( x > MAT_WIDTH  - 1) || (y > MAT_HEIGHT - 1));
}

// 8方向のひっくり返しフラグを作る
//　       右下  右上  左下  左上  右   左   下   上
// flag :  b7    b6    b5    b4  b3   b2   b1   b0
// bit  :  0..その方角にひっくり返せない, 1..その方角にひっくり返せる
unsigned char make_flip_dir_flag(enum stone_color brd[][MAT_WIDTH], int x, int y, enum stone_color sc)
{
    int dir, i;
    int dx, dy;
    unsigned char flag = 0x00;

    enum stone_color search;

    for(dir = 0; dir < 8; dir++)
    {
        dx = dy = 0;

        for(i = 0; i < 8; i++)
        {
            dx += DXDY[dir][0];
            dy += DXDY[dir][1];

            // 範囲外ならbreak
            if(is_out_of_board(x + dx, y + dy)) break;

            // コマの色を調査
            search = read_stone_at(brd, x + dx, y + dy);

            // 何も置かれていなかったらbreak
            if(search == stone_black) break;

            // 自色のコマに遭遇
            if(search == sc)
            {
                // i > 0 の時点で相手色を少なくとも1つは挟んでいる
                if(i > 0)
                {
                    flag |= (1 << dir);
                }

                break;
            }
        }
    }

    return flag;
}

// その場所にその色は置けるか？
int is_placeable(enum stone_color brd[][MAT_WIDTH], int x, int y, enum stone_color sc)
{
    unsigned char flag;

    // 何かおいてあったらだめ
    if(read_stone_at(brd, x, y) != stone_black) return 0;

     // 8方向フラグ作成
    flag = make_flip_dir_flag(brd, x, y, sc);

    // flag != 0x00なら少なくとも1方向は挟める
    return (flag != 0x00);
}

// 8方向フラグをつかって相手のコマをひっくり返す
void flip_stones(unsigned char flag, enum stone_color brd[][MAT_WIDTH], int x, int y, enum stone_color sc)
{
    int dir, i;
    int dx, dy;
    enum stone_color search;

    for(dir = 0; dir < 8; dir++)
    {
        dx = dy = 0;

        if(flag & (1 << dir))
        {
            for(i = 0; i < 8; i++)
            {
                dx += DXDY[dir][0];
                dy += DXDY[dir][1];

                // コマの色をチェック
                search = read_stone_at(brd, x + dx, y + dy);

                // 置きチェック済みなので確認するのは自分の色が出たかのみ
                if(search == sc)
                {
                    break;
                }

                // 新しくコマを置く
                place(brd, x + dx, y + dy, (search == stone_red) ? stone_green : stone_red);
            }
        }
    }
}

// ボード上の配置可能数を数える
int count_placeable(enum stone_color brd[][MAT_WIDTH], enum stone_color sc)
{
    int x, y;
    int count = 0;

    for(x = 0; x < MAT_WIDTH; x++)
    {
        for(y = 0; y < MAT_HEIGHT; y++)
        {
            if(is_placeable(brd, x, y, sc))
            {
                count++;
            }
        }
    }

    return count;
}

// 指定した色のコマの数を数える
int count_stones(enum stone_color brd[][MAT_WIDTH], enum stone_color sc)
{
    int x, y;
    int count = 0;

    for(x = 0; x < MAT_WIDTH; x++)
    {
        for(y = 0; y < MAT_HEIGHT; y++)
        {
            if(read_stone_at(brd, x, y) == sc)
            {
                count++;
            }
        }
    }

    return count;
}

// どっちも置けなかったらおわり
int is_game_over(int stone1_placeable_count, int stone2_placeable_count)
{
    return (!stone1_placeable_count && !stone2_placeable_count);
}

// コマを並べて結果発表
void line_up_result(enum stone_color brd[][MAT_WIDTH], int stone1_count, int stone2_count, int period_10ms)
{
	int x, y;

    // コマを全撤去
	for(x = 0; x < MAT_WIDTH; x++)
	{
		for(y = 0; y < MAT_HEIGHT; y++)
        {
            delete(brd, x, y);
        }
	}

    flush_board(brd);

	x = 0;

    // 最終結果をもとに再配置
	while(stone1_count || stone2_count)
	{
		if(stone1_count)
		{
            // 片方の色を左上から詰めていく
            place(brd, x % MAT_WIDTH, ((MAT_WIDTH - 1) - (x / MAT_WIDTH)), stone_red);

			stone1_count--;
		}
		else
		{   // 詰め終わったら続きからもう片方の色を詰めていく
			place(brd, x % MAT_WIDTH, ((MAT_WIDTH - 1) - (x / MAT_WIDTH)), stone_green);

		    stone2_count--;
		}

        flush_board(brd);

        // x座標に合わせてドレミ
		beep(C_SCALE[x % MAT_WIDTH], 50);

        // 詰めの感覚を調整
		wait_10ms(period_10ms);

		x++;
	}
}

/********************************************* AI ***********************************************/
// 盤面の位置評価を計算
int evaluate_position_weight(enum stone_color brd[][MAT_WIDTH], enum stone_color ai_color)
{
    int x, y;
    int ai_score = 0;
    int opp_score = 0;
    enum stone_color opp_color = (ai_color == stone_red) ? stone_green : stone_red;

    for(y = 0; y < MAT_HEIGHT; y++)
    {
        for(x = 0; x < MAT_WIDTH; x++)
        {
            if(read_stone_at(brd, x, y) == ai_color)
            {
                ai_score += POSITION_WEIGHTS[y][x];
            }
            else if(read_stone_at(brd, x, y) == opp_color)
            {
                opp_score += POSITION_WEIGHTS[y][x];
            }
        }
    }

    return ai_score - opp_score;
}

// コマの数の差を計算. 終盤用.
int evaluate_stone_count(enum stone_color brd[][MAT_WIDTH], enum stone_color ai_color)
{
    int x, y;
    int ai_count = 0;
    int opp_count = 0;
    enum stone_color opp_color = (ai_color == stone_red) ? stone_green : stone_red;

    for(y = 0; y < MAT_HEIGHT; y++)
    {
        for(x = 0; x < MAT_WIDTH; x++)
        {
            if(read_stone_at(brd, x, y) == ai_color)
            {
                ai_count++;
            }
            else if(read_stone_at(brd, x, y) == opp_color)
            {
                opp_count++;
            }
        }
    }

    return ai_count - opp_count;
}

// 絶対に取られないコマの数を計算
int count_stable_stones(enum stone_color brd[][MAT_WIDTH], enum stone_color sc)
{
    int stable_count = 0;

    // 角のコマは確定石
    if(read_stone_at(brd, 0,           0             ) == sc) stable_count++;
    if(read_stone_at(brd, MAT_WIDTH-1, 0             ) == sc) stable_count++;
    if(read_stone_at(brd, 0,           MAT_HEIGHT - 1) == sc) stable_count++;
    if(read_stone_at(brd, MAT_WIDTH-1, MAT_HEIGHT - 1) == sc) stable_count++;

    return stable_count;
}

// 盤面評価関数
// AI視点でのスコアを計算
int evaluate_board(enum stone_color brd[][MAT_WIDTH], enum stone_color ai_color)
{
    enum stone_color opp_color = (ai_color == stone_red) ? stone_green : stone_red;
    int position_score, mobility_score, stable_score;
    int ai_stable, opp_stable;
    int ai_mobility, opp_mobility;

    // 位置評価 各マスの価値
    position_score = evaluate_position_weight(brd, ai_color);

    // 配置可能数評価
    // 自分の手数が多く、相手の手数が少ないほど有利
    ai_mobility = count_placeable(brd, ai_color);
    opp_mobility = count_placeable(brd, opp_color);
    mobility_score = ai_mobility - opp_mobility;

    // 確定石評価
    // 角に配置されたコマは絶対に取られない
    ai_stable = count_stable_stones(brd, ai_color);
    opp_stable = count_stable_stones(brd, opp_color);
    stable_score = (ai_stable - opp_stable) * STABLE_WEIGHT;

    // 各要素に重み係数を掛けて総合スコアを算出
    return position_score * POS_WEIGHT + mobility_score * MOBILITY_WEIGHT + stable_score;
}

// ミニマックス法 + αβ枝刈り
// AIが最善の手を見つけるため、相手も最善手を打つと仮定して先読みする
int minimax_alphabeta(enum stone_color brd[][MAT_WIDTH], enum stone_color ai_color, int max_depth)
{
    int depth, x, y, i, move_idx;
    enum stone_color current_color;
    int score, best_score;
    int is_max_player;

    // スタック用の変数
    int stack_alpha[AI_DEPTH + 1];      // α値：MAXプレイヤーの最小値
    int stack_beta[AI_DEPTH + 1];       // β値：MINプレイヤーの最大値
    int stack_best_score[AI_DEPTH + 1]; // 各深さでの最良スコア
    int stack_move_idx[AI_DEPTH + 1];   // 現在評価中の手のインデックス
    int stack_is_max[AI_DEPTH + 1];     // MAXプレイヤーかどうかのフラグ

    // 初期化
	// 現在の盤面をシミュレーション用バッファにコピー
    memcpy(ai_buf[0], brd, sizeof(enum stone_color) * MAT_HEIGHT * MAT_WIDTH);

    // ルートノード（深さ0）の候補手を生成
    ai_move_counts[0] = 0;
    for(y = 0; y < MAT_HEIGHT; y++)
    {
        for(x = 0; x < MAT_WIDTH; x++)
        {
            // 配置可能な場所を全てリストアップ
            if(is_placeable(ai_buf[0], x, y, ai_color))
            {
                ai_moves[0][ai_move_counts[0]].x = x;
                ai_moves[0][ai_move_counts[0]].y = y;
                ai_moves[0][ai_move_counts[0]].score = -INF;
                ai_move_counts[0]++;
            }
        }
    }

    // 配置可能な場所がない場合
    if(ai_move_counts[0] == 0) return -INF;

    best_score = -INF;

    // ルートノードの各候補手を順番に評価
    for(i = 0; i < ai_move_counts[0]; i++)
    {
        x = ai_moves[0][i].x;
        y = ai_moves[0][i].y;

        // 手を打つ盤面をコピーしてコマを配置・反転
        memcpy(ai_buf[1], ai_buf[0], sizeof(enum stone_color) * MAT_HEIGHT * MAT_WIDTH);
        flip_stones(make_flip_dir_flag(ai_buf[1], x, y, ai_color), ai_buf[1], x, y, ai_color);

        // 深さ1から探索開始（相手のターン）
        depth = 1;
        stack_alpha[1] = -INF;      // α値初期化
        stack_beta[1] = INF;        // β値初期化
        stack_move_idx[1] = 0;      // 最初の手から評価
        stack_is_max[1] = 0;        // 次は相手のターン（MINプレイヤー）
        score = -INF;

        // 深さ優先探索をループで実装
        while(depth > 0)
        {
            // 葉ノード到達
            // 指定した深さまで探索完了
            if(depth >= max_depth)
            {
                // 評価値を計算
                score = evaluate_board(ai_buf[depth], ai_color);
                depth--;  // 一つ上の階層に戻る

                // 親ノードに評価値を伝播
                if(depth > 0)
                {
                    if(stack_is_max[depth])  // MAXプレイヤー（AI）
                    {
                        // より良いスコアを選択
                        if(score > stack_best_score[depth])
						{
							stack_best_score[depth] = score;
						}

                        // β枝刈り
						// MINプレイヤーがこのルートを選ばないことが確定
                        if(stack_best_score[depth] >= stack_beta[depth])
                        {
                            score = stack_best_score[depth];
                            depth--;
                            if(depth > 0)
                            {
                                stack_move_idx[depth]++;  // 次の手へ
                            }
                            continue;
                        }

                        // α値更新
                        if(stack_best_score[depth] > stack_alpha[depth])
						{
							 stack_alpha[depth] = stack_best_score[depth];
						}
                           
                    }
                    else  // MINプレイヤー（相手）
                    {
                        // より悪いスコアを選択
                        if(score < stack_best_score[depth])
						{
							stack_best_score[depth] = score;
						}
                            
                        // α枝刈り
						// MAXプレイヤーがこのルートを選ばないことが確定
                        if(stack_best_score[depth] <= stack_alpha[depth])
                        {
                            score = stack_best_score[depth];
                            depth--;
                            if(depth > 0)
                            {
                                stack_move_idx[depth]++;  // 次の手へ
                            }
                            continue;
                        }

                        // β値更新
                        if(stack_best_score[depth] < stack_beta[depth])
						{
							stack_beta[depth] = stack_best_score[depth];
						}
                            
                    }
                    stack_move_idx[depth]++;  // 次の手へ
                }
                continue;
            }

            // 中間ノード
			// 現在のプレイヤーを判定
            is_max_player = stack_is_max[depth];
            // 奇数深さ=相手、偶数深さ=AI
            current_color = (depth % 2 == 1) ? (ai_color == stone_red ? stone_green : stone_red) : ai_color;

            // 初回訪問時
			// このノードの候補手を生成
            if(stack_move_idx[depth] == 0)
            {
                ai_move_counts[depth] = 0;
                for(y = 0; y < MAT_HEIGHT; y++)
                {
                    for(x = 0; x < MAT_WIDTH; x++)
                    {
                        // 配置可能な場所をリストアップ
                        if(is_placeable(ai_buf[depth], x, y, current_color))
                        {
                            ai_moves[depth][ai_move_counts[depth]].x = x;
                            ai_moves[depth][ai_move_counts[depth]].y = y;
                            ai_move_counts[depth]++;
                        }
                    }
                }

                // 手がない場合（パス）
                if(ai_move_counts[depth] == 0)
                {
                    // パスの場合は現在の盤面を評価して返す
                    score = evaluate_board(ai_buf[depth], ai_color);
                    depth--;  // 親ノードに戻る

                    // スコアを親ノードに反映
                    if(depth > 0)
                    {
                        if(stack_is_max[depth])
                        {
                            if(score > stack_best_score[depth])
							{
								stack_best_score[depth] = score;
							}
                        }
                        else
                        {
                            if(score < stack_best_score[depth])
							{
								stack_best_score[depth] = score;
							}
                        }

                        stack_move_idx[depth]++;  // 次の手へ
                    }
                    continue;
                }

                // 最良スコア初期化（MAXは-∞、MINは+∞から開始）
                stack_best_score[depth] = is_max_player ? -INF : INF;
            }

            // すべての候補手を評価済みの場合
            if(stack_move_idx[depth] >= ai_move_counts[depth])
            {
                score = stack_best_score[depth];
                depth--;  // 親ノードに戻る

                // スコアを親ノードに伝播 + αβ枝刈りチェック
                if(depth > 0)
                {
                    if(stack_is_max[depth])  // MAXプレイヤー
                    {
                        if(score > stack_best_score[depth])
						{
							stack_best_score[depth] = score;
						}
                            
                        // β枝刈り
                        if(stack_best_score[depth] >= stack_beta[depth])
                        {
                            score = stack_best_score[depth];
                            depth--;
                            if(depth > 0)
                            {
                                stack_move_idx[depth]++;
                            }
                            continue;
                        }

                        // α値更新
                        if(stack_best_score[depth] > stack_alpha[depth])
						{
							stack_alpha[depth] = stack_best_score[depth];
						}
                    }
                    else  // MINプレイヤー
                    {
                        if(score < stack_best_score[depth])
						{
							stack_best_score[depth] = score;
						}
                            
                        // α枝刈り
                        if(stack_best_score[depth] <= stack_alpha[depth])
                        {
                            score = stack_best_score[depth];
                            depth--;
                            if(depth > 0)
                            {
                                stack_move_idx[depth]++;
                            }
                            continue;
                        }

                        // β値更新
                        if(stack_best_score[depth] < stack_beta[depth])
						{
							stack_beta[depth] = stack_best_score[depth];
						}
                    }

                    stack_move_idx[depth]++;  // 次の手へ
                }
                continue;
            }

            // 次の手を試す
            move_idx = stack_move_idx[depth];
            x = ai_moves[depth][move_idx].x;
            y = ai_moves[depth][move_idx].y;

            // 手を打つ
			// 盤面をコピーしてコマを配置・反転
            memcpy(ai_buf[depth + 1], ai_buf[depth], sizeof(enum stone_color) * MAT_HEIGHT * MAT_WIDTH);
            flip_stones(make_flip_dir_flag(ai_buf[depth + 1], x, y, current_color), ai_buf[depth + 1], x, y, current_color);

            // 次の深さへ進む（子ノードへ）
            depth++;
            stack_alpha[depth] = stack_alpha[depth - 1];  // α値を引き継ぐ
            stack_beta[depth] = stack_beta[depth - 1];    // β値を引き継ぐ
            stack_move_idx[depth] = 0;                    // 最初の手から評価
            stack_is_max[depth] = !is_max_player;         // プレイヤー切り替え
        }

        // ルートノードの各手のスコアを記録
        ai_moves[0][i].score = score;
        if(score > best_score)
        {
            best_score = score;
        }
    }

    return best_score;
}

// AIの次の行き先を決定する関数
void set_AI_cursor_dest(enum stone_color brd[][MAT_WIDTH], enum stone_color sc, int placeable_count, int depth)
{
    int i, best_idx, best_count;
    int best_score;

    // スキップ判定
	// どこにも置けない場合は現在のカーソル位置を維持
    if(!placeable_count)
    {
        cursor.dest_x = cursor.x;
        cursor.dest_y = cursor.y;
        return;
    }

    // ミニマックス + αβ枝刈りで全候補手を評価
    minimax_alphabeta(brd, sc, depth);

    // 最高評価のスコアを見つける
    best_score = -INF;

    for(i = 0; i < ai_move_counts[0]; i++)
    {
        if(ai_moves[0][i].score > best_score)
        {
            best_score = ai_moves[0][i].score;
        }
    }

    // 同じスコアの手が複数ある場合をカウント
    best_count = 0;

    for(i = 0; i < ai_move_counts[0]; i++)
    {
        if(ai_moves[0][i].score == best_score)
        {
            ai_entry_idx[best_count] = i;  // 同点の手のインデックスを記録
            best_count++;
        }
    }

    // 同点の場合はランダムに選択
    if(best_count > 1)
    {
        best_idx = ai_entry_idx[rand() % best_count];
    }
    else
    {
        best_idx = ai_entry_idx[0];
    }

    // カーソルの目標位置を設定
    cursor.dest_x = ai_moves[0][best_idx].x;
    cursor.dest_y = ai_moves[0][best_idx].y;
}
/*************************************************************************************************/


/***************************************** 初期設定 ******************************************/
// 位相計数用レジスタのカウント数初期化
void clear_pulse_diff_cnt(void)
{
    MTU1.TCNT = 0;
}

// ロータリーエンコーダー情報初期化
void init_Rotary(struct Rotary *r)
{
    r->current = 0;
    r->prev    = 0;
    r->delta   = 0;
}

// ゲーム情報初期化
void init_Game(struct Game *g, unsigned char option)
{
    g->is_reset         = !!(option & OPT_RES);
    g->is_buzzer_active = !!(option & OPT_SND);
    g->is_man_vs_man    = !!(option & OPT_MAN_VS_MAN);
	g->is_man_vs_AI     = !!(option & OPT_MAN_VS_AI);
    g->is_AI_vs_AI      = !!(option & OPT_AI_VS_AI);
	g->is_AI_turn       = !!(option & OPT_AI_TURN);
	g->is_skip          = !!(option & OPT_SKIP);
}

// プレイヤー情報初期化
void init_Player(struct Player *p1, struct Player *p2)
{
    // オセロのルール上最初は二か所しか置けない
	p1->placeable_count = 2;
	p2->placeable_count = 2;
    p1->result          = 0;
	p2->result          = 0;
}

// 盤面初期化
void init_board(enum stone_color brd[][MAT_WIDTH])
{   
	int x, y;

    // コマ全撤去
    for(x = 0;x < MAT_WIDTH; x++)
    {
        for(y = 0; y < MAT_HEIGHT; y++)
        {
            delete(brd, x, y);
        }
    }

    // 真ん中に４つ置く
    place(brd, 3, 3, stone_red);
    place(brd, 4, 4, stone_red);
    place(brd, 3, 4, stone_green);
    place(brd, 4, 3, stone_green);
}

// カーソル初期化
void init_Cursor(void)
{
    cursor.color = stone_red;
    cursor.x     = 5;
    cursor.y     = 3;
}

// LCD表示初期化
void init_lcd_show(enum stone_color sc)
{
	lcd_clear();
	lcd_xy(5, 1);
	lcd_puts("othello");
	lcd_xy(1, 2);
	lcd_puts("VS >FRIEND : AI");
	flush_lcd();
}
/*************************************************************************************************/


/****************************************** 割込み ************************************************/
// CMT0 CMI0 1msタイマ割込みハンドラ
// ブザー制御
void Excep_CMT0_CMI0(void)
{
    // ブザー鳴動期間をデクリメント
    beep_period_ms--;

    // 指定時間が経過したら、またはブザー無効フラグが立ったら音を止める
    if(!beep_period_ms)
    {
        // MTU0のカウント動作を停止（PWM出力停止 = ブザー消音）
        MTU.TSTR.BIT.CST0 = 0;
    }
}

// CMT1 CMI1 2msタイマ割込みハンドラ
// 8×8 マトリクスledのダイナミック点灯制御
void Excep_CMT1_CMI1(void)
{
    int x, y;
    unsigned int rg_data = 0x0000;  // 赤(上位8ビット)・緑(下位8ビット)のLEDデータ

    // 2msタイムカウンタをインクリメント
    tc_2ms++;

    // 現在表示する列を決定
    x = tc_2ms % MAT_WIDTH;

    // 指定列xの全行yをスキャンして表示データを作成
    for(y = 0; y < MAT_HEIGHT; y++)
    {
        if(screen[y][x] == stone_red)
        {
            // 赤コマの場合上位8ビット（bit8〜15）に対応するビットをセット
            rg_data |= (1 << (y + 8));
        }
        else if(screen[y][x] == stone_green)
        {
            // 緑コマの場合下位8ビット（bit0〜7）に対応するビットをセット
            rg_data |= (1 << y);
        }
    }

    // カーソル位置でないか、カーソルが黒色（非表示）の場合
    if((x != cursor.x) || (cursor.color == stone_black))
    {
        // 通常の表示データをそのまま出力
        col_out(x, rg_data);
        return;
    }

    // カーソル位置の場合：点滅制御
    // 点滅周期の半分ごとに表示/非表示を切り替え
    if((tc_2ms / (CURSOR_BLINK_PERIOD_MS / 2)) % 2)
    {
        // 点灯期間：カーソルのLEDをON
        rg_data |= (cursor.color == stone_red) ? (1 << (cursor.y + 8)) : (1 << cursor.y);
    }
    else if(rg_data & ((1 << (cursor.y + 8)) | (1 << cursor.y)))
    {
        // 消灯期間：カーソル位置に既にコマがある場合はそれも消す
        // カーソル位置のビットをクリア
        rg_data &= ~((1 << (cursor.y + 8)) | (1 << cursor.y));
    }

    // マトリックスLEDに出力（指定列を点灯）
    col_out(x, rg_data);
}

// CMT2 CMI2 5msタイマ割込みハンドラ
// 入力監視制御
void Excep_CMT2_CMI2(void)
{
    // 5msタイムカウンタをインクリメント
    // IRQ内でチャタリング除去に使用
    tc_5ms++;

    // 1秒間隔でリセットボタン入力を監視
    if(tc_5ms % (1000 / 5) == 0)
    {
        if(RESET_BTN_ON)
        {
            beep(DO1, 50);
            count_to_reset++;
        }
        else
        {
            count_to_reset = 0;
        }

        // 2～3秒長押しされたらリセット
        if(count_to_reset > 2)
        {
            beep(DO2, 300);
            count_to_reset = 0;
            g_Game_inst->is_reset = 1;
        }
    }
}

// CMT3 CMI3 10msタイマ割込みハンドラ
// 時間調整
void Excep_CMT3_CMI3(void)
{
    // 10msタイムカウンタをインクリメント
    // wait_10ms()関数などで使用
    tc_10ms++;
}

// ICU IRQ0 SW6立下がり割込みハンドラ
// ブザーON/OFF
void Excep_ICU_IRQ0(void)
{
    unsigned long now = tc_5ms;  // 現在の時刻を取得

    // チャタリング対策
	// 前回のIRQ発生から指定時間経っていない場合は無視
    if(now - tc_IRQ < MONITOR_CHATTERING_PERIOD_MS / 5) return;

    // ブザー有効フラグをトグル
    g_Game_inst->is_buzzer_active ^= 1;

    // 最後のIRQ発生時刻を記録（次回のチャタリング判定用）
    tc_IRQ = now;
}

// ICU IRQ1 SW7立下がり割込みハンドラ
// 決定ボタン
void Excep_ICU_IRQ1(void)
{
    unsigned long now = tc_5ms;  // 現在の時刻を取得

    // チャタリング対策
	// 前回のIRQ発生から指定時間経っていない場合は無視
    if(now - tc_IRQ < MONITOR_CHATTERING_PERIOD_MS / 5) return;

    // 決定ボタン押下フラグをセット
    select_btn_on = 1;

    // 最後のIRQ発生時刻を記録（次回のチャタリング判定用）
    tc_IRQ = now;
}
/**************************************************************************************************/
/******************************************* 関数定義終 ********************************************/


/******************************************** メイン ***********************************************/
void main(void)
{
    // 状態管理
    enum State state = INIT_HW;

    // ボード色情報
    enum stone_color board[MAT_HEIGHT][MAT_WIDTH];

	// ゲーム情報
    struct Game game;

    // 赤緑プレイヤー
    struct Player red, green;

    // ロータリーエンコーダ入力
    struct Rotary rotary;

    // コマ反転用フラグ
	//　       右下  右上   左下   左上  右   左   下   上
	// flag :  b7    b6    b5    b4    b3   b2   b1   b0
	// bit  :  0..その方角にひっくり返せない, 1..その方角にひっくり返せる
    unsigned char flip_dir_flag;

    // 初期化オプション
    unsigned char init_option = OPT_NORMAL;

    // グローバルアクセスGameインスタンス
    // ISR と beep関数で使用
    g_Game_inst = &game;

    // ハードウェア初期化
    init_RX210();

    while(1)
    {
        // フラグが立ったら初期化フェーズへ
        if(game.is_reset)
        {
            // 対戦モード選択中フェーズだったら初期化オプションをAI vs AI エキシビションに設定
            // それ以外の場合は通常初期化オプション
            init_option = (state == SELECT_WAIT || state == SELECT_VS) ? OPT_EXHIBITION : OPT_NORMAL;
          
            state = INIT_HW;
            
            game.is_reset = 0;
        }
        
        switch(state)
		{
		    //********** 初期化フェーズ **********//

		    // ハードウェア初期化状態
		    case INIT_HW:
		        // ロータリーエンコーダのパルス差分カウンタをクリア
		        clear_pulse_diff_cnt();

		        // ロータリーエンコーダ構造体を初期化
		        init_Rotary(&rotary);

		        // ゲーム初期化状態へ遷移
		        state = INIT_GAME;
		        break;

		    // ゲーム初期化状態
		    case INIT_GAME:
		        // A/D変換値を使って乱数シードを初期化
		        srand(get_AD0_val());

		        // ゲーム構造体を初期化
		        init_Game(&game, init_option);

		        // 赤・緑プレイヤーの情報を初期化
		        init_Player(&red, &green);

		        // 盤面を初期状態に設定
		        init_board(board);

		        // カーソルを初期位置に配置
		        init_Cursor();

		        // LCD初期表示（現在のカーソル色を表示）
		        init_lcd_show(cursor.color);

		        // 盤面をLEDマトリクスに出力
		        flush_board(board);

		        // 通常時:対戦モード選択待ち状態へ遷移
                // AI vs AI時:ターン開始状態へ遷移
		        state = (init_option == OPT_NORMAL) ? SELECT_WAIT : TURN_START;

		        break;

		    //********** 対戦モード選択フェーズ **********//

		    // 対戦モード選択待ち状態
		    case SELECT_WAIT:
		        // IRQ1割り込みフラグ（決定ボタン）がセットされているか確認
		        if(select_btn_on)
		        {
		            // 決定音を鳴らす
		            beep(DO2, 200);

		            // 現在のターン表示
		            lcd_show_whose_turn(cursor.color);

		            // ゲーム開始：ターン開始状態へ遷移
		            state = TURN_START;

		            // 割り込みフラグをクリア
		            select_btn_on = 0;
		        }
		        else
		        {
		            // まだ決定されていない場合は対戦モード選択状態へ
		            state = SELECT_VS;
		        }
		        break;

		    // 対戦モード選択状態
		    case SELECT_VS:
		        // ロータリーエンコーダの現在値を読み取り（クリック単位に変換）
		        rotary.current = read_rotary();

		        // 左右どちらかにクリックされた場合
		        if(is_rotary_turned_left(&rotary) || is_rotary_turned_right(&rotary))
		        {
		            // 選択変更音を鳴らす
		            beep(DO3, 50);

		            // AI対戦モードをトグル
					game.is_man_vs_man ^= 1;
		            game.is_man_vs_AI  ^= 1;

					if(game.is_man_vs_man)
		            {
		                // 友達対戦モード選択時
		                lcd_xy(1, 2);
		                lcd_puts("VS >FRIEND : AI");
		                flush_lcd();
		            }

		            // LCD表示を更新
		            if(game.is_man_vs_AI)
		            {
		                // AI対戦モード選択時
		                lcd_xy(1, 2);
		                lcd_puts("VS  FRIEND :>AI");
		                flush_lcd();
		            }
						
                    // 次回の比較用
		            rotary.prev += get_rotary_delta(&rotary);
		        }

		        // 選択待ち状態へ戻る
		        state = SELECT_WAIT;
		        break;

		    //********** ターン開始フェーズ **********//

		    // ターン開始状態
		    case TURN_START:
		        // ターンチェック状態へ遷移
		        state = TURN_CHECK;
		        break;

		    // ターンチェック状態（プレイヤーがAIか人間かを判定）
		    case TURN_CHECK:
		        if(game.is_AI_turn)
		        {
		            // AIのターンの場合、AI思考状態へ
		            state = AI_THINK;
		        }
		        else
		        {
		            // 人間のターンの場合、入力待ち状態へ
		            state = INPUT_WAIT;
		        }
		        break;

		    //********** AI思考フェーズ **********//

		    // AI思考状態
		    case AI_THINK:
		        // AIが次の手を決定
		        // 現在の盤面、コマの色、配置可能数、探索深度を渡す
		        set_AI_cursor_dest(board, cursor.color, (cursor.color == stone_red) ? red.placeable_count : green.placeable_count, AI_DEPTH);
		        // AI移動状態へ遷移
		        state = AI_MOVE;
		        break;

		    //********** プレイヤー入力フェーズ **********//

		    // 入力待ち状態
		    case INPUT_WAIT:
		        // IRQ1割り込みフラグ（決定ボタン）がセットされているか確認
		        if(select_btn_on)
		        {
		            // ボタンが押されたら配置チェック状態へ
		            state = PLACE_CHECK;

		            // 割り込みフラグをクリア
		            select_btn_on = 0;
		        }
		        else
		        {
		            // まだボタンが押されていない場合は入力読み取り状態へ
		            state = INPUT_READ;
		        }
		        break;

		    // 入力読み取り状態
		    case INPUT_READ:
		        // ロータリーエンコーダの現在値を読み取り
		        rotary.current = read_rotary();

		        // 左回転（反時計回り）が検出された場合
		        if(is_rotary_turned_left(&rotary))
		        {
		            // カーソルを移動（上下移動モードならDOWN、左右移動モードならLEFT）
		            move_cursor((MOVE_TYPE_UP_DOWN) ? DOWN : LEFT);

		            // 移動先の座標に応じた音階で音を鳴らす
		            beep(C_SCALE[(MOVE_TYPE_UP_DOWN) ? cursor.y : cursor.x], 100);

                    // 次回の比較用
		            rotary.prev += get_rotary_delta(&rotary);
		        }
		        // 右回転（時計回り）が検出された場合
		        else if(is_rotary_turned_right(&rotary))
		        {
		            // カーソルを移動（上下移動モードならUP、左右移動モードならRIGHT）
		            move_cursor((MOVE_TYPE_UP_DOWN) ? UP : RIGHT);

		            // 移動先の座標に応じた音階で音を鳴らす
		            beep(C_SCALE[(MOVE_TYPE_UP_DOWN) ? cursor.y : cursor.x], 100);

                    // 次回の比較用
		            rotary.prev += get_rotary_delta(&rotary);
		        }

		        // 入力待ち状態へ戻る
		        state = INPUT_WAIT;
		        break;

		    //********** AI自動移動フェーズ **********//

		    // AI移動状態
		    case AI_MOVE:
		        // X座標が目標より小さい場合、右へ移動
		        if(cursor.x < cursor.dest_x)
		        {
		            beep(C_SCALE[cursor.x], 100);
		            move_cursor(RIGHT);
		        }
		        // X座標が目標より大きい場合、左へ移動
		        else if(cursor.x > cursor.dest_x)
		        {
		            beep(C_SCALE[cursor.x], 100);
		            move_cursor(LEFT);
		        }

		        // Y座標が目標より小さい場合、上へ移動
		        if(cursor.y < cursor.dest_y)
		        {
		            beep(C_SCALE[cursor.y], 100);
		            move_cursor(UP);
		        }
		        // Y座標が目標より大きい場合、下へ移動
		        else if(cursor.y > cursor.dest_y)
		        {
		            beep(C_SCALE[cursor.y], 100);
		            move_cursor(DOWN);
		        }

		        // 目標位置に到達したか確認
		        if((cursor.x == cursor.dest_x) && (cursor.y == cursor.dest_y))
		        {
		            // 到達したら配置チェック状態へ
		            state = PLACE_CHECK;
		        }

		        // AI移動の待機時間
		        wait_10ms(AI_MOVE_PERIOD_MS / 10);
		        break;

		    //********** コマ配置フェーズ **********//

		    // 配置チェック状態
		    case PLACE_CHECK:
		        if(game.is_skip)
		        {
		            // スキップ（置ける場所がない）の場合は配置せずにターン終了
		            state = TURN_SWITCH;
		        }
		        else if(is_placeable(board, cursor.x, cursor.y, cursor.color))
		        {
		            // 配置可能な場合
		            state = PLACE_OK;
		        }
		        else
		        {
		            // 配置不可能な場合
		            state = PLACE_NG;
		        }
		        break;

		    // 配置成功状態
		    case PLACE_OK:
		        // 配置成功音を鳴らす
		        beep(DO2, 100);

		        // コマを配置
		        place(board, cursor.x, cursor.y, cursor.color);

		        // 盤面をLEDマトリクスに出力
		        flush_board(board);

		        // 反転計算状態へ遷移
		        state = FLIP_CALC;
		        break;

		    // 配置失敗状態
		    case PLACE_NG:
		        // エラー音を鳴らす
		        beep(DO0, 100);

		        // プレイヤーの場合は入力待ちに戻る
		        // AIの場合は理論上ここに来ない（AIは必ず置ける場所を選ぶため）
		        state = (game.is_AI_turn) ? TURN_START : INPUT_WAIT;
		        break;

		    //********** コマ反転フェーズ **********//

		    // 反転計算状態
		    case FLIP_CALC:
		        // どの方向のコマを反転させるかのフラグを作成
		        flip_dir_flag = make_flip_dir_flag(board, cursor.x, cursor.y, cursor.color);

		        // 反転実行状態へ遷移
		        state = FLIP_RUN;
		        break;

		    // 反転実行状態
		    case FLIP_RUN:
		        // コマを反転
		        flip_stones(flip_dir_flag, board, cursor.x, cursor.y, cursor.color);

		        // 盤面をLEDマトリクスに出力
		        flush_board(board);

		        // ターン切り替え状態へ遷移
		        state = TURN_SWITCH;
		        break;

		    //********** ターン終了フェーズ **********//

		    // ターン切り替え状態
		    case TURN_SWITCH:
		        // カーソルの色を反転（赤⇔緑）
		        cursor.color = ((cursor.color == stone_red) ? stone_green : stone_red);

		        // ターンカウント状態へ遷移
		        state = TURN_COUNT;
		        break;

		    // ターンカウント状態
		    case TURN_COUNT:
		        // 各プレイヤーの配置可能な場所の数を計算
		        red.placeable_count   = count_placeable(board, stone_red);
		        green.placeable_count = count_placeable(board, stone_green);

		        // ターン判定状態へ遷移
		        state = TURN_JUDGE;
		        break;

		    // ターン判定状態
		    case TURN_JUDGE:
		        // ゲーム終了条件をチェック（両者とも置けない場合）
		        if(is_game_over(red.placeable_count, green.placeable_count))
		        {
		            // ゲーム終了の場合、結果計算状態へ
		            state = END_CALC;
		        }
		        else
		        {
		            // ゲーム続行の場合、現在のプレイヤーがスキップかどうかを判定
		            game.is_skip = (cursor.color == stone_red) ? !red.placeable_count : !green.placeable_count;

		            // ターン表示状態へ遷移
		            state = TURN_SHOW;
		        }
		        break;

		    // ターン表示状態
		    case TURN_SHOW:
		        if(game.is_skip)
		        {
		            // スキップの場合、スキップメッセージを表示
		            lcd_show_skip_msg();
		        }
		        else
		        {
		            // 通常の場合、現在のターンを表示
		            lcd_show_whose_turn(cursor.color);
		        }

		        // AI対戦モードの場合、AIターンフラグを切り替え
		        if(game.is_man_vs_AI) game.is_AI_turn ^= 1;

		        // 次のターン開始状態へ遷移
		        state = TURN_START;
		        break;

		    //********** ゲーム終了フェーズ **********//

		    // 結果計算状態
		    case END_CALC:
		        // 赤・緑それぞれのコマの数を数える
		        red.result   = count_stones(board, stone_red);
		        green.result = count_stones(board, stone_green);

		        // 結果表示状態へ遷移
		        state = END_SHOW;
		        break;

		    // 結果表示状態
		    case END_SHOW:
		        
				// LCDをクリアして勝者発表メッセージ準備
				lcd_show_result_ready();

		        // カーソルを消す
		        cursor.color = stone_black;

		        // 盤面に結果を整列表示
		        line_up_result(board, red.result, green.result, LINE_UP_RESULT_PERIOD_MS / 10);

		        // 勝者を表示
		        lcd_show_winner(red.result, green.result);

		        // 結果表示待機時間
		        wait_10ms(SHOW_RESULT_WAIT_MS / 10);

		        // 確認メッセージを表示（再ゲーム確認）
		        lcd_show_confirm();

		        // 通常時:終了待ち状態へ遷移
                // AI vs AI時:ハードウェア初期化状態へ遷移
		        state = (init_option == OPT_NORMAL) ? END_WAIT : INIT_HW;

		        break;

		    // 終了待ち状態
		    case END_WAIT:
		        // IRQ1割り込みフラグ（決定ボタン）がセットされているか確認
		        if(select_btn_on)
		        {
		            // ボタンが押されたらフラグをクリアしてリセット状態へ
		            select_btn_on = 0;
		            state = END_RESET;
		        }
		        break;

		    // 終了リセット状態
		    case END_RESET:
		        // ハードウェア初期化状態へ遷移（ゲームを最初からやり直す）
		        state = INIT_HW;
		        break;
			
			// 未定義状態
			case STATE_UNDEFINED:
				nop();
				break;
			
		    // 予期しない状態の場合
		    default:
				// 未定義状態を通知
				lcd_show_state_err();
				state = STATE_UNDEFINED;
		        break;
		}
    }
}

#ifdef __cplusplus
extern "C" void abort(void) {}
#endif

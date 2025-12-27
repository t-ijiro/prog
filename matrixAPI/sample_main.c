//#include "typedefine.h"
#ifdef __cplusplus
//#include <ios>                        // Remove the comment when you use ios
//_SINT ios_base::Init::init_cnt;       // Remove the comment when you use ios
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <machine.h>
#include "iodefine.h"
#include "hardwareInit.h"
#include "task_flag.h"
#include "period.h"
#include "matrix.h"
#include "ucALPHABET.h"

#define NUM_CURSOR 3
#define NUM_COLOR  3

struct Cursor{
	uint8_t x;
	uint8_t y;
	enum led_color color;
	bool active;
};

volatile uint8_t timer_event_flag = 0x00;

void update_cursor(struct Cursor *cursor)
{
	cursor->x = (cursor->x + 1) % MAT_WIDTH;

	if(cursor->x == 0)
	{
		cursor->y = (cursor->y + 1) % MAT_HEIGHT;

		if(cursor->y == 0)
		{
			cursor->color = (enum led_color)(1 + (uint8_t)cursor->color % NUM_COLOR);
		}
	}
}

void main(void);
#ifdef __cplusplus
extern "C" {
void abort(void);
}
#endif

void main(void)
{
	uint8_t i;

	uint32_t counter_dynamic    = PERIOD_DYNAMIC_MS;
	uint32_t counter_gradation  = PERIOD_GRADATION_MS;
	uint32_t counter_scroll     = PERIOD_SCROLL_MS;
	
	uint8_t vert_cnt = 0;

	struct Cursor cursor[CURSOR_NUM] =
	{
		{0, 0, led_red,    true},
		{0, 0, led_green,  false},
		{0, 0, led_orange, false}
	};
	
	uint8_t counter_cursor_activation = 0;
	
	const char *TEXT         = "HELLOWORLD";
	const size_t TEXT_LENGTH = strlen(TEXT);
	uint8_t current_ch_idx  = 0;
	uint8_t scroll_line_pos = 0;
	
	init_CLK();
	init_CMT0();
	init_MATRIX();

	while(1)
	{
		// ************************************************************
		// 1ms周期タスク ソフトウェアタイマー生成処理
		// ************************************************************
		if(timer_event_flag & TASK_GEN_SOFTWARE_TIMER)
		{
			if(--counter_dynamic == 0)
			{
				timer_event_flag |= TASK_DYNAMIC;
				counter_dynamic = PERIOD_DYNAMIC_MS;
			}

			if(--counter_gradation == 0)
			{
				timer_event_flag |= TASK_GRADATION;
				counter_gradation = PERIOD_GRADATION_MS;
			}

			if(--counter_scroll == 0)
			{
				timer_event_flag |= TASK_SCROLL;
				counter_scroll = PERIOD_SCROLL_MS;
			}

			timer_event_flag &= ~TASK_GEN_SOFTWARE_TIMER;
		}
		
		// ************************************************************
		// 2ms周期タスク ダイナミック点灯処理
		// ************************************************************
		if(timer_event_flag & TASK_DYNAMIC)
		{
			vert_cnt = (vert_cnt + 1) % MAT_WIDTH;

			uint16_t vert_data = matrix_convert(vert_cnt);

			matrix_out(vert_cnt, vert_data);

			timer_event_flag &= ~TASK_DYNAMIC
		}
		
		// ************************************************************
		// 3ms周期タスク グラデーション処理
		// ************************************************************
		if(timer_event_flag & TASK_GRADATION)
		{
			for(i = 0; i < sizeof(cursor) / sizeof(cursor[0]); i++)
			{
				if(!cursor[i].active) continue;

				if(matrix_read(cursor[i].x, cursor[i].y) != led_off)
				{
					matrix_write(cursor[i].x, cursor[i].y, cursor[i].color);
				}

				update_cursor(&cursor[i]);
			}
			
			matrix_flush();
			
			if(!cursor[1].active || !cursor[2].active)
			{
				counter_cursor_activation++;

				if(counter_cursor_activation == MAT_PIXELS / 3)
				{
					cursor[1].active = true;
				}
			
				if(counter_cursor_activation == MAT_PIXELS * 2 / 3)
				{
					cursor[2].active = true;
				}
			}

			timer_event_flag &= ~TASK_GRADATION;
		}

		// ************************************************************
		// 300ms周期タスク スクロール処理
		// ************************************************************
		if(timer_event_flag & TASK_SCROLL)
		{
			matrix_scroll('l');
			
			char ch = TEXT[current_ch_idx];
			
			if(ch < 'A' || ch > 'Z')
			{
				timer_event_flag &= ~TASK_SCROLL;
				continue;
			}

			for(i = 0; i < MAT_HEIGHT; i++)
			{
				if(ALPHABET[ch - 'A'][scroll_line_pos] & (1 << i))
				{ 
					// 適当な色でマークだけつけておいてグラデーション処理で上書きする
					matrix_write(MAT_WIDTH - 1, i, led_red);
				}
				else
				{
					matrix_write(MAT_WIDTH - 1, i, led_off);
				}
			}
			
			// flushはしない
			
			scroll_line_pos ++;

			if(scroll_line_pos >= MAT_WIDTH)
			{
				scroll_line_pos = 0;

				current_ch_idx++;

				if(current_ch_idx >= TEXT_LENGTH)
				{
					current_ch_idx = 0;
				}
			}

			timer_event_flag &= ~TASK_SCROLL;
		}

		// ************************************************************
		// 
		// ************************************************************
	}
}

#ifdef __cplusplus
void abort(void)
{

}
#endif

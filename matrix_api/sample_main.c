/***********************************************************************/
/*                                                                     */
/*  FILE        : Main.c                                   */
/*  DATE        :Tue, Oct 31, 2006                                     */
/*  DESCRIPTION :Main Program                                          */
/*  CPU TYPE    :                                                      */
/*                                                                     */
/*  NOTE:THIS IS A TYPICAL EXAMPLE.                                    */
/*                                                                     */
/***********************************************************************/
//#include "typedefine.h"
#ifdef __cplusplus
//#include <ios>                        // Remove the comment when you use ios
//_SINT ios_base::Init::init_cnt;       // Remove the comment when you use ios
#endif

#include <machine.h>
#include "iodefine.h"
#include "vect.h"
#include "lcd_lib4.h"
#include "matrix.h"

const unsigned char arrow[4][8] = {
	{0x18, 0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x18, 0x00},
	{0x08, 0x18, 0x38, 0x7F, 0x7F, 0x38, 0x18, 0x08},
	{0x00, 0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18},
	{0x10, 0x18, 0x1C, 0xFE, 0xFE, 0x1C, 0x18, 0x10}
};

volatile unsigned char vert_cnt;
volatile unsigned char fore = 3;
volatile unsigned char back = 2;
volatile unsigned int An_X, An_Y;
volatile unsigned char dat;
volatile unsigned long time_10m_count;

void init_CLK()
{
	unsigned int i;

	SYSTEM.PRCR.WORD = 0xA50F;
	SYSTEM.VRCR = 0x00;
	SYSTEM.SOSCCR.BIT.SOSTP = 1;
	while (1 != SYSTEM.SOSCCR.BIT.SOSTP)
		;

	RTC.RCR3.BYTE = 0x0C;
	while (0 != RTC.RCR3.BIT.RTCEN)
		;

	SYSTEM.MOFCR.BYTE = (0x0D);
	SYSTEM.MOSCWTCR.BYTE = (0x0D);
	SYSTEM.MOSCCR.BIT.MOSTP = 0x00;
	while (0x00 != SYSTEM.MOSCCR.BIT.MOSTP)
		;

	for (i = 0; i < 100; i++)
		nop();

	SYSTEM.PLLCR.WORD = (0x0901);
	SYSTEM.PLLWTCR.BYTE = (0x09);
	SYSTEM.PLLCR2.BYTE = 0x00;

	for (i = 0; i < 100; i++)
		nop();

	SYSTEM.OPCCR.BYTE = (0x00);
	while (0 != SYSTEM.OPCCR.BIT.OPCMTSF)
		;

	SYSTEM.SCKCR.LONG = 0x21821211;
	while (0x21821211 != SYSTEM.SCKCR.LONG)
		;

	SYSTEM.SCKCR3.WORD = (0x0400);
	while ((0x0400) != SYSTEM.SCKCR3.WORD)
		;

	SYSTEM.PRCR.WORD = 0xA500;
}

void init_PORT(void)
{
	PORT1.PDR.BYTE = 0xE0;
	PORTE.PDR.BYTE = 0xFF;
}

void init_CMT0(void)
{
	SYSTEM.PRCR.WORD = 0x0A502;
	MSTP(CMT0) = 0;
	SYSTEM.PRCR.WORD = 0x0A500;

	SYSTEM.PRCR.WORD = 0x0000;
	CMT0.CMCOR = 250000 / 8 - 1;
	CMT0.CMCR.WORD |= 0x00C0;
	IEN(CMT0, CMI0) = 1;
	IPR(CMT0, CMI0) = 1;

	CMT.CMSTR0.BIT.STR0 = 1;
}

void init_CMT1(void)
{
	SYSTEM.PRCR.WORD = 0x0A502;
	MSTP(CMT1) = 0;
	SYSTEM.PRCR.WORD = 0x0A500;

	SYSTEM.PRCR.WORD = 0x0000;
	CMT1.CMCOR = 25000 / 8 - 1;
	CMT1.CMCR.WORD |= 0x00C0;
	IEN(CMT1, CMI1) = 1;
	IPR(CMT1, CMI1) = 1;

	CMT.CMSTR0.BIT.STR1 = 1;
}

void init_MTU1()
{
	SYSTEM.PRCR.WORD = 0x0A502;
	MSTP(MTU1) = 0;
	SYSTEM.PRCR.WORD = 0X0A500;

	PORT2.PMR.BIT.B4 = 1;
	PORT2.PMR.BIT.B5 = 1;
	MPC.PWPR.BIT.B0WI = 0;
	MPC.PWPR.BIT.PFSWE = 1;
	MPC.P24PFS.BIT.PSEL = 2;
	MPC.P25PFS.BIT.PSEL = 2;
	MPC.PWPR.BIT.PFSWE = 0;

	MTU1.TMDR.BIT.MD = 4;
	MTU1.TCNT = 0;
	MTU.TSTR.BIT.CST1 = 1;
}

void init_AD(void)
{
	SYSTEM.PRCR.WORD = 0xA502;
	MSTP(S12AD) = 0;
	SYSTEM.PRCR.WORD = 0xA500;
	PORT4.PMR.BYTE = 0x0f;
	S12AD.ADCSR.BIT.ADIE = 1;
	S12AD.ADANSA.BIT.ANSA0 = 1;
	S12AD.ADANSA.BIT.ANSA1 = 1;
	S12AD.ADANSA.BIT.ANSA2 = 1;
	S12AD.ADANSA.BIT.ANSA3 = 1;
	S12AD.ADCSR.BIT.ADCS = 0;
	MPC.PWPR.BIT.B0WI = 0;
	MPC.PWPR.BIT.PFSWE = 1;
	MPC.P40PFS.BIT.ASEL = 1;
	MPC.P41PFS.BIT.ASEL = 1;
	MPC.P42PFS.BIT.ASEL = 1;
	MPC.P43PFS.BIT.ASEL = 1;
	MPC.PWPR.BIT.PFSWE = 0;
	IEN(S12AD,S12ADI0) = 1;
	IPR(S12AD,S12ADI0) = 1;
}

void init_hardware(void)
{
	init_CLK();
	init_PORT();
	init_LCD();
	init_MATRIX();
	init_CMT0();
	init_CMT1();
	init_MTU1();
	init_AD();
	init_LCD();
	setpsw_i();
}

void ad_start(void)
{
	S12AD.ADCSR.BIT.ADST = 1;
}

void init_lcd_display(void)
{
	lcd_xy(1, 1);
	lcd_puts("rotary count");
	lcd_xy(1, 2);
	lcd_puts("fore:");
	lcd_dataout(fore);
	lcd_put(' ');
	lcd_puts("back:");
	lcd_dataout(back);
	flush_lcd();
}

void update_lcd_display(void)
{
	lcd_xy(6, 2);
	lcd_dataout(fore);
	lcd_xy(13, 2);
	lcd_dataout(back);
	flush_lcd();
}

void main(void);

#ifdef __cplusplus
extern "C" {
void abort(void);
}
#endif

void main(void)
{
	unsigned int roi = 0;
	unsigned int roi_pre = roi;

	init_hardware();
	init_lcd_display();
	
	time_10m_count = 0;

	while(1)
	{
		roi = MTU1.TCNT / 4;

		if((roi_pre < roi) || (roi_pre == 65535 / 4 && roi == 0))
		{
			fore = 1 + fore % 3;
		}
		else if((roi < roi_pre) || (roi == 65535 / 4 && roi_pre == 0))
		{
			back = 1 + back % 3;
		}

		if(roi != roi_pre)
		{
			update_lcd_display();
		}

		roi_pre = roi;
	}
}

#ifdef __cplusplus
void abort(void)
{

}
#endif

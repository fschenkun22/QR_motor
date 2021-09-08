#include "reg51.h"
#include "intrins.h"

#define FOSC 11059200UL
#define BRT (65536-FOSC/115200/4)

#define MAIN_Fosc		11059200L	//定义主时钟, 模拟串口和红外接收会自动适应。5~36MHZ

#define D_TIMER0		125			//选择定时器时间, us, 红外接收要求在60us~250us之间

#define	User_code		0xFF00		//定义红外接收用户码

#define freq_base			(MAIN_Fosc / 1200)
#define Timer0_Reload		(65536 - (D_TIMER0 * freq_base / 10000))


//****************本地变量************//
bit busy;
char wptr;
char rptr;
char buffer[16];
sbit	P_IR_RX = P5^4;		//红外


bit		P_IR_RX_temp;		//Last sample
bit		B_IR_Sync;			//已收到同步标志
unsigned char	IR_SampleCnt;		//采样计数
unsigned char	IR_BitCnt;			//编码位数
unsigned char	IR_UserH;			//用户码(地址)高字节
unsigned char	IR_UserL;			//用户码(地址)低字节
unsigned char	IR_data;			//数据原码
unsigned char	IR_DataShit;		//数据反码

bit		B_IrUserErr;		//User code error flag
bit		B_IR_Press;			//Key press flag,include repeat key.
unsigned char	IR_code;			//IR code	红外键码

/*************	本地函数声明	**************/

unsigned char 	HEX2ASCII(unsigned char dat);
void	InitTimer(void);


//*************串口********************//
void UartIsr() interrupt 4{
    if(TI){
        TI = 0;
        busy = 0;
    }
    if(RI){
        RI = 0;
        buffer[wptr++] = SBUF;
        wptr&=0x0f;
    }
}
void UartInit(){
    SCON = 0x50;
    TMOD = 0x00;
    TL1 = BRT;
    TH1 = BRT>>8;
    TR1 = 1;
    AUXR = 0x40;
    wptr = 0x00;
    rptr = 0x00;
    busy = 0;
}
void UartSend(char dat){
    while(busy);
    busy = 1;
    SBUF = dat;
}
void UartSendStr(char *p){
    while(*p)
    {
        UartSend(*p++);
    }
}
//**************************************

void main(){
    P3M0 = 0x00;
    P3M1 = 0x00;
    InitTimer();
    UartInit();
    ES = 1;
    EA = 1;
    UartSendStr("Uart send test \r\n");

    while(1){
        if(B_IR_Press){
            
            B_IR_Press = 0;		//清除IR键按下标志
            UartSendStr("have");
            UartSend(HEX2ASCII(IR_code>>4));
            UartSend(HEX2ASCII(IR_code));
            IR_code = 0;
        }
    }
}

/********************* 十六进制转ASCII函数 *************************/
unsigned char HEX2ASCII(unsigned char dat)
{
	dat &= 0x0f;
	if(dat <= 9)	return (dat + '0');	//数字0~9
	return (dat - 10 + 'A');			//字母A~F
}



/******************** 红外采样时间宏定义, 用户不要随意修改	*******************/

#if ((D_TIMER0 <= 250) && (D_TIMER0 >= 60))
#define	D_IR_sample			D_TIMER0		//定义采样时间，在60us~250us之间
#endif

#define D_IR_SYNC_MAX		(15000/D_IR_sample)	//SYNC max time
#define D_IR_SYNC_MIN		(9700 /D_IR_sample)	//SYNC min time
#define D_IR_SYNC_DIVIDE	(12375/D_IR_sample)	//decide data 0 or 1
#define D_IR_DATA_MAX		(3000 /D_IR_sample)	//data max time
#define D_IR_DATA_MIN		(600  /D_IR_sample)	//data min time
#define D_IR_DATA_DIVIDE	(1687 /D_IR_sample)	//decide data 0 or 1
#define D_IR_BIT_NUMBER		32					//bit number

//*******************************************************************************************

//**************************** IR RECEIVE MODULE ********************************************

void IR_RX_HT6121(void)
{
	unsigned char SampleTime;

	IR_SampleCnt++;							//Sample + 1

	F0 = P_IR_RX_temp;						//Save Last sample status
	P_IR_RX_temp = P_IR_RX;					//Read current status
	if(F0 && !P_IR_RX_temp)					//Last sample is high，and current sample is low, so is fall edge
	{
		SampleTime = IR_SampleCnt;			//get the sample time
		IR_SampleCnt = 0;					//Clear the sample counter

			 if(SampleTime > D_IR_SYNC_MAX)		B_IR_Sync = 0;	//large the Maxim SYNC time, then error
		else if(SampleTime >= D_IR_SYNC_MIN)					//SYNC
		{
			if(SampleTime >= D_IR_SYNC_DIVIDE)
			{
				B_IR_Sync = 1;					//has received SYNC
				IR_BitCnt = D_IR_BIT_NUMBER;	//Load bit number
			}
		}
		else if(B_IR_Sync)						//has received SYNC
		{
			if(SampleTime > D_IR_DATA_MAX)		B_IR_Sync=0;	//data samlpe time to large
			else
			{
				IR_DataShit >>= 1;					//data shift right 1 bit
				if(SampleTime >= D_IR_DATA_DIVIDE)	IR_DataShit |= 0x80;	//devide data 0 or 1
				if(--IR_BitCnt == 0)				//bit number is over?
				{
					B_IR_Sync = 0;					//Clear SYNC
					if(~IR_DataShit == IR_data)		//判断数据正反码
					{
						if((IR_UserH == (User_code / 256)) &&
							IR_UserL == (User_code % 256))
								B_IrUserErr = 0;	//User code is righe
						else	B_IrUserErr = 1;	//user code is wrong
							
						IR_code      = IR_data;
						B_IR_Press   = 1;			//数据有效
					}
				}
				else if((IR_BitCnt & 7)== 0)		//one byte receive
				{
					IR_UserL = IR_UserH;			//Save the User code high byte
					IR_UserH = IR_data;				//Save the User code low byte
					IR_data  = IR_DataShit;			//Save the IR data byte
				}
			}
		}
	}
}


/**************** Timer初始化函数 ******************************/
void InitTimer(void)
{
	TMOD = 0;		//for STC15Fxxx系列	Timer0 as 16bit reload timer.
	TH0 = Timer0_Reload / 256;
	TL0 = Timer0_Reload % 256;
	ET0 = 1;
	TR0 = 1;

	EA  = 1;
}


/********************** Timer0中断函数************************/
void timer0 (void) interrupt 1
{
	IR_RX_HT6121();
}





/*------------------------------------------------------------------*/
/* --- STC MCU International Limited -------------------------------*/
/* --- STC 1T Series MCU RC Demo -----------------------------------*/
/* --- Mobile: (86)13922805190 -------------------------------------*/
/* --- Fax: 86-755-82944243 ----------------------------------------*/
/* --- Tel: 86-755-82948412 ----------------------------------------*/
/* --- Web: www.STCMCU.com -----------------------------------------*/
/* If you want to use the program or the program referenced in the  */
/* article, please specify in which data and procedures from STC    */
/*------------------------------------------------------------------*/


/*************	功能说明	**************

红外接收程序。适用于市场上用量最大的HT6121/6122及其兼容IC的编码。

对于用户码与User_code定义不同的遥控器，程序会将用户码一起从串口输出。

使用模拟串口发送监控显示编码，显示内容为ASCII码和中文。

本接收程序基于状态机的方式，占用CPU的时间非常少。

HEX文件在本目录的/list里面。

******************************************/


/*************	用户系统配置	**************/

#define MAIN_Fosc		12000000L	//定义主时钟, 模拟串口和红外接收会自动适应。5~36MHZ

#define D_TIMER0		125			//选择定时器时间, us, 红外接收要求在60us~250us之间

#define	User_code		0xFF00		//定义红外接收用户码


/*************	以下宏定义用户请勿修改	**************/
#include	"reg51.H"
#define	uchar	unsigned char
#define uint	unsigned int

#define freq_base			(MAIN_Fosc / 1200)
#define Timer0_Reload		(65536 - (D_TIMER0 * freq_base / 10000))




/*************	本地常量声明	**************/
#define StepConter 600

/*************	本地变量声明	**************/
sbit	P_TXD1 = P0^1;		//定义模拟串口发送脚，打印信息用
sbit	P_IR_RX = P5^4;		//定义红外接收输入端口
sbit    DIR = P3^2;
sbit 	USB = P5^5;
sbit    Enabled = P3^1;
sbit 	STEP = P3^3;
sbit  	Senser = P3^0;
bit  	Setup;//是否进入配置模式


bit		P_IR_RX_temp;		//Last sample
bit		B_IR_Sync;			//已收到同步标志
uchar	IR_SampleCnt;		//采样计数
uchar	IR_BitCnt;			//编码位数
uchar	IR_UserH;			//用户码(地址)高字节
uchar	IR_UserL;			//用户码(地址)低字节
uchar	IR_data;			//数据原码
uchar	IR_DataShit;		//数据反码

bit		B_IrUserErr;		//User code error flag
bit		B_IR_Press;			//Key press flag,include repeat key.
uchar	IR_code;			//IR code	红外键码

bit 	CounterStatus;//计数器到位状态
uchar 	Counter;
/*************	本地函数声明	**************/
void	Tx1Send(uchar dat);
uchar	HEX2ASCII(uchar dat);
void	InitTimer(void);
void	PrintString(unsigned char code *puts);


/*************  自定函数 *********************/
void Delay5ms()		//@12.000MHz
{
	unsigned char i, j;

	i = 10;
	j = 183;
	do
	{
		while (--j);
	} while (--i);
}

void Delay100us()		//@12.000MHz
{
	unsigned char i, j;

	i = 2;
	j = 189;
	do
	{
		while (--j);
	} while (--i);
}



/*************  外部函数和变量声明 *****************/
void IntStart(void){
	P5M0 = 0;
	P5M1 = 0;
	P3M0 = 0;
	P3M1 = 0;
	Enabled = 0;
	CounterStatus = 1;
	Counter = 0;
	Setup = 0;
}

void MoveStep(void){
	STEP = 0;
	Delay5ms();
	STEP = 1;
	Delay5ms();
}

void MoveStepByCounter(cont,dir){
	DIR = dir;

	while(cont){
		MoveStep();
		cont--;
	}
}

void Reset(void){
	while(Senser==0){
		DIR = 0;//往回移动
		STEP = 0;
		Delay5ms();
		STEP = 1;
		Delay5ms();
	}
	//直到检测到传感器复位了
}

void ToCounter(void){
	uchar i = Counter;
	for(i=Counter; i>0;i--){
		MoveStepByCounter(StepConter,1);
	}
}



void Beep_setup(void){
	int i=0;
	for(i = 0;i<4096;i++){
		Enabled = 1;
		Delay100us();
		Enabled = 0;
		Delay100us();
	}

	for(i = 0;i<1024;i++){
		Enabled = 0;
		Delay100us();
		Enabled = 0;
		Delay100us();
	}

	for(i = 0;i<1024;i++){
		Enabled = 1;
		Delay100us();
		Enabled = 0;
		Delay100us();
	}

	for(i = 0;i<512;i++){
		Enabled = 0;
		Delay100us();
		Enabled = 0;
		Delay100us();
	}

	for(i = 0;i<1024;i++){
		Enabled = 1;
		Delay100us();
		Enabled = 0;
		Delay100us();
	}

}


//下一步开始写 按次数移动 1就调用一次move step by conter

/********************* 主函数 *************************/
void main(void)
{
	InitTimer();		//初始化Timer
	IntStart();
	//复位后检查是否按setup
	// if(B_IR_Press){
	// 	if(IR_code == 0x46){
	// 		Setup = 1;
	// 		B_IR_Press = 0;		//清除IR键按下标志
	// 		MoveStepByCounter(StepConter,1);
	// 	}
	// }

	//配置模式 Setup为1一直保持在配置模式，配置模式不管USB状态
	while(1)
	{
	    if(B_IR_Press)		//有IR键按下
		{
			if(IR_code == 0x46){
				IR_code = 0;
				// MoveStepByCounter(StepConter,1);
				B_IR_Press = 0;		//清除IR键按下标志
				Setup = 1;
				Beep_setup();//叫一声
				Reset();//进入配置前先复位
			}

			while (Setup) //进入配置模式，配置模式不检测USB
			{
				
				
				if(B_IR_Press){

					if(IR_code == 0x43){
						IR_code = 0;
						MoveStepByCounter(StepConter,1);
						B_IR_Press = 0;		//清除IR键按下标志
						Counter++;//将来加上counter最大值
					}

					if(IR_code == 0x40){
						IR_code = 0;
						MoveStepByCounter(StepConter,0);
						B_IR_Press = 0;		//清除IR键按下标志
						Counter--;//注意这里应该限制负值！！！
					}

					if(IR_code == 0x46){
						IR_code = 0;
						B_IR_Press = 0;		//清除IR键按下标志
						Setup = 0;
						//如果是46 证明按下的是mode按键，准备退出配置

						//退出前保存counter

						//退出前运行复位
						Reset();
					}

				}
			}

		}

		//接下来判断USB，也就是正常工作模式，USB如果为0 说明插入设备，就运行到counter，如果设备为1，就复位等待
		if (USB==0)
		{
			//如果USB接触不良，马上断开再接通可能出现继续出仓，所以应该加上强制复位后再出仓
			Reset();
			ToCounter();
			while (USB==0)
			{
				//原地等待 
			}
		}

		Reset();



	}
}


//*******************************************************************
//*********************** IR Remote Module **************************

//*********************** IR Remote Module **************************
//this programme is used for Receive IR Remote (HT6121).

//data format: Synchro,AddressH,AddressL,data,/data, (total 32 bit).

//send a frame(85ms), pause 23ms, send synchro of another frame, pause 94ms

//data rate: 108ms/Frame


//Synchro:low=9ms,high=4.5/2.25ms,low=0.5626ms
//Bit0:high=0.5626ms,low=0.5626ms
//Bit1:high=1.6879ms,low=0.5626ms
//frame space = 23 ms or 96 ms

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
	uchar	SampleTime;

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



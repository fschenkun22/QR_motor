#include "reg51.h"
#include "intrins.h"

#define FOSC 11059200UL
#define BRT (65536-FOSC/115200/4)

//****************本地变量************//
bit busy;
char wptr;
char rptr;
char buffer[16];
sbit	P_IR_RX = P5^4;		//红外

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

void main(){
    P3M0 = 0x00;
    P3M1 = 0x00;

    UartInit();
    ES = 1;
    EA = 1;
    UartSendStr("Uart send test \r\n");

    while(1){
        UartSendStr("test uart \r\n");
    }
}
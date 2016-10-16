#include <REG_MG82FL524-564.H>
#include <stdio.h>
#include "BIT.h"
#include "UART.h"
#define XTAL 22118400
#define TAKE_LOG2_T0_ISR_FREQ 10	//log2(1024)=10
#define T0_ISR_FREQ (1<<TAKE_LOG2_T0_ISR_FREQ)
#define TIMER0_INTERVAL ((XTAL/12)/T0_ISR_FREQ)
#define TH0_NUM (65535-TIMER0_INTERVAL)>>8
#define TL0_NUM (65535-TIMER0_INTERVAL)&0x00ff
#define LIMIT 	2000000000	//for difference and integral
#define PWM_RESOLUTION_BIT 10

void SetFeedback(void);
void SetPWM_8bit(void);
void SetPWM_10bit(void);
void SetPWM_16bit(void);
void dealy1ms(unsigned int x)
{unsigned int i,j;
	
	for(i=0;i<x;++i)
		for(j=0;j<1990;++j);
}
void DemoProgram(void);

char i;
char RxBuf;
unsigned char TxFlag=0;
unsigned char Counter10ms=0;
unsigned char ucUsedChannel=1;

int Position=511;
int KP=90;KI=5,KD=7,K_Num=0;
long int u=0;
long int KI_Sum=0,KD_Diff=0;
int ADCHL=0;


int main()
{ 
	UART_Initial();
	
	EA=ET0=ES=1; //啟用中斷
	
	TMOD|=0x01;// 使用Timer0 mode1
	TR0=1;//啟動Timer0
	
	//*************for PWM***********//
	//這裡要啟用兩個PWM
	P1M0&=~BIT[2],P1M1|=BIT[2]; //Set 01
	P1M0&=~BIT[3],P1M1|=BIT[3]; //Set 01
	//CMOD=0x00; //系統頻率/12
	CMOD=0x02; //系統頻率/2
	//PCAPWM0|=0x04; //Set PWM0 Inv out
	//PCAPWM1|=0x04; //Set PWM1 Inv out
	CCAP0H=(~0x00)&0xFF; //Auto Load to CCAPnL
	CCAP1H=(~0x00)&0xFF; //Auto Load to CCAPnL
	CCAPM0=CCAPM1=0x42; //table 14.4建議為0x42, why?
	CR=1;//啟動PCA
	//*************for PWM***********//
	
	//*************for ADC***********//
	/*
		1.開啟ADC硬體
		2.設定轉速
		3.選頻道
		4.被選中的頻道要設定成Input-Only Mode
		5.以ADRJ設定ADC結果格式
		*/
		
		ADCON=0x80; //步驟1~3
		//===ADCEN==SPEED1==SPEED2==ADCI==ADCS==CH2==CH1=CH0=
		//=====1======1=======1======0======0====0====0===0==
		
		//步驟4
		//有使用的通道Input-Only Mode
		for(i=0;i<ucUsedChannel;++i)
		{
			P1M0|=BIT[i];		//Set 1
			P1M1&=~BIT[i];	//Set 0
		}
		
		//步驟5
		//設定ADC結果格式by ADRJ(in AUXR0)
		AUXR0&=~BIT2;//ADCJ=1;
	//*************for ADC***********//
	
	//DemoProgram();
		
	while(1)
	{
		if(TxFlag>(T0_ISR_FREQ/16))//每秒發送8次
		{	
			P35=1;
			printf("ADCHL=%d, u=%ld, PWM=%ld%%\n\0",ADCHL,u,(u*100)/(1<<PWM_RESOLUTION_BIT));	//後兩者要用%ld,不可用%d,否則無法正常顯示
			TxFlag=0;
			P35=0;
		}
		
	}
}


void Timer0_INT(void) interrupt 1
{//定時
	//Reload, 1ms 中斷一次
	P36=1;
	TR0=0;//停止Timer0
	TH0=TH0_NUM;
	TL0=TL0_NUM;
	TR0=1;//啟動Timer0
			
	//切換頻道 & AD轉換 & 讀取 & 轉換資料格式
	for(i=0;i<ucUsedChannel;++i)
	{
		//ADCON&=0xF8; //清除CH2,1,0
		ADCON|=i; //設定這次取樣的頻道
		ADCON|=BIT3;	//ADCS=1,開始轉換
		P37=1;
		while(!(ADCON&BIT4));	//等待ADCI=1(轉換完畢),這行會讓UART失效
		P37=0;
		ADCON&=(~BIT4);	//然後清除ADCI
	}
	
	TxFlag++;
	SetFeedback();
	P36=0;
}

void UART_ISR(void) interrupt 4
{
	if(RI)
		{
			RI=0;
			RxBuf = SBUF;
			
			if(RxBuf>96 && RxBuf<123)RxBuf-=32; //小寫變大寫
			
			switch(RxBuf)
			{
				case'L':Position=318;break;
				case'M':Position=511;break;
				case'R':Position=690;break;
				case'+':Position++,printf("\nPosition=%d\n",Position);break;
				case'-':Position--,printf("\nPosition=%d\n",Position);break;
				
				case'P':KP=K_Num;K_Num=0;printf("\n KP=%d \n",KP);break;
				case'I':KI=K_Num;K_Num=0;printf("\n KI=%d \n",KI);break;
				case'D':KD=K_Num;K_Num=0;printf("\n KD=%d \n",KD);break;
				
				default:
					if(RxBuf>47 && RxBuf<58)//0~9
					{
						RxBuf-=48; //ASCII to Dec
						K_Num = K_Num*10;
						K_Num = K_Num + RxBuf;
					}
					break;
			}
		}
}

void SetFeedback(void)
{
	static long int tempErr;
	long int Err;
	
	ADCHL = ADCH<<2 | ADCL; //載入揮發性變數
	Err = ((long int)(ADCHL - Position))<<TAKE_LOG2_T0_ISR_FREQ; //求誤差
	
	//*************比例***********//
	u=KP*Err;
	//*************比例***********//
	
	//*************積分***********//
	KI_Sum += Err;
	if		 (KI_Sum> LIMIT)KI_Sum= LIMIT;
	else if(KI_Sum<-LIMIT)KI_Sum=-LIMIT;
	//if((Position-1)<=ADCHL || (Position+1)>=ADCHL)KI_Sum=0;//積分重置
	u+=KI*KI_Sum>>TAKE_LOG2_T0_ISR_FREQ;
	//*************積分***********//
	
	//*************微分***********//
	KD_Diff = (Err-tempErr)<<TAKE_LOG2_T0_ISR_FREQ;
	tempErr=Err;
	if		 (KD_Diff> LIMIT)KD_Diff= LIMIT;
	else if(KD_Diff<-LIMIT)KD_Diff=-LIMIT;
	u+=KD*KD_Diff;
	//*************微分***********//
	
	u=u>>TAKE_LOG2_T0_ISR_FREQ;	//尺度還原(注意signed bit遺失問題!)

	SetPWM_10bit();
}

void DemoProgram(void)
{unsigned DemoTime=3333;
	
	//左45度
	Position=75;
	dealy1ms(DemoTime);
	
	//右45度
	Position=179;
	dealy1ms(DemoTime);
	
	//追逆時針ramp
	while(Position!=10)
	{
		Position--;
		dealy1ms(70);
	}dealy1ms(1000);
	
	//追順時針ramp
	while(Position!=245)
	{
		Position++;
		dealy1ms(70);
	}dealy1ms(1000);
	
	//回到正中間
	Position=127;
	dealy1ms(DemoTime);
}

void SetPWM_8bit(void)
{
	if(u>256) u=256;				//設定上限
	else if(u<-256) u=-256;	//設定下限
	
	if(u>0) //正轉
	{
		PCAPWM1=0x03,CCAP1H = 0x00; //Duty Cycle = 0%
		PCAPWM0=0x00;CCAP0H = 0x100-u;
	}
	else if(u<0) //反轉
	{
		PCAPWM0=0x03,CCAP0H = 0x00; //Duty Cycle = 0%
		PCAPWM1=0x00;CCAP1H = 0x100+u;
	}
	else if(0==u)
	{
		PCAPWM0=0x03,CCAP0H=0x00;
		PCAPWM1=0x03,CCAP1H=0x00;
	}
}

void SetPWM_16bit(void)
{
	if(u>65536) u=65536;				//設定上限
	else if(u<-65536) u=-65536;	//設定下限
	
	if(u>0) //正轉
	{
		PCAPWM1=0xC3,CCAP1H = CCAP1L =0x00; //Duty Cycle = 0%
		PCAPWM0=0xC0;CCAP0H = (0x10000-u)>>8;CCAP0L = 0xFF&(0x10000-u);
	}
	else if(u<0) //反轉
	{
		PCAPWM0=0xC3,CCAP0H = CCAP0L =0x00; //Duty Cycle = 0%
		PCAPWM1=0xC0;CCAP1H = (0x10000+u)>>8;CCAP1L = 0xFF&(0x10000+u);
	}
	else if(0==u)
	{
		PCAPWM0=0xC3,CCAP0H=CCAP0L=0x00;
		PCAPWM1=0xC3,CCAP1H=CCAP1L=0x00;
	}
}

void SetPWM_10bit(void)
{
	if(u>1024) u=1024;				//設定上限
	else if(u<-1024) u=-1024;	//設定下限
	
	if(u>0) //正轉
	{
		PCAPWM1=0x43,CCAP1H = CCAP1L =0x00; //Duty Cycle = 0%
		PCAPWM0=0x40;CCAP0H = (0x400-u)>>8;CCAP0L = 0xFF&(0x400-u);
	}
	else if(u<0) //反轉
	{
		PCAPWM0=0x43,CCAP0H = CCAP0L =0x00; //Duty Cycle = 0%
		PCAPWM1=0x40;CCAP1H = (0x400+u)>>8;CCAP1L = 0xFF&(0x400+u);
	}
	else
	{
		PCAPWM0=0x43,CCAP0H=CCAP0L=0x00;
		PCAPWM1=0x43,CCAP1H=CCAP1L=0x00;
	}
}
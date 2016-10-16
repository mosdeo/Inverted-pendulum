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
	
	EA=ET0=ES=1; //�ҥΤ��_
	
	TMOD|=0x01;// �ϥ�Timer0 mode1
	TR0=1;//�Ұ�Timer0
	
	//*************for PWM***********//
	//�o�̭n�ҥΨ��PWM
	P1M0&=~BIT[2],P1M1|=BIT[2]; //Set 01
	P1M0&=~BIT[3],P1M1|=BIT[3]; //Set 01
	//CMOD=0x00; //�t���W�v/12
	CMOD=0x02; //�t���W�v/2
	//PCAPWM0|=0x04; //Set PWM0 Inv out
	//PCAPWM1|=0x04; //Set PWM1 Inv out
	CCAP0H=(~0x00)&0xFF; //Auto Load to CCAPnL
	CCAP1H=(~0x00)&0xFF; //Auto Load to CCAPnL
	CCAPM0=CCAPM1=0x42; //table 14.4��ĳ��0x42, why?
	CR=1;//�Ұ�PCA
	//*************for PWM***********//
	
	//*************for ADC***********//
	/*
		1.�}��ADC�w��
		2.�]�w��t
		3.���W�D
		4.�Q�襤���W�D�n�]�w��Input-Only Mode
		5.�HADRJ�]�wADC���G�榡
		*/
		
		ADCON=0x80; //�B�J1~3
		//===ADCEN==SPEED1==SPEED2==ADCI==ADCS==CH2==CH1=CH0=
		//=====1======1=======1======0======0====0====0===0==
		
		//�B�J4
		//���ϥΪ��q�DInput-Only Mode
		for(i=0;i<ucUsedChannel;++i)
		{
			P1M0|=BIT[i];		//Set 1
			P1M1&=~BIT[i];	//Set 0
		}
		
		//�B�J5
		//�]�wADC���G�榡by ADRJ(in AUXR0)
		AUXR0&=~BIT2;//ADCJ=1;
	//*************for ADC***********//
	
	//DemoProgram();
		
	while(1)
	{
		if(TxFlag>(T0_ISR_FREQ/16))//�C��o�e8��
		{	
			P35=1;
			printf("ADCHL=%d, u=%ld, PWM=%ld%%\n\0",ADCHL,u,(u*100)/(1<<PWM_RESOLUTION_BIT));	//���̭n��%ld,���i��%d,�_�h�L�k���`���
			TxFlag=0;
			P35=0;
		}
		
	}
}


void Timer0_INT(void) interrupt 1
{//�w��
	//Reload, 1ms ���_�@��
	P36=1;
	TR0=0;//����Timer0
	TH0=TH0_NUM;
	TL0=TL0_NUM;
	TR0=1;//�Ұ�Timer0
			
	//�����W�D & AD�ഫ & Ū�� & �ഫ��Ʈ榡
	for(i=0;i<ucUsedChannel;++i)
	{
		//ADCON&=0xF8; //�M��CH2,1,0
		ADCON|=i; //�]�w�o�����˪��W�D
		ADCON|=BIT3;	//ADCS=1,�}�l�ഫ
		P37=1;
		while(!(ADCON&BIT4));	//����ADCI=1(�ഫ����),�o��|��UART����
		P37=0;
		ADCON&=(~BIT4);	//�M��M��ADCI
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
			
			if(RxBuf>96 && RxBuf<123)RxBuf-=32; //�p�g�ܤj�g
			
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
	
	ADCHL = ADCH<<2 | ADCL; //���J���o���ܼ�
	Err = ((long int)(ADCHL - Position))<<TAKE_LOG2_T0_ISR_FREQ; //�D�~�t
	
	//*************���***********//
	u=KP*Err;
	//*************���***********//
	
	//*************�n��***********//
	KI_Sum += Err;
	if		 (KI_Sum> LIMIT)KI_Sum= LIMIT;
	else if(KI_Sum<-LIMIT)KI_Sum=-LIMIT;
	//if((Position-1)<=ADCHL || (Position+1)>=ADCHL)KI_Sum=0;//�n�����m
	u+=KI*KI_Sum>>TAKE_LOG2_T0_ISR_FREQ;
	//*************�n��***********//
	
	//*************�L��***********//
	KD_Diff = (Err-tempErr)<<TAKE_LOG2_T0_ISR_FREQ;
	tempErr=Err;
	if		 (KD_Diff> LIMIT)KD_Diff= LIMIT;
	else if(KD_Diff<-LIMIT)KD_Diff=-LIMIT;
	u+=KD*KD_Diff;
	//*************�L��***********//
	
	u=u>>TAKE_LOG2_T0_ISR_FREQ;	//�ث��٭�(�`�Nsigned bit�򥢰��D!)

	SetPWM_10bit();
}

void DemoProgram(void)
{unsigned DemoTime=3333;
	
	//��45��
	Position=75;
	dealy1ms(DemoTime);
	
	//�k45��
	Position=179;
	dealy1ms(DemoTime);
	
	//�l�f�ɰwramp
	while(Position!=10)
	{
		Position--;
		dealy1ms(70);
	}dealy1ms(1000);
	
	//�l���ɰwramp
	while(Position!=245)
	{
		Position++;
		dealy1ms(70);
	}dealy1ms(1000);
	
	//�^�쥿����
	Position=127;
	dealy1ms(DemoTime);
}

void SetPWM_8bit(void)
{
	if(u>256) u=256;				//�]�w�W��
	else if(u<-256) u=-256;	//�]�w�U��
	
	if(u>0) //����
	{
		PCAPWM1=0x03,CCAP1H = 0x00; //Duty Cycle = 0%
		PCAPWM0=0x00;CCAP0H = 0x100-u;
	}
	else if(u<0) //����
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
	if(u>65536) u=65536;				//�]�w�W��
	else if(u<-65536) u=-65536;	//�]�w�U��
	
	if(u>0) //����
	{
		PCAPWM1=0xC3,CCAP1H = CCAP1L =0x00; //Duty Cycle = 0%
		PCAPWM0=0xC0;CCAP0H = (0x10000-u)>>8;CCAP0L = 0xFF&(0x10000-u);
	}
	else if(u<0) //����
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
	if(u>1024) u=1024;				//�]�w�W��
	else if(u<-1024) u=-1024;	//�]�w�U��
	
	if(u>0) //����
	{
		PCAPWM1=0x43,CCAP1H = CCAP1L =0x00; //Duty Cycle = 0%
		PCAPWM0=0x40;CCAP0H = (0x400-u)>>8;CCAP0L = 0xFF&(0x400-u);
	}
	else if(u<0) //����
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
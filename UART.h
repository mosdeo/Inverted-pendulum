void UART_Initial(void)
{
//*************for UART***********//
	TMOD|=0x20;// �ϥ�Timer1 mode2
	SCON=0x50; //
	TH1=TL1=253; 
	//PCON|=0x80;//SMOD=1;
	TR1=1; /*Timer1 �Ұ�!*/
	TI=1; //�϶ǰe�i�H�ǰe
	//*************for UART***********//
}
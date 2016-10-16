void UART_Initial(void)
{
//*************for UART***********//
	TMOD|=0x20;// 使用Timer1 mode2
	SCON=0x50; //
	TH1=TL1=253; 
	//PCON|=0x80;//SMOD=1;
	TR1=1; /*Timer1 啟動!*/
	TI=1; //使傳送可以傳送
	//*************for UART***********//
}
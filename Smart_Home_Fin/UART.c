/*
 * 
 *
 * Created: 2021-04-06 오후 3:40:18
 *  Author: 노형우
 */ 
/* UART0.c		인터럽트 분기문을 사용해서 만듬
 * UART1.c
 *
 * Created: 2021-04-06 오후 3:25:35
 *  Author: ComputerSystem
 
	UART0의 통신속도를 // 115200 보율로 설정함
	UART1의 통신속도를 // 9600보울로 설정함
	
 * 
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "UART.h"		//BUFSIZE 떄문에 해더 파일 선언



char rx0data[BUFSIZE];			// 배열 선언
char rx1data[BUFSIZE];
volatile int rx0flag;
volatile int rx1flag;

//UART0 통신 설정
void UART0_init(void)
{
	UCSR0B |= _bv(RXEN0);		// 송수신 가능
	UCSR0B |= _bv(TXEN0);
	UCSR0B |= _bv(RXCIE0);	//RX Complete Interrupt Enable
	// 비동기, 8비트 데이터, 패리티 없음, 1비트 정지 비트 모드
	//	UCSR0C |= 0x06;
	UCSR0C |= _bv(UCSZ01); // 1 << 1
	UCSR0C |= _bv(UCSZ00); // 1 << 2
	UBRR0H = 0;			// 9600 보율로 설정
	UBRR0L = 103;
}
// 데이터를 받는 인터럽트가 발생하면	

// UART1 통신 설정
void UART1_init(void){
	UBRR1H = 0x00;		//9600 baud 설정
	UBRR1L = 207;
	UCSR1A = _bv(U2X1);		// 2배속 모드
	// 비동기, 8비트 데이터, 패리티 없음, 1비트 정지 비트 모드
	UCSR1C |= 0x06;
	UCSR1B |= _bv(RXCIE1);	//RX Complete Interrupt Enable
	UCSR1B |= _bv(RXEN1);	 
	UCSR1B |= _bv(TXEN1);
}

ISR(USART0_RX_vect)
{
	static int i = 0;
	rx0data[i] = UDR0;	// 인터럽트 발생시 배열에 저장
	if(rx0data[i] == '\r')	// /r문자를 만나면 널문자 저장
	{
		rx0data[i]='\0';
		rx0flag = 1;		// flag 변수 활성화
		i=0;
	}
	else
		i++;
}

ISR(USART1_RX_vect)
{	
	static int i=0;
	char ISR_RX_data = UDR1;
	if(i == 0 && (ISR_RX_data == '\n' || ISR_RX_data == '\0')){
		//UART1_print_string("UDR1 data is 개행문자 or 널문자\r\n");
	}
	else{
		rx1data[i] = ISR_RX_data;
		if(rx1data[i] == '\r')  //CR,LF 수신 문자열 끝
		{
			rx1data[i] = '\0';
			/*내가 사용하는 블루투스 용*/
			if(!(strncmp(rx1data, "OK+LOSTOK+CONN", 14)) ){		//CONNECT를 할때
				strcpy(rx1data, rx1data + 14); 
			}
			else if(!(strncmp(rx1data, "OK+CONN", 7)) ){		// mcu를 처음 킬떄
				strcpy(rx1data, rx1data + 7);
			}
			rx1flag = 1;
			i=0;
		}
		else
		{
			i++;
			if(i==BUFSIZE)
			{
				rx1data[i-1] = '\0';
				rx1flag = 1;
				i=0;
			}
		}
	}
}

void UART0_transmit(char data)
{
	while( !(UCSR0A & (1 << UDRE0)) );	// 송신 가능 대기
	UDR0 = data;				// 데이터 전송
}

// 문자열을 출력할때 매개변수를 포인터로 받음 
void UART0_print_string(char * str){		
	/*while(*str != 0){
		UART0_transmit(*str);		//  transmit함수에 데이터 저장
		str++;						// 값 증가 
	}*/
	for(int i = 0; str[i]; i++)
		UART0_transmit(str[i]);
}
// 문자 데이터 한개를 받는 함수
unsigned char UART0_receive(void)
{
	while( !(UCSR0A & (1<<RXC0)) );	// 데이터 수신 대기
		return UDR0;				// UDR0 값 반환
/*	if(UCSR0A & (1<<RXC0))
		return UDR0;
	else
		return 0;
*/
}


// 데이터를 (시리얼로) 보내는 함수
void UART1_transmit(char data){
	while( !(UCSR1A & (1 << UDRE1)) );	//송신 가능 대기
	UDR1 = data;			//데이터 전송
}
// 데이터를 받는함수(시리얼에서)
unsigned char UART1_receive(void){
	while( !(UCSR1A & (1 << RXC1)) );	// 송신 가능 대기
	return UDR1;
}
// 문자열 송신 
void UART1_print_string(char *str){
	for(int i = 0; str[i]; i++)		//'0'문자를 만날때까지 반복
		UART1_transmit(str[i]);
}
// UART 통신으로 1바이트 크기 정수 
void UART1_print_1_byte_number(uint8_t n){
	char numString[4] = "0";
	int i, index = 0;
	
	if( n > 0){
		for(i = 0; n!=0; i++){
			numString[i] = n %10  + '0';
			n = n /10;
		}
		numString[i] = '\0';
		index = i -1;
	}
	
	for(i= index; i >= 0; i--)
		UART1_transmit(numString[i]);
}

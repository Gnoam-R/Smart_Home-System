/*
 * UART1.h
 *
 * Created: 2021-04-06 오후 3:41:13
 *  Author: ComputerSystem
 */ 


#ifndef UART_H_
#define UART_H_

#define BUFSIZE 100
#define _bv(bit) (1 << bit)		// 전처리  비트연산

//uart 통신 설정 및 초기화
void UART0_init(void);	//	115200 baud
void UART1_init(void);	//	9600 baud

void UART0_transmit(char data);		//데이터 1개 송신 함수
void UART1_transmit(char data);

unsigned char UART0_receive(void);	// 데이터 1개 수신 함수	
unsigned char UART1_receive(void);	

void UART0_print_string(char *str);	//문자열 송신
void UART1_print_string(char *str);		

void UART1_print_1_byte_number(uint8_t n);	//UART 통신으로 1바티으 크기 정수 출력

#endif /* UART_H_ */
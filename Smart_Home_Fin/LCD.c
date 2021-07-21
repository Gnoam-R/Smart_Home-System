/*
 * LCD.c
 *
 * Created: 2021-04-08 오후 12:16:53
 *  Author: ComputerSystem
 */ 

#include <avr/io.h>
#include <util/delay.h>

// 핀 설정을 바꾸려면 여기서 설정
#define PORT_DATA		PORTC		// 데이터 핀 연결 포트
#define PORT_CONTROL	PORTA		// 제어 핀 연결 포트
#define DDR_DATA		DDRC		//데이터 핀의 데이터 방향
#define DDR_CONTROL		DDRA		// 제어 핀의 데이터 방향

#define RS_PIN		5		// RS 제어 핀의 비트 번호
#define RW_PIN		6		//	R/W 제어 핀의 비트 번호
#define E_PIN		7		//	E 제어 핀의 비트 번호

#define COMMAND_CLEAR_DISPLAY		0x01
#define COMMAND_8_BIT_MODE			0x38		// 8비트, 2라인 5 x 8 폰트
#define COMMAND_4_BIT_MODe			0x28		// 4비트, 2라인, 5 x 8 폰트

#define COMMAND_DISPLAY_ON_OFF_BIT		2		// 디스플레이
#define COMMAND_CURSOR_ON_OFF_BIT		1		// 커서
#define COMMAND_BLINK_ON_OFF_BIT		0		// 깜빡임


//////////딜레이 사용했다.../////
void LCD_pulse_enable(void){			// 하강 에지에서 동작
	PORT_CONTROL |= (1 << E_PIN);		// E를 HIGH로
	_delay_ms(5);
	PORT_CONTROL &= ~(1 << E_PIN);		// E를 LOW로
	_delay_ms(5);
}

void LCD_write_data(uint8_t data){
	PORT_CONTROL |= (1 << RS_PIN);		// 문자 출력에서 RS는 1
	PORT_DATA = data;					// 출력할 문자 데이터(data)
	LCD_pulse_enable();
	_delay_ms(1);
}
void LCD_write_command(uint8_t command){
	PORT_CONTROL &= ~(1 <<RS_PIN);		// 명령어 실행에서 RS 는 0
	PORT_DATA = command;				//	데이터 핀에 명령어 전달
	LCD_pulse_enable();					// 명령어 실행
	_delay_ms(2);
}
void LCD_clear(void){
	LCD_write_command(COMMAND_CLEAR_DISPLAY);
	_delay_ms(2);
}
void LCD_init(void){
	_delay_ms(50);
	
	// 연결 핀을 출력으로 설정
	DDR_DATA = 0xFF;
	PORT_DATA = 0x00;
	DDR_CONTROL |= (1 << RS_PIN) | (1 << RW_PIN) | (1 << E_PIN);
	
	// R / W 핀으로 LOW를 출력하여 쓰기 전용으로 사용
	PORT_CONTROL &= ~(1 << RW_PIN);
	
	LCD_write_command(COMMAND_8_BIT_MODE); // 8비트 모드
	// display on/ off control
	// 화면 on, 커서 off, 커서 깜빡임 off
	uint8_t command = 0x08 | (1 << COMMAND_DISPLAY_ON_OFF_BIT);
	LCD_write_command(command);
	
	LCD_clear();		// 화면 지움
	
	//Entry Mode set
	// 출력 후 커서를 오른쪽을 옮김, 즉 DDRAM의 주소가 증가하며 화면 이동은 없음
	LCD_write_command(0x06);
}
void LCD_write_string(char * string){
	uint8_t i;
	for( i= 0; string[i]; i++)		// 종료 문자(NULL)를 만날 떄 까지
	LCD_write_data(string[i]);	// 문자 단위 출력
}

void LCD_goto_XY(uint8_t row, uint8_t col){
	col %= 16;			// 열 0 ~ 15
	row %= 2;			// 행 0 ~ 1
	
	// 첫쨰 라인 시작 주소는 0x00 둘쨰 라인 시작 주소는 0x40
	uint8_t address = (0x40 * row) + col;
	uint8_t command = 0x80 + address;
	
	LCD_write_command(command);		// 커서 이동
}
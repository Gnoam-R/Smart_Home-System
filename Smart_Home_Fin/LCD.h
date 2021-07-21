/*
 * LCD.h
 *
 * Created: 2021-04-08 오후 12:17:08
 *  Author: ComputerSystem
 */ 


#ifndef LCD_H_
#define LCD_H_

void LCD_pulse_enable(void);				// 상승 에지 하강에지 설정
void LCD_write_data(uint8_t data);			// 문자 출력 
void LCD_write_command(uint8_t command);	// 설정을 위한 명령어 입력 함수
void LCD_clear(void);						// LCD 초기화
void LCD_init(void);						// LCD 초기 설정
void LCD_write_string(char * string);		// LCD 문자열 출력
void LCD_goto_XY(uint8_t row, uint8_t col);	// LCD 커서 위치 

#endif /* LCD_H_ */

/*
* Smart_Home.c
*
스마트홈 제작용 프로젝트
UART통신 1번핀을 사용

2021-06-30 최종 기능
1. 타이머 카운터0(1초마다 기능 동작), 2번(BUZZER 사용), 3번(서브 모터사용)
2. 온습도 센서(DHT11)를 사용해서 현재 온도를 측정
3. 사람 인식(적외선 센서)와 현관문(서브모터 사용)
4. 릴레이 모듈와 화제 감지센서를 통한 화제 감지  & LCD에 출력(INT0번 사용)
6. UART1번을 사용해서 BLUETOOTH 통신을 함
7. LED MODE를 사용해서 MOOD MODE 변경
8. UART 통신을 사용해서 원격으로 여러가지 동작 제어


문제 1. 온습도 값을 올릴때 12V FAN이 도는데 습도값이 낮아져서 FAN이 멈출때 1에서 0으로 가는 신호가
MCU에 들어가서 interrupt1번핀에 인터럽트가 걸리게 된다. 그렇기 떄문에 LCD에 화제 경고 message가
계속 나타나게 된다 -> 해결 : 12fan쪽에 오실로스코프를 찍어서 확인했을 때 역전압이 발생하였고, 다이오드를 달아
신호를 차단했음.
문제 2. LCD Clear 딜레이 때문에 지속적으로 동작오류가 발생하여 수정함.
문제 3. 블루투스 통신이 시작되면 "OK+COMMAND"와 같은 contact message가 들어옴
-> UART.h에서 예외처리를 해서 부분적으로 해결함. 

* Created: 2021-07-15 오전 9:25:27
* Author : 노 형 우
*/
#define _CRT_SECURE_NO_WARNINGS
#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "UART.h"
//#include "adc.h"
#include "LCD.h"


// 파일 입출력
FILE OUTPUT = FDEV_SETUP_STREAM((void *) UART1_transmit, NULL, _FDEV_SETUP_WRITE);
/*-------------------------------------------------------------------------
					external definition
-------------------------------------------------------------------------*/
extern volatile int rx1flag;
extern char rx1data[BUFSIZE];      // BUFSIZE 는 100
/*-------------------------------------------------------------------------
					internal definition
-------------------------------------------------------------------------*/

typedef struct {
	int FIRE_WARNING_STATE;
	int INFRAY_SENSOR_STATE;
	int LCD_CLEAR_STATE;
	int FAN_STATE;
}Bool, *PBool;
//Sensor SENSOR;

#define BOOL_HDL(x) ((Bool *) x)

typedef void * Bool_handle; // 자료형 변형
Bool_handle Bool_init(){
	Bool_handle bool;
	bool = malloc(sizeof(Bool));
	return bool;
}


enum {NORMAL_MODE, WARNING_MODE, PRINT_MODE} M_State = NORMAL_MODE;
enum {LED_M1, LED_M2, LED_M3} LED_M_State = LED_M1;
enum {NORMAL, MOOD_MODE_ON, MOOD_MODE_OFF,DOOR_OPEN, DOOR_CLOSE, FAN_ON, FAN_OFF, LIV_LIGHT_ON, LIV_LIGHT_OFF, LED_PRINT1, LED_PRINT2, LED_PRINT3} LCD_MESSAGE_PRINT = NORMAL;

// RX COMMAND SIZE
#define CMD_LENGTH9 9
#define CMD_LENGTH6 6
#define CMD_LENGTH5 5
#define CMD_LENGTH4 4
#define CMD_LENGTH3 3
#define CMD_LENGTH2 2
#define CMD_LENGTH1 1
// LED def PORTA 0 ~ 7
#define MOOD_LED PORTA
#define MOOD_LED_INIT DDRA
// fire SENSOR PORTB 0
#define FIRE_SENSE PORTB
#define FIRE_SENSE_PIN PINB   // 0x01
// servo motor OUTPUT COMPARE REGISTER 3A
#define SERVO_PWM_VALUE OCR3A
// buzzer PWM 제어 포트 아직 미정
#define FIRE_WARNING_BUZZER DDRB
#define TIMER3_SERVO DDRE
// BUZZER def PORTB
#define ON 0
#define OFF 1
// RELAY MODULE
#define FIRE_RELAY_DDRF   DDRF
#define FIRE_RELAY_PORTF PORTF

#define FAN_CONTROL_DDRF DDRF
#define FAN_CONTROL_PORTF PORTF

#define sbi(PORTX,bitX) PORTX|=(1<<bitX)
#define cbi(PORTX,bitX) PORTX&=~(1<<bitX)
char DHT11_data = 0,hum_int,hum_dec,tem_int,tem_dec,parity;

// motor pwm
//#define MOTOR_START PA3

// volatile definition
volatile int BUZZER_FIRE_STATE;
volatile int TIMER0_count;      // 오버 플로우가 몇번 발생하는지
volatile int buzzer_state, buzzer_tone;
volatile int bit_mv;
volatile int OBSTACLE_DETECT_SENSOR = 0;
volatile int obstacle_count = 0;
volatile int password_success = 0;

volatile int lcd_time_state = 1;
volatile int BLE_LCD_CNT = 0;
volatile int delay_cnt = 0;
volatile int MoodLED_MODE =0;
/*----------------------------------------------------------------------
				main.c에서 사용하는 함수 모음
----------------------------------------------------------------------*/
void LCD_Start();
void LCD_PRINT_START();
void TIMER0_init();
void TIMER3_init_SERVO();

void DHT_request();
int DHT_response();
char DHT_receive_data();
void get_dht11();
void TEMP_HUMID_print();


ISR(INT0_vect){
	if((FIRE_SENSE_PIN & 0x01) == 0x01){      // 지금 오류가 생겨서 일단 넣어둠
		M_State = WARNING_MODE;
		LCD_PRINT_START();
		printf("start warning_mode\r\n");
	}
}
/*----------------------------------------------------------------------
				LCD 제어 및 스타트
----------------------------------------------------------------------*/
void LCD_Start(){
	LCD_init();      // 텍스트 LCD 초기화
	LCD_goto_XY(0, 2);
	LCD_write_string("Smart Home");   // 출력
	LCD_goto_XY(1, 4);
	LCD_write_string("start!!!");
	_delay_ms(2000);   // 2초 대기
	LCD_clear();
}



void BlueTooth_LCD_Print(){
	LCD_goto_XY(0, 0);
	LCD_write_string("Entered Message");
	LCD_goto_XY(1, 0);
	LCD_write_string("State:");
	switch(LCD_MESSAGE_PRINT){
		case NORMAL:
			break;
		case MOOD_MODE_ON:
			LCD_goto_XY(1, 6);
			LCD_write_string("MOOD_ON");
			break;
		case MOOD_MODE_OFF:
			LCD_goto_XY(1, 6);
			LCD_write_string("MOOD_OFF");
			break;
		case DOOR_OPEN:
			LCD_goto_XY(1, 6);
			LCD_write_string("DOOR_OPEN");
			break;
		case DOOR_CLOSE:
			LCD_goto_XY(1, 6);
			LCD_write_string("DOOR_CLOSE");
			break;
		case FAN_ON:
			LCD_goto_XY(1, 8);
			LCD_write_string("FAN_ON");
			break;
		case FAN_OFF:
			LCD_goto_XY(1, 8);
			LCD_write_string("FAN_OFF");
			break;
		case LIV_LIGHT_ON:
			LCD_goto_XY(1, 6);
			LCD_write_string("LIGHT_ON");
			break;
		case LIV_LIGHT_OFF:
			LCD_goto_XY(1, 6);
			LCD_write_string("LIGHT_OFF");
			break;
		case LED_PRINT1:
			LCD_goto_XY(1, 6);
			LCD_write_string("LED_M1");
			break;
		case LED_PRINT2:
			LCD_goto_XY(1, 6);
			LCD_write_string("LED_M2");
			break;
		case LED_PRINT3:
			LCD_goto_XY(1, 6);
			LCD_write_string("LED_M3");
			break;
	}
	
}
// 이거 미세먼지 센서로 수정하고, NORMAL_MODE 말고 미세먼지 센서 모드로 바꾸자
void LCD_PRINT_START(){
	
	static int prev_state = NORMAL_MODE;
	if(M_State != prev_state){
		LCD_clear();
	}
	if(M_State == NORMAL_MODE){
		TEMP_HUMID_print();
	}
	else if(M_State == PRINT_MODE){
		BlueTooth_LCD_Print();
	}
	else if(M_State == WARNING_MODE) {
		LCD_goto_XY(0, 0);
		LCD_write_string("THERE'S FIRE!!!");
		LCD_goto_XY(1, 0);
		LCD_write_string("PLEASE CALL 119");
	}
	prev_state = M_State;
}

/*---------------------------------------------------------------------
					타이머 카운터 0번, 2번, 3번 설정
----------------------------------------------------------------------*/

void MOOD_MODE_Start() {
	if(LED_M_State == LED_M1){
		MOOD_LED = (1 << bit_mv++);
		if(bit_mv >= 4) bit_mv = 0;
	}
	else if(LED_M_State == LED_M2){
		MOOD_LED = (8 >> bit_mv++);
		if(bit_mv >= 4) bit_mv = 0;
	}
	else if(LED_M_State == LED_M3){
		MOOD_LED = (1 << bit_mv) | (4 << bit_mv);
		if( bit_mv++ >= 1) bit_mv = 0;;
	}
}

ISR(TIMER0_OVF_vect){ // 인터럽트가 발생시 0.016초가 흐른다
	
	if((PINA & 0x10) == 0 && OBSTACLE_DETECT_SENSOR == 0){   // 이 센서가 너무 구려서 while 문에 넣어두면  측정이 어려운 양의 데이터를 받아 정확하지 못한 값을 읽음
		OBSTACLE_DETECT_SENSOR = 1;
	}
	TIMER0_count++;
	if(TIMER0_count >= 64){   // 1초가 흐를때
		LCD_PRINT_START();
		if(BLE_LCD_CNT){
			lcd_time_state = Delay_10();
		}
		if(MoodLED_MODE){
			MOOD_MODE_Start();
		}
		TIMER0_count = 0;
		if(password_success){   //5261메세지가 전송 성공했을경우
			obstacle_count++;
		}
	}
}

ISR(TIMER2_OVF_vect) {   //12.8ms
	if(BUZZER_FIRE_STATE){
		if(buzzer_state == OFF){
			PORTB |= 1 << 7; // 버저 포트ON
			buzzer_state = ON;
		}
		else{
			PORTB &= ~(1 << 7);   // 버저 포트 OFF
			buzzer_state = OFF;
		}
		TCNT2 = 50;
	}
	else {
		PORTB &= ~(1 << 7);   // 버저 포트 OFF
		buzzer_state = OFF;
	}
}
/*---------------------------------------------------------------------
						타이머 함수 선언
----------------------------------------------------------------------*/

void TIMER0_init(){      // 0.5초를 만들고싶어
	
	// 1 / 16000000hz = 0.0000000625s * 1024 = 0.000064  * 256(8비트) = 0.016
	TCCR0 |= (1 << CS02) | (1 << CS01) | (1 << CS00); //분주 1024
	TIMSK |= (1 << TOIE0);
	//sei();      // 전역인터럽트
}
void TIMER2_init_BUZZER(){ // 420hz
	FIRE_WARNING_BUZZER = 0x80;      // 버저 연결 포트(PORT B의 7번 비트) 출력으로 설정
	//TCCR2 = 0x03;      // 프리스케일러 8분주
	TCCR2 = 0x03;
	TIMSK |= (1 << TOIE2);
	TCNT2 = 0;      // TCNT0 초기화
}

void TIMER3_init_SERVO(){
	
	TCCR3A |= (1 << WGM31); // Fast PWM
	TCCR3B |= (1 << WGM32) | (1 << WGM33);
	
	// 비반전모드
	// TOP: ICR3, 비교 일치값: OCR3A 레지스터
	TCCR3A |= (1 << COM3A1);
	TCCR3B |= (1 << CS31); //분주비 8, 2mhz
	ICR3 = 39999;
	
	TIMER3_SERVO |= (1 << PORTE3);
	/*1 / 2000000 = 0.0000005(sec) * 1000  = 0.0005ms

	0.7ms = 1,400 overflow
	1.5ms = 3,000  overflow
	2ms = 4300 overflow
	*/
	
}
void INT0_init() {
	EICRA |= (1 << ISC01) | (1 << ISC00);
	EIMSK = 0x01;
}


/**************************************************************
				온습도 센서 TEST
**************************************************************/

void DHT_request(){
	// 핀 f를 출력으로 설정
	sbi(DDRF,7);      //output
	cbi(PORTF,7);      //low
	//MCU와 DHT11 사이에서 통신이 시작되면 PINF는 0으로 바뀜
	// 이과정은 최소 18ms가 소요되어야만 MCU의 신호를 받을수있음
	_delay_ms(20);      //more than 18ms
	sbi(PORTF,7);      //high
	// pull up & wait for sensor DHT_response
}

int DHT_response(){   // finish DHT_request
	// PINF 를 입력으로 바꿈
	cbi(DDRF,7);         //input
	_delay_us(39);         //pulled up & wait 20~40us
	if((PINF & (1<<7)))      //DHT_response signal check
	return 1;
	_delay_us(80);         //wait 80us
	if(!(PINF & (1<<7)))   //pulled ready output check
	return 1;
	_delay_us(80);         //wait 80us
	//high-level voltage가 26~28us이면 "0", "70us"이면 "1"을 의미한다.
}

char DHT_receive_data()      // 총 8비트
{
	
	for(int q=0; q<8; q++){
		//DHT_response에 따라서 데이터가 0인지 1인지를 채크한다
		while((PINF & (1<<7)) == 0); //50us signal wait
		_delay_us(30);
		if(PINF & (1<<7))         //greater than 30ms -> HIGH
		DHT11_data = (DHT11_data<<1)|(0x01);
		else                  //less than 30ms -> LOW
		DHT11_data = (DHT11_data<<1);
		while(PINF & (1<<7));
	}
	return DHT11_data;
}
void get_dht11(){
	
	DHT_request();                  //Host send a start signal
	if(DHT_response() != 1){         //DHT11 DHT_response
		hum_int = DHT_receive_data();   //Humidity integer
		hum_dec = DHT_receive_data();   //Humidity decimal
		tem_int = DHT_receive_data();   //Temporature integer
		tem_dec = DHT_receive_data();   //Temporature decimal
		parity = DHT_receive_data();   //parity
	}
}


void TEMP_HUMID_print(){
	char normal_lcd_arr[BUFSIZE] = {0,};
	get_dht11();
	
	if((hum_int + hum_dec + tem_int + tem_dec) != parity){
		LCD_write_string("DHT_value is ERROR");
	}
	else{
		sprintf(normal_lcd_arr, "TEMP is %d.%d%", tem_int, tem_dec);
		LCD_goto_XY(0, 0);
		LCD_write_string(normal_lcd_arr);   // 출력
		
		sprintf(normal_lcd_arr, "HUMID is %d.%d%", hum_int, hum_dec);
		LCD_goto_XY(1, 0);
		LCD_write_string(normal_lcd_arr);
	}
}
int Delay_10() {		// 약 10초를 기다리는 함수
	
	delay_cnt++;
	if(delay_cnt >= 10){
		delay_cnt = 0;
		return 0;
	}
	return 1;
}



int main(void)
{
	/* FILE OUTPUT*/
	stdout=&OUTPUT;
	/*  FUNCTION INIT*/
	MOOD_LED_INIT |= 0x0f;   //PORTA pinA OUTPUT
	MOOD_LED &= 0xf0;
	FIRE_RELAY_DDRF |= 0x01;      // RELAY MOUDULE DDRF OUTPUT
	
	// 내가 수정한 부분 2021-06-25
	FAN_CONTROL_DDRF |= 0x02;     // FAN MOUDULE DDRF OUTPUT
	FIRE_RELAY_PORTF &= ~(1 << PORTF1);
	//////////////////////////////////
	
	UART1_init();
	TIMER0_init();
	TIMER3_init_SERVO();
	TIMER2_init_BUZZER();
	INT0_init();
	
	Bool_handle bool;
	bool = Bool_init();
	
	BOOL_HDL(bool)->INFRAY_SENSOR_STATE = 0;
	/*String Data Convition*/
	char * str_div1 = (char *)malloc(sizeof(char) * BUFSIZE);
	char * str_div2 = (char *)malloc(sizeof(char) * BUFSIZE);
	memset((void *)str_div1,0x00,sizeof(str_div1));
	memset((void *)str_div2,0x00,sizeof(str_div2));
	
	/*화제 감지*/
	FIRE_SENSE = 0;      // B PORT RESET
	int fire_state = 0;
	
	/*main start*/
	LCD_Start();
	printf("/**********************************/\r\n");
	printf("   @@@@ Program is turning on @@@@\r\n");
	printf("/**********************************/\r\n");
	
	sei();         // global interrupt enable
	while (1)
	{
		/*----------------------------------------------------------------------------------------
						UART, BLUETOOTH Receive
						when the data is received using UART
						the rx1 flag is replaced by 1and entered into the condition statement
						we use PORTB1
		-----------------------------------------------------------------------------------------*/
		// RX1 interrupt fin
		
		if(BLE_LCD_CNT){
			M_State = PRINT_MODE;
			BLE_LCD_CNT = lcd_time_state;
			if(BLE_LCD_CNT == 0){
				M_State = NORMAL_MODE;
				lcd_time_state = 1;
				/*printf("nomal_mode come!!\r\n");*/
			} 
		}
		if(rx1flag){
			if(rx1data == 0)
			printf("memory is full\r\n");
			
			BLE_LCD_CNT = 1;
			str_div1 = strtok(rx1data, " ");
			str_div2 = strtok(NULL, " ");
			// if it's same as str_div1 and receive data, strncmp return 0
			if( !(strncmp(str_div1, "LIV_LIGHT", CMD_LENGTH9)) || !(strncmp(str_div1, "liv_light", CMD_LENGTH9)) ){
				// LIV_LIGHT ON
				if( !(strncmp(str_div2, "ON", CMD_LENGTH2)) || !(strncmp(str_div2, "on", CMD_LENGTH2))){
					LCD_MESSAGE_PRINT = LIV_LIGHT_ON;
					MOOD_LED |= 0x0F;
					printf("THE Mood Light in living room is on \r\n");
				}
				// LIV_LIGHT OFF
				else if(!(strncmp(str_div2, "OFF", CMD_LENGTH3)) || !(strncmp(str_div2, "off", CMD_LENGTH3)) ) {
					MOOD_LED &= ~(0x0F);
					LCD_MESSAGE_PRINT = LIV_LIGHT_OFF;
					printf("THE Mood Light in living room is off \r\n");
				}
			}
			else if( !(strncmp(str_div1, "FAN", CMD_LENGTH3)) || !(strncmp(str_div1, "FAN", CMD_LENGTH3)) ){
				if( !(strncmp(str_div2, "ON", CMD_LENGTH2)) || !(strncmp(str_div2, "on", CMD_LENGTH2))){
					printf("turning on the Air Conditioner\r\n");
					FAN_CONTROL_PORTF |= 0x02;
					LCD_MESSAGE_PRINT = FAN_ON;
					// Serial data를 lcd에 출력시킨다.
				}
				else if(!(strncmp(str_div2, "OFF",  CMD_LENGTH3)) || !(strncmp(str_div2, "off", CMD_LENGTH3)) ) {
					printf("turning off the Air Conditioner\r\n");
					LCD_MESSAGE_PRINT = FAN_OFF;
					FAN_CONTROL_PORTF &= ~(0x02);
				}
			}
			else if(!(strncmp(str_div1, "MOOD_MODE", CMD_LENGTH9)) || !(strncmp(str_div1, "mood_mode", CMD_LENGTH9)) ){
				// LIV_LIGHT ON
				if( !(strncmp(str_div2, "ON", CMD_LENGTH2)) || !(strncmp(str_div2, "on", CMD_LENGTH2))){
					MoodLED_MODE = 1;
					LCD_MESSAGE_PRINT = MOOD_MODE_ON;
					printf("MOOD_MODE start !!!\r\n");
				}
				// LIV_LIGHT OFF
				else if(!(strncmp(str_div2, "OFF", CMD_LENGTH3)) || !(strncmp(str_div2, "off", CMD_LENGTH3)) ) {
					MoodLED_MODE = 0;
					LCD_MESSAGE_PRINT = MOOD_MODE_OFF;
					printf("MOOD_MODE stop !!!\r\n");
					MOOD_LED = 0x00;
				}
			}
			else if( !strncmp(str_div1, "LED_M1", CMD_LENGTH6)){
				LED_M_State = LED_M1;
				LCD_MESSAGE_PRINT = LED_PRINT1;
				bit_mv = 0;
			}
			else if( !strncmp(str_div1, "LED_M2", CMD_LENGTH6)){
				LED_M_State = LED_M2;
				LCD_MESSAGE_PRINT = LED_PRINT2;
				bit_mv = 0;
			}
			else if( !strncmp(str_div1, "LED_M3", CMD_LENGTH6)){
				LED_M_State = LED_M3;
				LCD_MESSAGE_PRINT = LED_PRINT3;
				bit_mv = 0;
			}
			else if(!(strncmp(str_div1, "DOOR", CMD_LENGTH4)) || !(strncmp(str_div1, "door", CMD_LENGTH4)) ){
				// LIV_LIGHT ON
				if( !(strncmp(str_div2, "OPEN", CMD_LENGTH4)) || !(strncmp(str_div2, "open", CMD_LENGTH4))){
					SERVO_PWM_VALUE = 2500; // 문 열림 90도
					LCD_MESSAGE_PRINT = DOOR_OPEN;
					password_success = 1;
					printf("door open\r\n");
				}
				// LIV_LIGHT OFF
				else if(!(strncmp(str_div2, "close", CMD_LENGTH5)) || !(strncmp(str_div2, "close", CMD_LENGTH5)) ) {
					printf("door close\r\n");
					SERVO_PWM_VALUE = 0; // 문 닫힘
				}
			}
			else if( !strncmp(str_div1, "DOOR", CMD_LENGTH4) && password_success){
				if( !(strncmp(str_div2, "OPEN", CMD_LENGTH4)) || !(strncmp(str_div2, "open", CMD_LENGTH4))){
					printf("door is already opened\r\n");
				}
			}
			else {
				printf("you have entered an invalid word\r\n");
				printf("please re-enter\r\n");
			}
			LCD_clear();
			delay_cnt = 0;
			rx1flag = 0;
		}
		/*----------------------------------------------------------------------------------------
						ADC(fire sensor),  BUZZER, TX( BLUETOOTH) send
						fire detecting sensor
						Power: 3.3V ~ 5.3V
						this sensor return 1 when sensor looking like led close to fire
		
		-----------------------------------------------------------------------------------------*/
		
		if((FIRE_SENSE_PIN & 0x01) == 0x01){       // 화재 센서에서 0신호를 보내면 불남
			BOOL_HDL(bool)->FIRE_WARNING_STATE = 1;
			fire_state = 1;
		}
		else  {
			BOOL_HDL(bool)->FIRE_WARNING_STATE = 0;
			if(fire_state) {
				M_State = NORMAL_MODE;
				fire_state = 0;
				//FIRE_RELAY_PORTF = 0x00;
			}
		}
		
		if(BOOL_HDL(bool)->FIRE_WARNING_STATE){
			BUZZER_FIRE_STATE = 1;
			FIRE_RELAY_PORTF |= 0x01;
			BOOL_HDL(bool)->LCD_CLEAR_STATE = 1;
		}
		else if(BOOL_HDL(bool)->FIRE_WARNING_STATE == 0){
			FIRE_RELAY_PORTF &= ~(0x01);
			BUZZER_FIRE_STATE = 0;
			if(BOOL_HDL(bool)->LCD_CLEAR_STATE){
				LCD_clear();
				BOOL_HDL(bool)->LCD_CLEAR_STATE = 0;
			}
		}
		

		/*----------------------------------------------------------------------------------------
							INFRAY_SENSOR, front door control
		-----------------------------------------------------------------------------------------*/
		if(OBSTACLE_DETECT_SENSOR && (BOOL_HDL(bool)->INFRAY_SENSOR_STATE == 0)){
			printf("please input password\r\n");
			BOOL_HDL(bool)->INFRAY_SENSOR_STATE = 1;
		}
		if(obstacle_count >= 5 && BOOL_HDL(bool)->INFRAY_SENSOR_STATE){   // 5초 뒤에 닫힘 (30초 너무 길어서 수정)
			printf("door close\r\n");
			SERVO_PWM_VALUE = 0; // 문 닫힘
			//delay_cnt = 0;
			OBSTACLE_DETECT_SENSOR = 0;
			obstacle_count = 0;
			password_success = 0;
			BOOL_HDL(bool)->INFRAY_SENSOR_STATE = 0;
		}
		
		if(hum_int >= 60 && BOOL_HDL(bool)->FAN_STATE == 0){
			FAN_CONTROL_PORTF |= 0x02;
			BOOL_HDL(bool)->FAN_STATE = 1;
			printf("turning on the Air Conditioner\r\n");
		}
		else if(hum_int < 60 && BOOL_HDL(bool)->FAN_STATE){
			FAN_CONTROL_PORTF &= ~(0x02);
			BOOL_HDL(bool)->FAN_STATE = 0;
			printf("turning off the Air Conditioner\r\n"); 
		}
		
	}
	free(bool);
	free(str_div1);   // release malloc
	free(str_div2);
	
	return 0;
}

/*
 * uart.c
 *
 *  Created on: Mar 11, 2024
 *      Author: cowbo
 */
#include "uart.h"
#include <stdio.h>

UART_HandleTypeDef *myHuart;

#define rxBufferMax 255

int rxBufferGp;		//get pointer(read)
int rxBufferPp;		//put pointer(write)
uint8_t rxBuffer[rxBufferMax];
uint8_t rxChar;

//init device
void initUart(UART_HandleTypeDef *inHuart)
{
	myHuart = inHuart;
	HAL_UART_Receive_IT(myHuart, &rxChar, 1);//ring buffer
}

//process received charactor
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	rxBuffer[rxBufferPp++] = rxChar;
	rxBufferPp %= rxBufferMax;
	HAL_UART_Receive_IT(myHuart, &rxChar, 1);
}

//get charactor from buffer
int16_t getChar()//아스키 데이터 통신(binary data 통신 적합하지않음)
{
	int16_t result;
	if(rxBufferGp == rxBufferPp) return -1;
	result = rxBuffer[rxBufferGp++];
	rxBufferGp %= rxBufferMax;
	return result;
}

int _write(int file, char* p, int len)
{
	 HAL_UART_Transmit(myHuart, p, len, 10);
	 return len;
}

void transmitPacket(protocol_t data){
	//사전준비->버퍼만들기
	uint8_t txBuffer[] = {STX,0,0,0,0,ETX};
	txBuffer[1] = data.command;
	txBuffer[2] = (data.data >> 7) | 0x80;
	txBuffer[3] = (data.data & 0x7f) | 0x80;
	//CRC계산
	txBuffer[4] = txBuffer[1]+txBuffer[2]+txBuffer[3];
	//데이터전송
	HAL_UART_Transmit(myHuart, txBuffer, sizeof(txBuffer), 1);
	//데이터 전송 완료 대기
	while(HAL_UART_GetState(myHuart)==HAL_UART_STATE_BUSY_TX
			|| HAL_UART_GetState(myHuart)==HAL_UART_STATE_BUSY_TX_RX);//uart장치상태를 가져온 후 장치의 상태가 전송중이거나 전송과 수신을 동시에 하는 중이라면 이 상태가 끝날 때까지 기다림
}

//패킷 수신
protocol_t receivePacket(){
	protocol_t result;
	uint8_t		buffer[6];
	uint8_t		count = 0;
	uint32_t	timeout;

	int16_t	ch = getChar();
	memset(&result,0,sizeof(buffer));
	if(ch == STX){
		buffer[count++] = ch;
		timeout = HAL_GetTick();//gettick으로 일정시간이 지나면 종료하게 할수있음(타임아웃시작)
		while(ch!=ETX){
			ch = getChar();
			if(ch != -1/*데이터수신을 받았다*/){
				buffer[count++] = ch;
			}
			//타임아웃 계싼
			if(HAL_GetTick()-timeout>=2)return result;
		}
		//crc검사
		uint8_t crc=0;
		for(int i=0; i<4; i++)
			crc += buffer[i];
		if(crc != buffer[4]) return result;
		//수신완료후 파싱(parsing)
		result.command = buffer[1];
		result.data = buffer[3] & 0x7f;
		result.data |= (buffer[2] & 0x7f) << 7;
	}
	return result;
}

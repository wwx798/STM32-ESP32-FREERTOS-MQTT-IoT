#ifndef _MYIIC_H
#define _MYIIC_H
#include "main.h"
#define OLED_ADDRESS 0x78
#define IIC_PORT GPIOB
#define SCL GPIO_PIN_8
#define SDA GPIO_PIN_9
#define High GPIO_PIN_SET
#define Low GPIO_PIN_RESET
void IIC_Start();
void IIC_Stop();
void IIC_WriteBit(uint8_t value);
void IIC_Init();
uint8_t IIC_WriteByte(uint8_t Byte);
uint8_t IIC_ACK_Receive();
uint8_t IIC_ReadByte(uint8_t ACK);
uint8_t IIC_Write_Reg(uint8_t Slave_Address,uint8_t Reg_Address,uint8_t data);
uint8_t IIC_Read_Reg(uint8_t Slave_Address,uint8_t Reg_Address);
#endif
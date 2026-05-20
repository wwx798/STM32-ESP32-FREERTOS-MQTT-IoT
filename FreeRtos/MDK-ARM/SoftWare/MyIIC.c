#include "main.h"
#include "MyIIC.h"
static inline void delay_us(uint32_t us)
{
    volatile uint32_t count = us * 8;
    while(count--);
}

void IIC_Init()
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  HAL_GPIO_WritePin(IIC_PORT, SCL|SDA, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = SCL;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(IIC_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = SDA;
  HAL_GPIO_Init(IIC_PORT, &GPIO_InitStruct);
}
void IIC_Write_SCL(GPIO_PinState value)
{
HAL_GPIO_WritePin(IIC_PORT,SCL,value);
	//delay_us(50);
}
void IIC_Write_SDA(GPIO_PinState value)
{
HAL_GPIO_WritePin(IIC_PORT,SDA,value);
	//delay_us(50);
}

void IIC_Start()
{
		IIC_Write_SCL(High);
		IIC_Write_SDA(High);			//起始条件SCL高位时：SDA由高--》》低
		IIC_Write_SDA(Low);
		IIC_Write_SCL(Low);
		
	
}
void IIC_Stop()
{
		IIC_Write_SDA(Low);		//终止条件：SCL高位时： SDA由低————》》高
	
		IIC_Write_SCL(High);
		IIC_Write_SDA(High);
}

void IIC_WriteBit(uint8_t value){
	
	IIC_Write_SCL(Low);		//下拉SCL，SCL为高电平时不允许改变SDA
	IIC_Write_SDA(value);
	IIC_Write_SCL(High);
	IIC_Write_SCL(Low);
}
uint8_t IIC_GetACK_Receive()
{
	uint8_t Bit=0;
	IIC_Write_SCL(Low);
	IIC_Write_SDA(High);
	IIC_Write_SCL(High);
	if(!HAL_GPIO_ReadPin(IIC_PORT,SDA))
	Bit=1;
	IIC_Write_SCL(Low);//SDA低位为应答，高位非应答
	return Bit;//返回1为收到应答
}

uint8_t IIC_WriteByte(uint8_t Byte)
{
	for(int i=0;i<8;i++)
	{
	if(Byte&(0x80>>i))
	IIC_WriteBit(High);
	else
	IIC_WriteBit(Low);//IIC高位先行
	}
	return IIC_GetACK_Receive();
}
uint8_t IIC_ReadBit(){
	uint8_t Bit;
	IIC_Write_SCL(Low);  
	IIC_Write_SDA(High);
	IIC_Write_SCL(High);
	if(HAL_GPIO_ReadPin(IIC_PORT,SDA)) Bit=1 ;
    else	Bit=0;

	IIC_Write_SCL(Low);
	return Bit;
}
uint8_t IIC_ReadByte(uint8_t Flag)
{
    uint8_t Byte = 0;
    for(int i = 7; i >= 0; i--)
    {
        Byte |= (IIC_ReadBit() << i);
    }
    
    // Flag: 1 = 继续读 (发ACK, 低电平), 0 = 停止读 (发NACK, 高电平)
    if (Flag == 1)
        IIC_WriteBit(0);   // 发送 ACK
    else
        IIC_WriteBit(1);   // 发送 NACK
		
    return Byte;
}
uint8_t IIC_Write_Reg(uint8_t Slave_Address,uint8_t Reg_Address,uint8_t data)
{
	uint8_t flag=1;
	
	IIC_Start();
	flag=IIC_WriteByte(Slave_Address&0xFE);
	flag=flag && IIC_WriteByte(Reg_Address);
	flag=flag && IIC_WriteByte(data);
	IIC_Stop();
	return flag;
}
uint8_t IIC_Read_Reg(uint8_t Slave_Address,uint8_t Reg_Address)
{
	uint8_t Data=0;
	
	IIC_Start();
	IIC_WriteByte(Slave_Address&0xFE);
	IIC_WriteByte(Reg_Address);
	IIC_Start();
	IIC_WriteByte(Slave_Address|0x01);
	Data=IIC_ReadByte(0);
	IIC_Stop();
	return Data;
}




#include "gpio.h"
#include "oledfont.h"
#include "main.h"
#include "MyIIC.h"
/*引脚定义*/
#define OLED_W_SCL(x)		HAL_GPIO_WritePin(IIC_PORT, SCL, x)
#define OLED_W_SDA(x)		HAL_GPIO_WritePin(IIC_PORT, SDA, x)

/*I2C初始化*/
void OLED_I2C_Init(void)
{
   IIC_Init();
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C起始信号
  */
void OLED_I2C_Start(void)
{
	IIC_Start();
}

/**
  * @brief  I2C停止信号
  */
void OLED_I2C_Stop(void)
{
	IIC_Stop();
}

/**
  * @brief  I2C发送字节
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	IIC_WriteByte(Byte);
}

/**
  * @brief  OLED写命令
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		// 从机地址
	OLED_I2C_SendByte(0x00);		// 命令模式
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		// 从机地址
	OLED_I2C_SendByte(0x40);		// 数据模式
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  设置光标位置
  * @param  Y 页地址(0~7)
  * @param  X 列地址(0~127)
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					// 设置页地址
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	// 设置列高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			// 设置列低4位
}

/**
  * @brief  OLED清屏
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

/**
  * @brief  显示单个字符(8x16字体)
  * @param  Line   行号(1~4)
  * @param  Column 列号(1~16)
  * @param  Char   要显示的字符
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	// 上半部分
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);
	}
	// 下半部分
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
	}
}

/**
  * @brief  显示字符串
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  计算幂(内部函数)
  */
static uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  显示无符号整数
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  显示有符号整数
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  显示十六进制数
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  显示二进制数
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	// 上电延时
	for (i = 0; i < 1000; i++)
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			// I2C初始化
	
	OLED_WriteCommand(0xAE);	// 关闭显示
	
	OLED_WriteCommand(0xD5);	// 设置显示时钟分频/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	// 设置多路复用比
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	// 设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	// 设置显示起始行
	
	OLED_WriteCommand(0xA1);	// 段重映射,0xA1左右翻转
	
	OLED_WriteCommand(0xC8);	// COM扫描方向,0xC8上下翻转

	OLED_WriteCommand(0xDA);	// 设置COM引脚
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	// 设置对比度
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	// 设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	// 设置VCOMH电压
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	// 启用全局显示恢复

	OLED_WriteCommand(0xA6);	// 正常显示

	OLED_WriteCommand(0x8D);	// 启用电荷泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	// 开启显示
		
	OLED_Clear();				// OLED清屏
}
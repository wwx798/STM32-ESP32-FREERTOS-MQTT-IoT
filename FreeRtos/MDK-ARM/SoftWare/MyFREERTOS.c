#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>
#include "oled.h"
extern DMA_HandleTypeDef hdma_usart1_rx;
extern void SET_RGB(uint32_t R,uint32_t G,uint32_t B);
/* ==================== 数据类型定义 ==================== */
#define SENSOR_TYPE_TEMP   0
#define SENSOR_TYPE_HUMI   1
#define SENSOR_TYPE_LIGHT  2
#define ADC_NUM     3
#define RX_BUFFER_SIZE  256     // 接收缓冲区大小
typedef struct {
    uint8_t type;    // 数据来源标识
    float   value;   // 数值
} SensorData_t;
 uint16_t     adc_raw[ADC_NUM];  // 存放三个通道的原始值
uint8_t UART_RX[RX_BUFFER_SIZE];    // DMA接收缓冲区
uint8_t UART_RX_BYTE;
volatile uint8_t UART_RX_INDEX=0;
/* ==================== 信号量及队列 ==================== */

SemaphoreHandle_t UART1_Mutex;           // 串口互斥量
SemaphoreHandle_t OLED_Mutex;            // OLED互斥量
SemaphoreHandle_t ADC_Done_Sem;			 //ADC完成信号量
SemaphoreHandle_t CMD_OK_Sem;			 //命令接受完成信号量
QueueHandle_t     Sensor_Queue;          // 传感器原始数据队列
QueueHandle_t     Oled_Queue;            // OLED 显示数据队列
QueueHandle_t     Uart_Queue;            // 串口上报数据队列

/* ==================== 任务句柄 ==================== */
TaskHandle_t Sensor_TaskHandle      = NULL;
TaskHandle_t DataProcess_TaskHandle = NULL;
TaskHandle_t OLED_TaskHandle        = NULL;
TaskHandle_t UART_TaskHandle        = NULL;
TaskHandle_t CmdRunning_TaskHandle  = NULL;
/* ======================================== 中断函数 ======================================== */

/* ==================== ADC+DMA中断 ==================== */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
    if(hadc == &hadc1) {
        // 释放信号量，唤醒 Sensor_Task
        xSemaphoreGiveFromISR(ADC_Done_Sem, &xHigherPriorityTaskWoken);

        // 立即切换任务
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
	//OLED_ShowNum(3,1,adc_raw[1]++,3);
}


/* ==================== UART1接收中断 ==================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart==&huart1)
  {
	   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	  if(UART_RX_BYTE!='\n'&&(UART_RX_INDEX<RX_BUFFER_SIZE-1))
	  {
		  
			UART_RX[UART_RX_INDEX++]=UART_RX_BYTE;
	  }
	  else if(UART_RX_BYTE=='\n')
	  {
	  UART_RX[UART_RX_INDEX]='\0';
		  UART_RX_INDEX=0;
		  // 释放信号量，唤醒 CmdRunning_Task
        xSemaphoreGiveFromISR(CMD_OK_Sem, &xHigherPriorityTaskWoken);

        // 立即切换任务
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	  }
	  HAL_UART_Receive_IT(&huart1,&UART_RX_BYTE,1);// 使能串口接收中断
  }
}


/* ==================== Sensor_Task：采集三个ADC通道 ==================== */
void Sensor_Task(void *argument)
{
    SensorData_t data;
   HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw, 3);
	//---- DMA 模式：一次采集三个通道 ----
     
       
    for(;;)
  { 
	
	
     if(xSemaphoreTake(ADC_Done_Sem, portMAX_DELAY) == pdPASS)
	 {
        // ---- 发送温度 ----
        data.type  = SENSOR_TYPE_TEMP;
        data.value = adc_raw[0] * 3.3 / 4096.0 ;  // 0~50°C
        xQueueSend(Sensor_Queue, &data, 0);

        // ---- 发送湿度 ----
        data.type  = SENSOR_TYPE_HUMI;
        data.value = adc_raw[1] * 3.3 / 4096.0;   // 0~100%
        xQueueSend(Sensor_Queue, &data, 0);

        // ---- 发送光照 ----
        data.type  = SENSOR_TYPE_LIGHT;
        data.value = adc_raw[2] * 3.3/ 4096.0 ;   // 0~100%
        xQueueSend(Sensor_Queue, &data, 0);
	    vTaskDelay(2000);//数据更新间隔
		
    }
  }
}
/* ==================== CMDRunning_Task ==================== */
void CmdRunning_Task(void *argument)
{
    
    for(;;)
  { 
	
	
     if(xSemaphoreTake(CMD_OK_Sem, portMAX_DELAY) == pdPASS)
	 {
        // 解析命令
            if(strcmp((char*)UART_RX, "LED_ON") == 0)
            {
                SET_RGB(255, 255, 255);
                printf("LED ON\r\n");
            }
            else if(strcmp((char*)UART_RX, "LED_OFF") == 0)
            {
                SET_RGB(0, 0, 0);
                printf("LED OFF\r\n");
            }
            else if(strcmp((char*)UART_RX, "RGB_R") == 0)
            {
                SET_RGB(255, 0, 0);
            }
            else if(strcmp((char*)UART_RX, "RGB_G") == 0)
            {
                SET_RGB(0, 255, 0);
            }
            else if(strcmp((char*)UART_RX, "RGB_B") == 0)
            {
                SET_RGB(0, 0, 255);
            }
            else
            {
                printf("Unknown: %s\r\n", UART_RX);
            }
		
    }
  }
}

/* ==================== DataProcess_Task：分发数据 ==================== */
void DataProcess_Task(void *argument)
{
	
	SensorData_t data;

    for(;;) {
        // 阻塞等待传感器数据
        if(xQueueReceive(Sensor_Queue, &data, portMAX_DELAY) == pdPASS) {
            // 同时发给 OLED 和 UART
            xQueueSend(Oled_Queue, &data, 0);
            xQueueSend(Uart_Queue, &data, 0);
        }
    }
}

/* ==================== OLED_Task：显示数据 ==================== */
void OLED_Task(void *argument)
{
	
    SensorData_t data;

    for(;;) {
        if(xQueueReceive(Oled_Queue, &data, portMAX_DELAY) == pdPASS) {

            xSemaphoreTake(OLED_Mutex, portMAX_DELAY);

            // 根据数据类型更新不同行
            switch(data.type) {
                case SENSOR_TYPE_TEMP:
                    OLED_ShowString(1, 1, "ADC1:0.00V");
                    OLED_ShowNum(1, 6, (int)data.value, 1);
					OLED_ShowNum(1, 8, (int)(data.value*100)%100, 2);
                    break;

                case SENSOR_TYPE_HUMI:
                   OLED_ShowString(2, 1, "ADC2:0.00V");
                    OLED_ShowNum(2, 6, (int)data.value, 1);
					OLED_ShowNum(2, 8, (int)(data.value*100)%100, 2);
                    break;

                case SENSOR_TYPE_LIGHT:
                    OLED_ShowString(3, 1, "ADC3:0.00V");
                    OLED_ShowNum(3, 6, (int)data.value, 1);
					OLED_ShowNum(3, 8, (int)(data.value*100)%100, 2);
                    break;
            }
			//OLED_ShowString(1,1,"OLED WORK!");
			
            xSemaphoreGive(OLED_Mutex);
			
        }
    }
	
}

/* ==================== UART_Task：发送 JSON ==================== */
void UART_Task(void *argument)
{
	
		SensorData_t data;
    char         json_buf[128];
	
	
	
    static float temp  = 0;
    static float humi  = 0;
    static float light = 0;  // 保存各传感器最新值

    for(;;) {
        if(xQueueReceive(Uart_Queue, &data, portMAX_DELAY) == pdPASS) {

            // 更新对应类型的最新值
            switch(data.type) {
                case SENSOR_TYPE_TEMP:  temp  = data.value; break;
                case SENSOR_TYPE_HUMI:  humi  = data.value; break;
                case SENSOR_TYPE_LIGHT: light = data.value; break;
            }

            // 组装完整 JSON，末尾加 \n 方便 ESP32 判断帧结束
            snprintf(json_buf, sizeof(json_buf),
                     "{\"temp\":%.1f,\"humi\":%.1f,\"light\":%.1f}\n",
                     temp, humi, light);

            xSemaphoreTake(UART1_Mutex, portMAX_DELAY);
            printf("%s", json_buf);
            xSemaphoreGive(UART1_Mutex);
        }
    }
}



/* ==================== Init ==================== */
void FREEROOS_Init(void)
{
    OLED_Init();

    // ---- 创建互斥锁 /信号量----
    UART1_Mutex = xSemaphoreCreateMutex();
    OLED_Mutex  = xSemaphoreCreateMutex();
	ADC_Done_Sem =xSemaphoreCreateBinary();
	CMD_OK_Sem =xSemaphoreCreateBinary();
    // ---- 创建队列 ----
    Sensor_Queue = xQueueCreate(10, sizeof(SensorData_t));  // 10 条缓冲
    Oled_Queue   = xQueueCreate(5,  sizeof(SensorData_t));
    Uart_Queue   = xQueueCreate(5,  sizeof(SensorData_t));

    // ---- 创建任务（栈大小加大，确保不溢出）----
	BaseType_t res;
	res=xTaskCreate((TaskFunction_t)Sensor_Task,      "Sensor",   256, NULL, 2, &Sensor_TaskHandle);
	if(res==pdPASS)
	printf("Sensor_TASK Created\r\n");
    res=xTaskCreate((TaskFunction_t)DataProcess_Task, "DataProc", 256, NULL, 3, &DataProcess_TaskHandle);
	if(res==pdPASS)
	printf("DataProcess_Task Created\r\n");
    res=xTaskCreate((TaskFunction_t)OLED_Task,        "OLED",     256, NULL, 2, &OLED_TaskHandle);
	if(res==pdPASS)
	printf("OLED_Task Created\r\n");
    res=xTaskCreate((TaskFunction_t)UART_Task,        "UART",     256, NULL, 3, &UART_TaskHandle);
	if(res==pdPASS)
	printf("UART_Task Created\r\n");
	res=xTaskCreate((TaskFunction_t)CmdRunning_Task,        "CmdRun",     128, NULL, 4, &CmdRunning_TaskHandle);
	if(res==pdPASS)
	printf("CmdRunning_Task Created\r\n");
	

}
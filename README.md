# 基于 FreeRTOS 与 MQTT 的双机物联网系统

## 项目简介

本项目实现了一套完整的物联网原型系统，采用 **STM32F103C8T6 + ESP32 双芯片架构**。

- **STM32** 作为采集与控制终端，运行 FreeRTOS 实时操作系统，负责多通道 ADC 传感器数据采集、OLED 显示及外设控制。
- **ESP32** 作为无线通信网关，通过 MQTT 协议连接云端，实现数据上报与远程指令下发。

## 系统架构
传感器组 ──ADC──> STM32F103 ──UART(JSON)──> ESP32 ──WiFi──> MQTT Broker ──> 手机/PC
│ │ │
│ OLED显示 下发控制指令
│ │ │
└───────────────┴── RGB灯 / 舵机等外设 <───────────────────┘

## 功能特性

- **多任务数据采集**：STM32 基于 FreeRTOS 创建 5 个任务，使用消息队列、信号量、互斥锁完成任务间同步与通信
- **高效 ADC 采集**：多通道 ADC + DMA 循环模式，配合中断回调与二进制信号量实现数据同步
- **双机通信**：自定义 JSON 协议通过 UART 与 ESP32 通信，支持双向数据传输
- **MQTT 上云**：ESP32 通过 WiFi 连接公共 MQTT Broker，上报传感器数据并接收云端指令
- **远程控制**：云端可通过 MQTT 下发指令控制 STM32 端 RGB 灯颜色
- **断线重连**：ESP32 端支持 WiFi / MQTT 断开后自动重连

## 硬件配置

| 器件 | 型号 |
|:---|:---|
| 主控 | STM32F103C8T6 |
| 无线模块 | ESP32 |
| 显示 | 0.96寸 OLED（I2C） |
| 传感器 | 电位器模拟 ADC 输入（可替换为实际传感器） |
| 执行器 | RGB LED |

## 软件架构

### STM32 端（FreeRTOS）

| 任务名 | 优先级 | 功能 |
|:---|:---|:---|
| Sensor_Task | 2 | DMA + ADC 多通道采集，发送到数据队列 |
| DataProcess_Task | 3 | 接收传感器数据，分发给 OLED 和 UART 队列 |
| OLED_Task | 2 | 从 OLED 队列取数据，刷新屏幕显示 |
| UART_Task | 3 | 从 UART 队列取数据，封装 JSON 发送至 ESP32 |
| CmdRunning_Task | 4 | 等待命令信号量，解析并执行控制指令 |

### ESP32 端（Arduino）

- 接收 STM32 发来的 JSON 数据，通过 MQTT 发布到 `stm32/sensor`
- 订阅 `stm32/ctrl` 主题，收到指令后通过 UART 转发给 STM32
- WiFi / MQTT 断线自动重连

## 通信协议

### STM32 → ESP32（数据上报）

```json
{"temp":1.65,"humi":2.10,"light":3.00}

### 开发环境

STM32CubeMX + Keil MDK

Arduino IDE（ESP32）

MQTT Broker：broker.emqx.io（公共测试服）

调试工具：串口助手、MQTTX / Mqtizer
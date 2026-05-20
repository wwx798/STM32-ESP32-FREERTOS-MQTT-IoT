/**
 * @file ESP32_MQTT_Gateway.ino
 * @brief ESP32作为STM32和MQTT云平台之间的网关
 * @details 功能：
 *          1. 接收STM32发送的JSON格式传感器数据（温度、湿度、光照）
 *          2. 提取并记录传感器数据到本地缓冲区
 *          3. 原样转发JSON数据到MQTT云端
 *          4. 接收云端指令，提取CMD字段并转发给STM32
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ================== 网络配置区 ==================
const char* ssid     = "ZhuNiu";      // WiFi名称
const char* password = "20050826";    // WiFi密码

// MQTT服务器配置
const char* mqtt_server = "broker.emqx.io"; // MQTT Broker地址（公共测试服务器）
const int   mqtt_port   = 1883;              // MQTT端口（1883为非加密端口）
const char* topic_report = "stm32/sensor";   // 发布主题：ESP32将STM32数据发布到此主题
const char* topic_ctrl   = "stm32/ctrl";     // 订阅主题：接收云端控制指令

// ================== 硬件串口配置 ==================
// ESP32与STM32通信使用UART2
#define STM32_SERIAL  Serial2    // 使用Serial2
#define STM32_BAUD    115200     // 波特率必须与STM32一致
// ESP32 UART2默认引脚：RX=GPIO16, TX=GPIO17
// 如果需要更改引脚，可以在begin函数中指定

// ================== 数据记录配置 ==================
#define MAX_DATA_LOG 20  // 最多保存20条传感器数据记录

/**
 * @brief 传感器数据结构体
 * @details 用于在ESP32内存中缓存STM32发送的传感器数据
 */
struct SensorDataLog {
    unsigned long timestamp;    // 数据接收时间戳（毫秒）
    float temperature;          // 温度值（摄氏度）
    float humidity;             // 湿度值（百分比）
    float light;                // 光照强度（勒克斯）
    String rawJson;             // 原始JSON字符串
};

SensorDataLog dataLog[MAX_DATA_LOG];  // 循环缓冲区
int logIndex = 0;   // 当前写入位置索引
int logCount = 0;   // 已记录的数据总数（最多MAX_DATA_LOG）

// ================== MQTT客户端对象 ==================
WiFiClient wifiClient;           // WiFi客户端
PubSubClient mqttClient(wifiClient);  // MQTT客户端

/**
 * @brief 提取并记录STM32传感器数据
 * @param jsonData STM32发送的原始JSON字符串
 * @return bool true-提取成功 false-提取失败
 * @details 解析STM32发来的JSON格式：{"temp":25.5,"humi":65.0,"light":300.5}
 */
bool extractAndRecordData(const String& jsonData)
{
    // 创建JSON文档对象，容量256字节足够存储传感器数据
    StaticJsonDocument<256> doc;
    
    // 解析JSON字符串
    DeserializationError err = deserializeJson(doc, jsonData);
    
    // 检查解析是否出错
    if (err) {
        Serial.print("[Data Extract] Failed to parse JSON: ");
        Serial.println(err.c_str());
        return false;
    }
    
    // 提取温湿度光照数据
    // 使用 | 运算符提供默认值-999.0（表示数据无效）
    float temp = doc["temp"] | -999.0;   // 温度：支持小数点后1位
    float humi = doc["humi"] | -999.0;   // 湿度：支持小数点后1位
    float light = doc["light"] | -999.0; // 光照：支持小数点后1位
    
    // 验证是否至少有一个有效数据
    if (temp == -999.0 && humi == -999.0 && light == -999.0) {
        Serial.println("[Data Extract] No valid sensor data found in JSON");
        return false;
    }
    
    // 将数据保存到循环缓冲区
    dataLog[logIndex].timestamp = millis();      // 记录接收时间
    dataLog[logIndex].temperature = temp;        // 记录温度
    dataLog[logIndex].humidity = humi;           // 记录湿度
    dataLog[logIndex].light = light;             // 记录光照
    dataLog[logIndex].rawJson = jsonData;        // 保留原始JSON
    
    // 更新缓冲区索引（循环写入）
    logIndex = (logIndex + 1) % MAX_DATA_LOG;
    // 更新记录总数（不超过最大容量）
    if (logCount < MAX_DATA_LOG) logCount++;
    
    // 串口打印提取的数据（便于调试）
    Serial.println("========== Sensor Data Extracted ==========");
    Serial.printf("Timestamp: %lu ms\n", millis());
    if (temp != -999.0) Serial.printf("Temperature: %.1f °C\n", temp);
    if (humi != -999.0) Serial.printf("Humidity: %.1f %%\n", humi);
    if (light != -999.0) Serial.printf("Light: %.1f lux\n", light);
    Serial.printf("Raw JSON: %s\n", jsonData.c_str());
    Serial.println("===========================================");
    
    return true;
}

/**
 * @brief 打印最近的传感器数据记录
 * @param num 要显示的记录数量（默认5条）
 */
void printRecentDataLog(int num = 5)
{
    // 如果没有数据，直接返回
    if (logCount == 0) {
        Serial.println("No data logged yet");
        return;
    }
    
    Serial.println("========== Recent Sensor Data Log ==========");
    
    // 计算起始索引（显示最近的num条记录）
    int startIndex = (logIndex - min(num, logCount) + MAX_DATA_LOG) % MAX_DATA_LOG;
    
    // 循环打印每条记录
    for (int i = 0; i < min(num, logCount); i++) {
        int idx = (startIndex + i) % MAX_DATA_LOG;
        Serial.printf("[%d] Time: %lu ms | Temp: %.1f | Humi: %.1f | Light: %.1f\n", 
                     i+1, 
                     dataLog[idx].timestamp, 
                     dataLog[idx].temperature, 
                     dataLog[idx].humidity,
                     dataLog[idx].light);
    }
    Serial.println("============================================");
}

/**
 * @brief MQTT消息回调函数
 * @details 当ESP32订阅的主题收到消息时，此函数会被自动调用
 *          功能：解析云端JSON，提取CMD字段，转发给STM32
 * 
 * 云端下发的JSON格式示例：
 * {"CMD":"FAN_ON"}        -> 发送 "FAN_ON" 给STM32
 * {"CMD":"LED_RED"}       -> 发送 "LED_RED" 给STM32
 * {"CMD":"SET_PWM 128"}   -> 发送 "SET_PWM 128" 给STM32
 */
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    // 将payload（字节数组）转换为字符串
    char jsonBuf[256];
    
    // 检查数据长度是否超过缓冲区大小
    if (length >= sizeof(jsonBuf)) {
        Serial.println("[Cloud -> ESP32] MQTT payload too large, dropped.");
        return;
    }
    
    // 复制数据并添加字符串结束符
    memcpy(jsonBuf, payload, length);
    jsonBuf[length] = '\0';  // 添加字符串结束符

    // 打印接收到的云端消息
    Serial.print("[Cloud -> ESP32] Topic: ");
    Serial.print(topic);
    Serial.print(" | JSON: ");
    Serial.println(jsonBuf);

    // 创建JSON文档对象并解析
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, jsonBuf);
    
    // 检查JSON解析是否成功
    if (err) {
        Serial.print("[JSON Parse Error] ");
        Serial.println(err.c_str());
        return;  // 解析失败，直接返回，不发送给STM32
    }
    
    // 提取"CMD"字段
    const char* cmd = doc["CMD"];
    
    if (cmd) {
        // 将CMD内容发送给STM32（添加换行符作为数据帧结束标志）
        STM32_SERIAL.print(cmd);
        STM32_SERIAL.write('\n');  // 发送换行符，STM32可使用readStringUntil读取
        
        // 打印调试信息
        Serial.print("[ESP32 -> STM32] Extracted CMD: ");
        Serial.println(cmd);
    } else {
        // 如果JSON中没有CMD字段，输出警告
        Serial.println("[Warning] No 'CMD' field found in cloud message");
        Serial.println("Expected format: {\"CMD\":\"your_command\"}");
    }
}

/**
 * @brief WiFi连接初始化函数
 * @details 连接指定的WiFi网络，如果连接失败则重启ESP32
 */
void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // 设置为Station模式
    WiFi.mode(WIFI_STA);
    // 开始连接WiFi
    WiFi.begin(ssid, password);
    
    // 等待WiFi连接，超时时间15秒
    unsigned int start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        
        // 超时判断
        if (millis() - start >= 15000) {
            Serial.println("\nWiFi Connect Error! Restarting...");
            ESP.restart();  // 重启ESP32
        }
    }

    // 连接成功，打印IP地址
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

/**
 * @brief MQTT断线重连函数
 * @details 检查MQTT连接状态，如果断开则尝试重新连接
 */
void reconnectMQTT()
{
    // 循环直到连接成功
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        
        // 生成随机客户端ID（避免多个设备冲突）
        String clientId = "ESP32-" + String(random(0xFFFF), HEX);
        
        // 尝试连接MQTT服务器
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println("connected");
            
            // 订阅控制主题，接收云端指令
            mqttClient.subscribe(topic_ctrl);
            Serial.print("Subscribed to: ");
            Serial.println(topic_ctrl);
        }
        else
        {
            // 连接失败，打印错误码并等待5秒后重试
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println("; retry in 5s");
            delay(5000);
        }
    }
}

/**
 * @brief Arduino初始化函数
 * @details 在ESP32启动时执行一次
 */
void setup() {
    // 初始化调试串口（用于连接电脑查看日志）
    Serial.begin(115200);
    
    // 等待串口就绪（最多等待3秒）
    unsigned long start = millis();
    while (!Serial) {
        if (millis() - start > 3000) {
            break;  // 超时退出，避免卡死
        }
    }

    // 初始化与STM32的串口通信
    // 参数：波特率, 数据格式, RX引脚, TX引脚
    STM32_SERIAL.begin(STM32_BAUD, SERIAL_8N1, 16, 17);
    Serial.println("UART2 initialized for STM32 communication");
    Serial.println("Expected STM32 JSON format: {\"temp\":xx.x,\"humi\":xx.x,\"light\":xx.x}");

    // 连接WiFi
    setupWiFi();
    
    // 配置MQTT服务器和回调函数
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);  // 设置消息接收回调函数
    
    Serial.println("ESP32 ready. Waiting for STM32 JSON data...");
    
    // 初始化数据日志缓冲区（设置默认值）
    for (int i = 0; i < MAX_DATA_LOG; i++) {
        dataLog[i].temperature = -999.0;
        dataLog[i].humidity = -999.0;
        dataLog[i].light = -999.0;
        dataLog[i].timestamp = 0;
        dataLog[i].rawJson = "";
    }
}

/**
 * @brief Arduino主循环函数
 * @details ESP32启动后会不断循环执行此函数
 */
void loop() {
    // 检查MQTT连接状态，如果断开则重连
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    // 处理MQTT消息（检查是否有新消息并调用回调函数）
    mqttClient.loop();
     // 检查STM32是否有数据发送过来
    if (STM32_SERIAL.available()) {
        // 读取一行数据（以换行符为结束标志）
        String jsonData = STM32_SERIAL.readStringUntil('\n');
        jsonData.trim();  // 去除首尾空白字符

        // 确保数据不为空
        if (jsonData.length() > 0) {
            // 打印原始接收到的数据
            Serial.print("[STM32 -> ESP32] Raw JSON: ");
            Serial.println(jsonData);
            
            // 步骤1：提取并记录传感器数据到本地缓冲区
            if (extractAndRecordData(jsonData)) {
                // 步骤2：原样转发JSON到云端MQTT Broker
                bool published = mqttClient.publish(topic_report, jsonData.c_str());
                
                // 打印发布结果
                if (published) {
                    Serial.println("[ESP32 -> Cloud] JSON forwarded successfully");
                } else {
                    Serial.println("[ESP32 -> Cloud] Failed to publish JSON");
                }
            } else {
                Serial.println("[ESP32] Failed to extract data, but still forwarding to cloud");
                // 即使提取失败也尝试转发原始数据
                mqttClient.publish(topic_report, jsonData.c_str());
            }
            
          
        }
    }
   
}
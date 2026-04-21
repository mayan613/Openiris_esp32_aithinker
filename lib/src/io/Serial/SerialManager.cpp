#include "SerialManager.hpp"
#include <functional>
// 修改了，加入web日志功能相关的代码

SerialManager::SerialManager(CommandManager *commandManager)
    : commandManager(commandManager) {}

// ==================== 新增：设置日志回调函数 ====================
void SerialManager::setLogCallback(std::function<void(const String &)> callback)
{
  logCallback = callback;
}

#ifdef ETVR_EYE_TRACKER_USB_API
void SerialManager::send_frame()
{
  if (!last_frame)
    last_frame = esp_timer_get_time();

  uint8_t len_bytes[2];

  size_t len = 0;
  uint8_t *buf = NULL;

  auto fb = esp_camera_fb_get();
  if (fb)
  {
    len = fb->len;
    buf = fb->buf;
  }
  else
    err = ESP_FAIL;

  // if we failed to capture the frame, we bail, but we still want to listen to
  // commands
  if (err != ESP_OK)
  {
    Serial.printf("Camera capture failed with response: %s", esp_err_to_name(err));
    return;
  }

  Serial.write(ETVR_HEADER, 2);
  Serial.write(ETVR_HEADER_FRAME, 2);
  len_bytes[0] = len & 0xFF;
  len_bytes[1] = (len >> CHAR_BIT) & 0xFF;
  Serial.write(len_bytes, 2);
  Serial.write((const char *)buf, len);

  if (fb)
  {
    esp_camera_fb_return(fb);
    fb = NULL;
    buf = NULL;
  }
  else if (buf)
  {
    free(buf);
    buf = NULL;
  }

  long request_end = millis();
  long latency = request_end - last_request_time;
  last_request_time = request_end;
  Serial.printf("大小: %uKB, 时间: %ums (%ifps)\n", len / 1024, latency,
        1000 / latency);
}
#endif

void SerialManager::init()
{
#ifdef SERIAL_MANAGER_USE_HIGHER_FREQUENCY
  Serial.begin(3000000);
#endif
  if (SERIAL_FLUSH_ENABLED)
  {
    Serial.flush();
  }
}
void SerialManager::sendQuery(QueryAction action,
                              QueryStatus status,
                              std::string additional_info)
{
}

void SerialManager::run()
{
  // 调试：每次调用都输出
  static unsigned long lastRunDebug = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastRunDebug > 5000) {  // 每5秒输出一次
    lastRunDebug = currentTime;
    Serial.printf("[SerialManager::run] 被调用，Serial.available()=%d\n", Serial.available());
  }

  // ====================== 日志转发到 Web 服务器 ======================
  static String logBuffer = "";

  while (Serial.available()) //这里有点问题，我在研究，不知道为什么一直为0，导致这个循环体内的代码永远不执行，我真没招了 QAQ
  {
    char c = Serial.read();
    logBuffer += c;

    // 遇到换行符或者缓冲区太长时发送一次
    if (c == '\n' || logBuffer.length() > 1024)
    {
      Serial.printf("\n[SerialManager] 读到数据，长度=%d，缓冲区前50字: %.50s\n", logBuffer.length(), logBuffer.c_str());
      
      // 尝试解析为JSON命令
      JsonDocument doc;
      DeserializationError deserializationError = deserializeJson(doc, logBuffer);

      if (!deserializationError)
      {
        // 是JSON命令，处理它
        Serial.printf("[SerialManager] 识别为JSON命令\n");
        CommandsPayload commands = {doc};
        this->commandManager->handleCommands(commands);
      }
      else
      {
        // 不是JSON，当作日志转发
        Serial.printf("[SerialManager] 非JSON，尝试转发日志。logCallback=%s\n", logCallback ? "有效" : "空");
        if (logCallback)
        {
          Serial.printf("[SerialManager] 调用回调函数\n");
          logCallback(logBuffer);
          Serial.printf("[SerialManager] 回调函数已调用\n");
        }
        else
        {
          Serial.printf("[SerialManager] WARNING: logCallback为空！\n");
        }
      }
      logBuffer = "";
    }
  }

#ifdef ETVR_EYE_TRACKER_USB_API
  // 如果没有串口数据，发送帧
  if (!Serial.available())
  {
    this->send_frame();
  }
#endif
}

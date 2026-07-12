#pragma once
#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include <Arduino.h>
#include <functional>
#include <esp_log.h>

// ================================================
//  统一日志管理器 — GLogManager
//  一次调用 → 串口 + Web SSE 双输出
//  替代原来分散的 Serial.printf / log_i / SendLogToWeb 三套体系
// ================================================

enum class GLogLevel {
  DEBUG,
  INFO,
  WARN,
  ERROR
};

class GLogManager {
 public:
  static GLogManager& instance();

  // 设置全局最低日志级别——低于此级别的日志不会被输出
  // DEBUG（全部输出）> INFO > WARN > ERROR（仅输出错误）
  void setLevel(GLogLevel level);
  GLogLevel getLevel() const;

  // 设置 Web SSE 推送回调（main.cpp 中 setupWebLogServer 时调用）
  void setWebCallback(std::function<void(const String&)> callback);

  // 清除 Web 回调（OTA 等场景下暂停推送）
  void clearWebCallback();

  // 核心输出：参数 = (级别, 标签, printf 格式串, ...)
  void log(GLogLevel level, const char* tag, const char* fmt, ...)
      __attribute__((format(printf, 4, 5)));

  // 发送纯字符串（已有的 String，不经过格式化）
  void send(GLogLevel level, const char* tag, const String& msg);

  // Web SSE 是否已就绪（由 main.cpp 设置）
  bool webReady = false;

 private:
  GLogManager() = default;
  std::function<void(const String&)> _webCallback;
  GLogLevel _minLevel = GLogLevel::DEBUG;  // 默认输出所有级别

  static const char* levelPrefix(GLogLevel level);
  static const char* ansiColor(GLogLevel level);
  const char* ansiReset();
};

// ================================================
//  便捷宏 — 替换原来的 log_i / log_d / log_e / Serial.printf / SendLogToWeb
// ================================================

// 带标签的日志（推荐用于 lib/ 中各模块）
#define GLOG_D(tag, fmt, ...) \
  GLogManager::instance().log(GLogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)
#define GLOG_I(tag, fmt, ...) \
  GLogManager::instance().log(GLogLevel::INFO,  tag, fmt, ##__VA_ARGS__)
#define GLOG_W(tag, fmt, ...) \
  GLogManager::instance().log(GLogLevel::WARN,  tag, fmt, ##__VA_ARGS__)
#define GLOG_E(tag, fmt, ...) \
  GLogManager::instance().log(GLogLevel::ERROR, tag, fmt, ##__VA_ARGS__)

// 便捷宏（不需要 tag，默认 "MAIN"）— 用于 main.cpp 等场景
#define GLOG(fmt, ...) \
  GLogManager::instance().log(GLogLevel::INFO, "MAIN", fmt, ##__VA_ARGS__)

#endif  // LOG_MANAGER_HPP

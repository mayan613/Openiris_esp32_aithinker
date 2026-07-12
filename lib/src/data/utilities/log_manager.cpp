#include "log_manager.hpp"

// ================================================
//  单例访问
// ================================================

GLogManager& GLogManager::instance() {
  static GLogManager mgr;
  return mgr;
}

// ================================================
//  Web 回调管理
// ================================================

void GLogManager::setWebCallback(std::function<void(const String&)> callback) {
  _webCallback = callback;
}

void GLogManager::clearWebCallback() {
  _webCallback = nullptr;
}

// ================================================
//  日志级别过滤
// ================================================

void GLogManager::setLevel(GLogLevel level) {
  _minLevel = level;
}

GLogLevel GLogManager::getLevel() const {
  return _minLevel;
}

// ================================================
//  辅助：ANSI 颜色（串口带颜色输出）
//  注意：只有串口输出带颜色，Web 端有自己的 CSS 颜色方案
// ================================================

const char* GLogManager::ansiColor(GLogLevel level) {
  switch (level) {
    case GLogLevel::DEBUG: return "\033[36m";  // 青色
    case GLogLevel::INFO:  return "\033[32m";  // 绿色
    case GLogLevel::WARN:  return "\033[33m";  // 黄色
    case GLogLevel::ERROR: return "\033[31m";  // 红色
    default:               return "\033[0m";
  }
}

const char* GLogManager::ansiReset() {
  return "\033[0m";
}

const char* GLogManager::levelPrefix(GLogLevel level) {
  switch (level) {
    case GLogLevel::DEBUG: return "调试";
    case GLogLevel::INFO:  return "信息";
    case GLogLevel::WARN:  return "警告";
    case GLogLevel::ERROR: return "错误";
    default:               return "未知";
  }
}

// ================================================
//  核心输出：格式化 → 串口 + Web
// ================================================

void GLogManager::log(GLogLevel level, const char* tag, const char* fmt, ...) {
  // 1. vsnprintf 格式化
  char buf[512];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len <= 0) return;

  String msg = String(buf);
  send(level, tag, msg);
}

void GLogManager::send(GLogLevel level, const char* tag, const String& msg) {
  // 0. 级别过滤——低于 _minLevel 的日志直接丢弃
  if (level < _minLevel) return;

  // 2a. → 串口（带 ANSI 颜色）
  // 格式: \[信息][WIFI] Connected to xxx
  Serial.printf("%s[%s][%s] %s%s\n",
                ansiColor(level), levelPrefix(level), tag,
                msg.c_str(), ansiReset());

  // 2b. → Web SSE（如果就绪）
  if (webReady && _webCallback) {
    // Web 端格式更紧凑，日志级别前缀方便 CSS 着色
    String webMsg = "[" + String(levelPrefix(level)) + "][" + String(tag) + "] " + msg;
    _webCallback(webMsg);
  }
}

#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

// ================================================
//  OpenIris 配网页面 - 美化版
//  作者：Galeros Studio
// ================================================

// 这个文件用于存放HTML内容，主要是配网相关的HTML页面内容，用户可以根据需要修改这些HTML内容来定制自己的配网页面

// 目前包含两个HTML页面：一个是AP模式下的配网页面，另一个是STA模式下的管理页面（显示当前连接的网络和修改按钮）
// AP 模式配网页面
const char ap_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenIris 首次配网</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #333;
        }
        .container {
            background: white;
            padding: 40px 30px;
            border-radius: 16px;
            box-shadow: 0 15px 35px rgba(0,0,0,0.1);
            max-width: 420px;
            width: 90%;
            text-align: center;
        }
        h1 {
            color: #4a6cf7;
            margin-bottom: 8px;
            font-size: 28px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 15px;
        }
        .form-group {
            margin-bottom: 20px;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 6px;
            font-weight: 600;
            color: #444;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 14px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
            transition: all 0.3s;
        }
        input[type="text"]:focus, input[type="password"]:focus {
            border-color: #4a6cf7;
            outline: none;
            box-shadow: 0 0 0 3px rgba(74, 108, 247, 0.1);
        }
        .submit-btn {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, #4a6cf7, #667eea);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 17px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 10px;
            transition: all 0.3s;
        }
        .submit-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 20px rgba(74, 108, 247, 0.3);
        }
        .footer {
            margin-top: 30px;
            font-size: 13px;
            color: #999;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>OpenIris 首次配网</h1>
        <p class="subtitle">请输入你的家用 WiFi 信息</p>
        
        <form action="/save" method="POST">
            <div class="form-group">
                <label>WiFi 名称 (SSID)</label>
                <input type="text" name="ssid" required placeholder="请输入 WiFi 名称">
            </div>
            <div class="form-group">
                <label>WiFi 密码</label>
                <input type="password" name="password" placeholder="留空表示无密码">
            </div>
            <button type="submit" class="submit-btn">保存并重启设备</button>
        </form>

        <div class="footer">
            由彩咖工作室 (Galeros Studio) 提供支持
        </div>
    </div>
</body>
</html>
)rawliteral";

// 已连接WiFi时的管理页面（显示当前网络 + 修改按钮）
const char sta_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenIris WiFi 配置</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #333;
        }
        .container {
            background: white;
            padding: 40px 30px;
            border-radius: 16px;
            box-shadow: 0 15px 35px rgba(0,0,0,0.1);
            max-width: 420px;
            width: 90%;
            text-align: center;
        }
        h1 { color: #4a6cf7; margin-bottom: 10px; font-size: 28px; }
        .status {
            background: #f0f8ff;
            padding: 12px 20px;
            border-radius: 8px;
            margin: 20px 0;
            font-size: 15px;
        }
        .form-group {
            margin-bottom: 20px;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 6px;
            font-weight: 600;
            color: #444;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 14px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
            transition: all 0.3s;
        }
        input:focus {
            border-color: #4a6cf7;
            box-shadow: 0 0 0 3px rgba(74, 108, 247, 0.1);
        }
        .submit-btn {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, #4a6cf7, #667eea);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 17px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 10px;
        }
        .submit-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 20px rgba(74, 108, 247, 0.3);
        }
        .footer {
            margin-top: 30px;
            font-size: 13px;
            color: #999;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>OpenIris WiFi 配置</h1>
        
        <div class="status">
            <b>当前已连接：</b> %CURRENT_SSID%
        </div>

        <h2 style="margin:25px 0 15px; color:#555;">修改 WiFi 网络</h2>
        
        <form action="/save" method="POST">
            <div class="form-group">
                <label>新 WiFi 名称 (SSID)</label>
                <input type="text" name="ssid" required placeholder="请输入新的 WiFi 名称">
            </div>
            <div class="form-group">
                <label>新 WiFi 密码</label>
                <input type="password" name="password" placeholder="留空表示不修改密码">
            </div>
            <button type="submit" class="submit-btn">保存并重启设备</button>
        </form>

        <div class="footer">
            由彩咖工作室 (Galeros Studio) 提供支持
        </div>
    </div>
</body>
</html>
)rawliteral";

// ====================== Web 实时日志页面 ======================
const char web_log_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenIris - 实时日志监控</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            font-family: 'Consolas', 'Courier New', monospace;
            background: #0d1117;
            color: #c9d1d9;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        .header {
            background: #161b22;
            padding: 12px 20px;
            border-bottom: 1px solid #30363d;
            display: flex;
            justify-content: space-between;
            align-items: center;
            position: sticky;
            top: 0;
            z-index: 10;
        }
        h1 { font-size: 20px; color: #58a6ff; }
        .controls button {
            padding: 6px 14px;
            margin-left: 8px;
            background: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 6px;
            cursor: pointer;
        }
        .controls button:hover { background: #30363d; }
        #log {
            flex: 1;
            padding: 15px;
            overflow-y: auto;
            background: #0d1117;
            line-height: 1.5;
            font-size: 14px;
            white-space: pre-wrap;
            word-break: break-all;
        }
        .log-line { margin-bottom: 2px; }
        .timestamp { color: #8b949e; margin-right: 10px; }
        .level-d { color: #58a6ff; }   /* debug */
        .level-i { color: #3fb950; }   /* info */
        .level-w { color: #d29922; }   /* warning */
        .level-e { color: #f85149; }   /* error */
    </style>
</head>
<body>
    <div class="header">
        <h1>📡 OpenIris 实时日志监控 </h1>
        <div class="controls">
            <button onclick="clearLog()">清空日志</button>
            <button onclick="togglePause()">暂停/继续</button>
        </div>
    </div>
    <div id="log"></div>

    <script>
        let eventSource = null;
        let isPaused = false;

        function startLogStream() {
            if (eventSource) eventSource.close();
            
            eventSource = new EventSource('/logs');
            const logDiv = document.getElementById('log');

            eventSource.onmessage = function(e) {
                if (isPaused) return;
                const time = new Date().toLocaleTimeString('zh-CN', {hour12: false});
                logDiv.innerHTML += `<div class="log-line"><span class="timestamp">[${time}]</span>${e.data}</div>`;
                logDiv.scrollTop = logDiv.scrollHeight;
            };

            eventSource.onerror = function() {
                console.log("日志连接断开，尝试重连...");
            };
        }

        function clearLog() {
            document.getElementById('log').innerHTML = '';
        }

        function togglePause() {
            isPaused = !isPaused;
            alert(isPaused ? "日志已暂停" : "日志已继续");
        }

        // 页面加载完成后启动
        window.onload = startLogStream;
    </script>
</body>
</html>
)rawliteral";


#endif // HTML_CONTENT_H
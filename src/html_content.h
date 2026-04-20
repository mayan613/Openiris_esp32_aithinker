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

#endif // HTML_CONTENT_H
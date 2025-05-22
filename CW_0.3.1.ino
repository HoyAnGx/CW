// ESP MORSE CODE (ver0.3.1)
// 这是一个ESP发送摩斯码的程序
// 可以不用LED,不用LCD1602,不播放莫斯码
// 只用一枚ESP-01的两个GPIO控制发报
// 由 BD1AMC 献上



#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LiquidCrystal_I2C.h>

// WiFi凭据
struct WiFiCred {
  const char* ssid;
  const char* password;
};

WiFiCred wifiCreds[] = {
  {"@Ruijie", "#plankton#"},
  {"BackupWiFi", "backup123"}
};
const int numWiFi = sizeof(wifiCreds) / sizeof(wifiCreds[0]);

// Web服务器运行在端口80
WebServer server(80);

// LCD1602设置（I2C地址0x27，16列x2行）
LiquidCrystal_I2C lcd(0x27, 16, 2); // SDA=GPIO 21，SCL=GPIO 22

// 引脚定义
const int ledPin = 13;      // LED输出引脚（GPIO 13）
const int cwOutputPin = 14; // CW输出引脚（GPIO 14）

// 莫斯码表（A-Z, 0-9）
const char* morseCode[] = {
  ".-", "-...", "-.-.", "-..", ".", // A-E
  "..-.", "--.", "....", "..", ".---", // F-J
  "-.-", ".-..", "--", "-.", "---", // K-O
  ".--.", "--.-", ".-.", "...", "-", // P-T
  "..-", "...-", ".--", "-..-", "-.--", // U-Y
  "--..", // Z
  "-----", ".----", "..---", "...--", "....-", // 0-4
  ".....", "-....", "--...", "---..", "----." // 5-9
};

// 莫斯码时间参数（毫秒，20 WPM）
const int dotDuration = 60;   // 点持续时间：60 ms
const int dashDuration = 180; // 划持续时间：180 ms
const int symbolGap = 60;     // 符号间间隔：60 ms
const int letterGap = 180;    // 字母间间隔：180 ms
const int wordGap = 420;      // 单词间间隔：420 ms

// 滚动显示变量
String inputText = ""; // 存储当前报文
int scrollPos = 0;     // 滚动起始位置

// 初始化硬件和网络
void setup() {
  // 初始化串口用于调试
  Serial.begin(115200);

  // 初始化引脚并设置默认状态
  pinMode(ledPin, OUTPUT);
  pinMode(cwOutputPin, OUTPUT);
  digitalWrite(ledPin, LOW);       // LED断开
  digitalWrite(cwOutputPin, LOW);  // CW低电平（空闲）

  // 初始化LCD1602
  lcd.init();
  lcd.backlight();                 // 开启背光以显示开机信息
  lcd.setCursor(0, 0);
  lcd.print("BD1AMC");           // 第一行显示呼号
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");    // 第二行显示连接状态

  // 连接WiFi
  connectWiFi();

  // 初始化mDNS
  if (MDNS.begin("morse")) {
    Serial.println("mDNS启动: http://morse.local/");
    lcd.setCursor(0, 1);
    lcd.print("mDNS OK        ");
    delay(1000);
  } else {
    Serial.println("mDNS启动失败");
    lcd.setCursor(0, 1);
    lcd.print("mDNS Failed    ");
    delay(1000);
  }

  // 设置Web服务器路由
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
  Serial.println("Web服务器启动");
  lcd.setCursor(0, 1);
  lcd.print("Server OK      ");
  delay(1000);

  // 显示默认界面
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BD1AMC MORSE");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

// 连接WiFi，支持多WiFi尝试
void connectWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    // 尝试每个WiFi
    for (int i = 0; i < numWiFi; i++) {
      Serial.println("尝试连接WiFi: " + String(wifiCreds[i].ssid));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("BD1AMC");
      lcd.setCursor(0, 1);
      lcd.print("Connecting..."); // 可选：显示wifiCreds[i].ssid（需截断）
      WiFi.begin(wifiCreds[i].ssid, wifiCreds[i].password);
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi连接成功");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("BD1AMC MORSE");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        return; // 连接成功，退出
      } else {
        Serial.println("\n连接失败: " + String(wifiCreds[i].ssid));
      }
    }
    
    // 所有WiFi尝试失败
    Serial.println("所有WiFi连接失败，重试...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BD1AMC MORSE");
    lcd.setCursor(0, 1);
    lcd.print("WiFi Failed");
    delay(5000); // 等待5秒后重试
  }
}

// 主循环，处理客户端请求并保持空闲状态
void loop() {
  server.handleClient();
  // 无输入时保持默认状态
  digitalWrite(ledPin, LOW);       // LED断开
  digitalWrite(cwOutputPin, LOW);  // CW低电平（空闲）
  lcd.noBacklight();               // 空闲时关闭背光
}

// 主页面，显示输入框和快捷按钮
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>BD1AMC Morse Code</title>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;padding:20px;background-color:#f0f0f0;}";
  html += "input{font-size:16px;padding:10px;margin:10px;}";
  html += "button{font-size:16px;padding:10px 20px;margin:5px;cursor:pointer;}";
  html += ".selected{background-color:red;color:white;font-weight:bold;}";
  html += "#sending{display:none;font-size:18px;color:blue;margin-top:20px;}";
  html += "</style>";
  html += "<script>";
  html += "function selectElement(id) {";
  html += "  document.querySelectorAll('input,button').forEach(el => el.classList.remove('selected'));";
  html += "  if (id) document.getElementById(id).classList.add('selected');";
  html += "}";
  html += "function submitForm(text, id) {";
  html += "  selectElement(id);";
  html += "  var sending = document.getElementById('sending');";
  html += "  var inputs = document.querySelectorAll('input,button');";
  html += "  inputs.forEach(el => el.disabled = true);";
  html += "  sending.style.display = 'block';";
  html += "  fetch('/submit?text=' + encodeURIComponent(text), { signal: AbortSignal.timeout(30000) })";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      if (data === 'OK') {";
  html += "        inputs.forEach(el => el.disabled = false);";
  html += "        sending.style.display = 'none';";
  html += "      } else {";
  html += "        alert('发送失败，请重试');";
  html += "        inputs.forEach(el => el.disabled = false);";
  html += "        sending.style.display = 'none';";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      alert('网络错误: ' + error);";
  html += "      inputs.forEach(el => el.disabled = false);";
  html += "      sending.style.display = 'none';";
  html += "    });";
  html += "}";
  html += "</script>";
  html += "</head><body>";
  html += "<h1>BD1AMC Morse Code</h1>";
  html += "<form onsubmit='event.preventDefault();submitForm(document.getElementById(\"textInput\").value, \"textInput\")'>";
  html += "<input id='textInput' type='text' name='text' placeholder='输入字母或数字 (A-Z, 0-9)' maxlength='50'>";
  html += "<br><button type='submit'>发送</button>";
  html += "</form>";
  html += "<h3>快捷报文</h3>";
  html += "<div><button id='btn1' onclick=\"submitForm('CQ CQ CQ', 'btn1')\">CQ CQ CQ</button></div>";
  html += "<div><button id='btn2' onclick=\"submitForm('DE BD1AMC BD1AMC BD1AMC PSE K', 'btn2')\">DE BD1AMC BD1AMC BD1AMC PSE K</button></div>";
  html += "<div><button id='btn3' onclick=\"submitForm('DE BD1AMC UR 59 K', 'btn3')\">DE BD1AMC UR 59 K</button></div>";
  html += "<div><button id='btn4' onclick=\"submitForm('QTH MI YUN BEI JING', 'btn4')\">QTH MI YUN BEI JING</button></div>";
  html += "<div><button id='btn5' onclick=\"submitForm('73', 'btn5')\">73</button></div>";
  html += "<div id='sending'>发送中...</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// 处理提交的报文，发送莫斯码
void handleSubmit() {
  if (server.hasArg("text")) {
    String input = server.arg("text");
    input.toUpperCase();
    // 验证输入，仅保留A-Z、0-9和空格
    String validInput = "";
    for (int i = 0; i < input.length(); i++) {
      char c = input[i];
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
        validInput += c;
      }
    }
    if (validInput.length() == 0) {
      Serial.println("错误: 非法输入");
      server.send(400, "text/plain", "Invalid input");
      return;
    }
    Serial.println("Received: " + validInput);

    // 初始化滚动显示
    inputText = validInput;
    scrollPos = 0;

    // 更新LCD显示报文
    lcd.clear();
    lcd.backlight(); // 确保发报时背光开启
    lcd.setCursor(0, 0);
    if (validInput.length() > 16) {
      lcd.print(validInput.substring(0, 16)); // 显示前16字符
    } else {
      lcd.print(validInput); // 显示完整报文
    }
    lcd.setCursor(0, 1);

    // 发送莫斯码
    for (int i = 0; i < validInput.length(); i++) {
      char c = validInput[i];
      if (c >= 'A' && c <= 'Z') {
        String morse = morseCode[c - 'A'];
        Serial.println("发送字符: " + String(c) + " (" + morse + ")");
        playMorse(morse, c);
        delay(letterGap);
        // 清空第二行，准备下一个字符
        lcd.setCursor(0, 1);
        lcd.print("                ");
      } else if (c >= '0' && c <= '9') {
        String morse = morseCode[26 + (c - '0')];
        Serial.println("发送字符: " + String(c) + " (" + morse + ")");
        playMorse(morse, c);
        delay(letterGap);
        // 清空第二行，准备下一个字符
        lcd.setCursor(0, 1);
        lcd.print("                ");
      } else if (c == ' ') {
        Serial.println("单词间隔");
        delay(wordGap);
        // 清空第二行，准备下一个字符
        lcd.setCursor(0, 1);
        lcd.print("                ");
      }
    }

    // 恢复LCD显示
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Standing By.....");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    lcd.noBacklight(); // 发报完成后关闭背光
    inputText = ""; // 清空报文
    scrollPos = 0;  // 重置滚动位置

    Serial.println("发报完成");
    server.send(200, "text/plain", "OK");
  } else {
    Serial.println("错误: 无输入");
    server.send(400, "text/plain", "Invalid input");
  }
}

// 播放莫斯码，控制CW、LED、背光和LCD
void playMorse(String morse, char letter) {
  // 居中显示 (字母) 莫斯码
  String displayStr = "(" + String(letter) + ") " + morse;
  int totalLength = displayStr.length();
  int startCol = (16 - totalLength) / 2; // 居中起始列
  lcd.setCursor(0, 1);
  // 填充左边空格
  for (int i = 0; i < startCol; i++) {
    lcd.print(" ");
  }
  // 显示 (字母) 莫斯码
  lcd.print(displayStr);
  // 填充右边空格
  for (int i = startCol + totalLength; i < 16; i++) {
    lcd.print(" ");
  }

  // 播放点划
  for (int i = 0; i < morse.length(); i++) {
    char symbol = morse[i];
    if (symbol == '.') {
      Serial.println("CW点: 高电平（电键按下，触发发送）");
      digitalWrite(cwOutputPin, HIGH); // 电键按下（3.3V，触发发送）
      digitalWrite(ledPin, HIGH);      // LED点亮
      //lcd.backlight();                 // 开启背光
      delay(dotDuration);
      Serial.println("CW点: 低电平（电键释放，空闲）");
      digitalWrite(cwOutputPin, LOW);  // 电键释放（0V，空闲）
      digitalWrite(ledPin, LOW);       // LED关闭
      //lcd.noBacklight();               // 关闭背光
    } else if (symbol == '-') {
      Serial.println("CW划: 高电平（电键按下，触发发送）");
      digitalWrite(cwOutputPin, HIGH); // 电键按下（3.3V，触发发送）
      digitalWrite(ledPin, HIGH);      // LED点亮
      //lcd.backlight();                 // 开启背光
      delay(dashDuration);
      Serial.println("CW划: 低电平（电键释放，空闲）");
      digitalWrite(cwOutputPin, LOW);  // 电键释放（0V，空闲）
      digitalWrite(ledPin, LOW);       // LED关闭
      //lcd.noBacklight();               // 关闭背光
    }
    // 更新第一行滚动显示
    if (inputText.length() > 16) {
      lcd.setCursor(0, 0);
      int endPos = scrollPos + 16;
      if (endPos > inputText.length()) {
        endPos = inputText.length();
      }
      lcd.print(inputText.substring(scrollPos, endPos));
      // 填充剩余空间
      for (int j = endPos - scrollPos; j < 16; j++) {
        lcd.print(" ");
      }
      scrollPos++;
      if (scrollPos >= inputText.length() - 15) {
        scrollPos = 0; // 循环滚动
      }
    }
    delay(symbolGap);
  }
}

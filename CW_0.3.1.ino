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
  {"NETGEAR", "classover"}
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

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(cwOutputPin, OUTPUT);
  digitalWrite(ledPin, LOW);       // LED断开
  digitalWrite(cwOutputPin, HIGH); // CW高电平（空闲，电台静音）
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BD1AMC");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");
  connectWiFi();
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
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
  Serial.println("Web服务器启动");
  lcd.setCursor(0, 1);
  lcd.print("Server OK      ");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BD1AMC MORSE");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

void connectWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < numWiFi; i++) {
      Serial.println("尝试连接WiFi: " + String(wifiCreds[i].ssid));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("BD1AMC");
      lcd.setCursor(0, 1);
      lcd.print("Connecting...");
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
        return;
      } else {
        Serial.println("\n连接失败: " + String(wifiCreds[i].ssid));
      }
    }
    Serial.println("所有WiFi连接失败，重试...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BD1AMC MORSE");
    lcd.setCursor(0, 1);
    lcd.print("WiFi Failed");
    delay(5000);
  }
}

void loop() {
  server.handleClient();
  digitalWrite(ledPin, LOW);       // LED断开
  digitalWrite(cwOutputPin, HIGH); // CW高电平（空闲，电台静音）
  lcd.noBacklight();
}

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

void handleSubmit() {
  if (server.hasArg("text")) {
    String input = server.arg("text");
    input.toUpperCase();
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
    inputText = validInput;
    scrollPos = 0;
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(0, 0);
    if (validInput.length() > 16) {
      lcd.print(validInput.substring(0, 16));
    } else {
      lcd.print(validInput);
    }
    lcd.setCursor(0, 1);
    for (int i = 0; i < validInput.length(); i++) {
      char c = validInput[i];
      if (c >= 'A' && c <= 'Z') {
        String morse = morseCode[c - 'A'];
        Serial.println("发送字符: " + String(c) + " (" + morse + ")");
        playMorse(morse, c);
        delay(letterGap);
        lcd.setCursor(0, 1);
        lcd.print("                ");
      } else if (c >= '0' && c <= '9') {
        String morse = morseCode[26 + (c - '0')];
        Serial.println("发送字符: " + String(c) + " (" + morse + ")");
        playMorse(morse, c);
        delay(letterGap);
        lcd.setCursor(0, 1);
        lcd.print("                ");
      } else if (c == ' ') {
        Serial.println("单词间隔");
        delay(wordGap);
        lcd.setCursor(0, 1);
        lcd.print("                ");
      }
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Standing by.....");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    lcd.noBacklight();
    inputText = "";
    scrollPos = 0;
    Serial.println("发报完成");
    server.send(200, "text/plain", "OK");
  } else {
    Serial.println("错误: 无输入");
    server.send(400, "text/plain", "Invalid input");
  }
}

void playMorse(String morse, char letter) {
  String displayStr = "(" + String(letter) + ") " + morse;
  int totalLength = displayStr.length();
  int startCol = (16 - totalLength) / 2;
  lcd.setCursor(0, 1);
  for (int i = 0; i < startCol; i++) {
    lcd.print(" ");
  }
  lcd.print(displayStr);
  for (int i = startCol + totalLength; i < 16; i++) {
    lcd.print(" ");
  }
  for (int i = 0; i < morse.length(); i++) {
    char symbol = morse[i];
    if (symbol == '.') {
      Serial.println("CW点: 低电平（电键按下，触发发送）");
      digitalWrite(cwOutputPin, LOW);  // 电键按下（0V，触发发送）
      digitalWrite(ledPin, HIGH);      // LED点亮
      lcd.backlight();
      delay(dotDuration);
      Serial.println("CW点: 高电平（电键释放，空闲）");
      digitalWrite(cwOutputPin, HIGH); // 电键释放（3.3V，空闲）
      digitalWrite(ledPin, LOW);
      lcd.noBacklight();
    } else if (symbol == '-') {
      Serial.println("CW划: 低电平（电键按下，触发发送）");
      digitalWrite(cwOutputPin, LOW);  // 电键按下（0V，触发发送）
      digitalWrite(ledPin, HIGH);      // LED点亮
      lcd.backlight();
      delay(dashDuration);
      Serial.println("CW划: 高电平（电键释放，空闲）");
      digitalWrite(cwOutputPin, HIGH); // 电键释放（3.3V，空闲）
      digitalWrite(ledPin, LOW);
      lcd.noBacklight();
    }
    if (inputText.length() > 16) {
      lcd.setCursor(0, 0);
      int endPos = scrollPos + 16;
      if (endPos > inputText.length()) {
        endPos = inputText.length();
      }
      lcd.print(inputText.substring(scrollPos, endPos));
      for (int j = endPos - scrollPos; j < 16; j++) {
        lcd.print(" ");
      }
      scrollPos++;
      if (scrollPos >= inputText.length() - 15) {
        scrollPos = 0;
      }
    }
    delay(symbolGap);
  }
}

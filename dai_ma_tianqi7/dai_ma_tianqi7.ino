#include <ESP8266WiFi.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "p_1.h"
#include "p_2.h"
#include "p_3.h"
#include "p_4.h"
#include "cns.h"
#include <WiFiManager.h>
#include <ESP8266_Seniverse.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

// TFT显示屏配置
TFT_eSPI tft = TFT_eSPI(); // 创建TFT对象

// 按键引脚定义
const int buttonPin = 4;

// 状态变量
enum Page { IMAGE, TIME, WEATHER, EFFECT }; // 定义页面枚举
Page currentPage = IMAGE; // 当前页面状态
bool backgroundUpdated = false; // 用于跟踪背景颜色是否已更新

// 心知天气API配置
String reqUserKey = "在这里粘贴密钥";   // 私钥
String reqLocation = "Beijing";            // 城市
String reqUnit = "c";                      // 摄氏/华氏

WeatherNow weatherNow;  // 建立WeatherNow对象用于获取心知天气信息
Forecast forecast; // 建立Forecast对象用于获取心知天气信息

// 添加天气更新时间控制
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 3600000; // 一小时 = 3600000毫秒
//ip地址更新时间
unsigned long lastLocationUpdate = 0;
const unsigned long locationUpdateInterval = 3600000;
//时间显示刷新时间间隔
unsigned long lastTimeUpdate = 0;
const unsigned long TIME_UPDATE_INTERVAL = 1000; // 1秒更新一次


const char* effects[] = {//随机事件词条
  "DEAD GO BOOM",//
  "METEOR SHOWER",//
  "DEATH BATTLE",//
  "ORBITAL LASER",//
  "LOW GRAVITY",//
  "MEGA DAMAGE",//
  "ALIEN INVASION",//
  "RETRO INVASION 82"//
};


unsigned long lastEffectUpdate = 0;
const unsigned long effectUpdateInterval = 20000; // 20秒
int currentEffect = 0;
int rdcns = 0;//彩蛋随机变量
int upcns = false;//彩蛋背景更新变量

// 休眠模式相关变量
unsigned long lastActivityTime = 0;
const unsigned long SLEEP_TIMEOUT = 60000; // 1分钟无操作后进入休眠
bool isInSleepMode = false;

void updateTimeDisplay(bool updateBackground);

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  // 初始化按键引脚
  pinMode(buttonPin, INPUT_PULLUP);

  // 使用WiFiManager进行WiFi配置
  //开机显示页面
  drawWeatherBackground();
  tft.fillRect(0, 60, 240, 130, TFT_BLACK);
  tft.pushImage(21, 76, 198, 49, cns);
  tft.setCursor(20, 150);
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.printf("Connected to WiFi");
  tft.setCursor(100, 170 );
  tft.printf(".");
  //开始连接wifi
  WiFiManager wifiManager;
  tft.printf(".");
  wifiManager.autoConnect("xX_TF_CNS_CeratopSID_06_Xx");
  tft.printf(".");
  Serial.println("Connected to WiFi");
  tft.printf(".");


  // 获取位置信息
  updateLocationByIP();

  // 配置心知天气请求信息
  weatherNow.config(reqUserKey, reqLocation, reqUnit);
  forecast.config(reqUserKey, reqLocation, reqUnit);

  // 设置时间服务器
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // 显示开机动画
  startupAnimation();

  tft.fillScreen(TFT_BLACK);

  // 初始获取一次天气信息
  getWeather();
  lastWeatherUpdate = millis();
  lastActivityTime = millis(); // 初始化最后活动时间

  randomSeed(analogRead(0));

}

void loop() {

  unsigned long currentTime = millis();

  // 检查是否需要更新位置信息
  if (currentTime - lastLocationUpdate >= locationUpdateInterval) {
    updateLocationByIP();
    lastLocationUpdate = currentTime;
  }

  // 检查按键状态
  if (digitalRead(buttonPin) == LOW) { // 按键按下
    lastActivityTime = currentTime; // 更新最后活动时间
    if (isInSleepMode) {
      // 从休眠模式唤醒
      isInSleepMode = false;
      WiFi.mode(WIFI_STA); // 重新启用WiFi
      WiFi.setSleepMode(WIFI_NONE_SLEEP); // 禁用WiFi休眠
      // 不需要强制更新显示，保持原有内容
    } else {
      currentPage = static_cast<Page>((currentPage + 1) % 4);
      backgroundUpdated = false;
    }
    delay(300);
  }

  // 检查是否需要进入休眠模式
  if (!isInSleepMode && (currentTime - lastActivityTime >= SLEEP_TIMEOUT)) {
    // 进入休眠模式
    isInSleepMode = true;
    // 不清空屏幕，保持显示内容
    WiFi.setSleepMode(WIFI_LIGHT_SLEEP); // 设置WiFi休眠模式
    return; // 跳过其余循环
  }

  // 如果在休眠模式中，跳过不必要的更新操作
  if (isInSleepMode) {
    // 在休眠模式下，只在TIME页面时更新时间显示（如果需要的话）
    if (currentPage == TIME) {
      // 可以降低时间更新频率
      static unsigned long lastTimeUpdate = 0;
      if (currentTime - lastTimeUpdate >= 10000) { // 10秒更新一次
        updateTimeDisplay(false); // 需要新建这个函数来更新时间
        lastTimeUpdate = currentTime;
      }
    }
    delay(100);
    return;
  }


  // 检查是否需要更新天气信息
  if (currentTime - lastWeatherUpdate >= weatherUpdateInterval) {
    getWeather();
    lastWeatherUpdate = currentTime;
  }

  // 根据当前页面显示内容
  if (currentPage == IMAGE) {
    if (!backgroundUpdated) {
      tft.fillScreen(TFT_BLACK);
      backgroundUpdated = true;

      // 随机选择 p_1 或 p_2 显示
      currentEffect = random(0, 4); // 0 表示 p_1，1 表示 p_2，2表示p_3，3表示p_4
      rdcns = random(0, 15);//彩蛋概率
    }

    // 根据选择的图片设置宽度和高度
    int imageWidth = (currentEffect == 0) ? 116 : (currentEffect == 1) ? 188 : (currentEffect == 2) ? 188 : 188; // 根据选择的图片设置宽度
    int imageHeight = (currentEffect == 0) ? 152 : (currentEffect == 1) ? 108 : (currentEffect == 2) ? 60 : 50; // 根据选择的图片设置高度

    // 计算居中位置
    int xOffset = (240 - imageWidth) / 2; // 水平居中
    int yOffset = (240 - imageHeight) / 2; // 垂直居中

    if (rdcns == 5) {//彩蛋触发
      imageWidth = 198;
      imageHeight = 49;
      xOffset = (240 - imageWidth) / 2; // 水平居中
      yOffset = (240 - imageHeight) / 2; // 垂直居中
      tft.pushImage(xOffset, yOffset, imageWidth, imageHeight, cns);
      if (random(0, 10) == 5) {//彩色屏闪概率
        drawWeatherBackground();
        tft.setTextColor(TFT_BLUE);
        //tft.pushImage(xOffset, yOffset, imageWidth, imageHeight, cns);
        tft.setTextSize(2);
        tft.setCursor(20, 20);
        tft.printf("GOOD EVENING ! \n");
        tft.printf("THIS BROADCAST HAS BEEN HACKED BY CNS \n");
        tft.printf("[ONE NIGHT, THREE DAWNS \n");
        tft.printf("SEE THE SIGNS] \n");
        upcns = false;
      } else {
        if (!upcns) {
          tft.fillScreen(TFT_BLACK);
          upcns = true; // 标记彩蛋背景已更新
        }
      }
    } else {//彩蛋未触发
      // 显示图片
      tft.pushImage(xOffset, yOffset, imageWidth, imageHeight, (currentEffect == 0) ? p_1 : (currentEffect == 1) ? p_2 : (currentEffect == 2) ? p_3 : p_4); // 显示图片
    }

  } else if (currentPage == TIME) {
    // 切换至显示时间界面时，设置背景颜色为红色（仅更新一次）
    if (!backgroundUpdated) {
      tft.fillScreen(TFT_RED); // 设置背景为红色
      backgroundUpdated = true; // 标记背景已更新
      updateTimeDisplay(true); // 立即更新时间显示，true表示包含背景
    }


    if (currentTime - lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
      updateTimeDisplay(false); // false表示不更新背景
      lastTimeUpdate = currentTime;
    }
  } else if (currentPage == WEATHER) {
    // 切换至显示天气界面时，绘制七个颜色块
    if (!backgroundUpdated) {
      drawWeatherBackground(); // 绘制天气背景
      backgroundUpdated = true; // 标记背景已更新
      displayWeather(); // 显示已缓存的天气信息
    }
  } else if (currentPage == EFFECT) {
    // 切换至显示词条界面时，绘制背景
    if (!backgroundUpdated) {
      drawWeatherBackground(); // 使用相同的背景
      backgroundUpdated = true;
      currentEffect = random(0, 8);
      displayEffect(); // 显示初始词条
    }

    // 检查是否需要更新词条
    if (currentTime - lastEffectUpdate >= effectUpdateInterval) {
      currentEffect = random(0, 8); // 随机选择一个词条
      displayEffect();
      lastEffectUpdate = currentTime;
    }
  }

  delay(100); // 每次循环的延迟
}

void drawWeatherBackground() {//显示彩屏背景函数
  // 定义颜色数组
  uint16_t colors[] = {TFT_WHITE, TFT_YELLOW, TFT_CYAN, TFT_GREEN, TFT_PINK, TFT_RED, TFT_NAVY};
  int blockWidth = 240 / 6.8; // 每个颜色块的宽度

  // 绘制七个颜色块
  for (int i = 0; i < 7; i++) {
    tft.fillRect(i * blockWidth, 0, blockWidth, 240, colors[i]);
  }
}

void displayWeather() {//天气显示函数

  if (isInSleepMode) return; // 休眠模式下不更新显示

  int rectWidth = 240; // 矩形宽度
  int rectHeight = 120; // 矩形高度
  int rectX = (240 - rectWidth) / 2; // 矩形X坐标
  int rectY = (240 - rectHeight) / 2; // 矩形Y坐标

  tft.fillRect(rectX, rectY, rectWidth, rectHeight, TFT_BLACK); // 绘制黑色矩形

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  tft.setCursor(rectX + 10, rectY + 10); // 设置光标位置
  tft.printf("Weather Info:"); // 显示天气信息标题

  tft.setCursor(rectX + 10, rectY + 30); // 设置光标位置
  tft.setTextColor(TFT_GREEN);
  tft.printf("Temp: %dC-%dC-%dC", forecast.getLow(0), weatherNow.getDegree(), forecast.getHigh(0)); //显示气温
  //tft.printf("Temp: %dC", weatherNow.getDegree());//显示气温

  tft.setCursor(rectX + 10, rectY + 50); // 设置光标位置
  tft.setTextColor(TFT_ORANGE);
  tft.printf("Weather: %s", weatherNow.getWeatherText());//显示天气

  tft.setCursor(rectX + 10, rectY + 70); // 设置光标位置
  tft.setTextColor(TFT_BLUE);
  tft.printf("Rainfall:%.0f%%", forecast.getRain(0)); //显示降雨概率

  tft.setCursor(rectX + 10, rectY + 90); // 设置光标位置
  tft.setTextColor(TFT_CYAN);
  tft.printf("Humidity: %d%%",  forecast.getHumidity(0));//显示湿度
}

void getWeather() {//获取天气信息函数

  if (isInSleepMode) return; // 休眠模式下不更新显示

  if (WiFi.status() == WL_CONNECTED) {
    if (weatherNow.update() && forecast.update()) { // 检查请求是否成功
      if (currentPage == WEATHER) {
        displayWeather(); // 如果当前在天气页面，则更新显示
      }
    } else {
      Serial.println("Error on weather request");
    }
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void displayEffect() {//显示随机词条函数

  if (isInSleepMode) return; // 休眠模式下不更新显示
  
  int rectWidth = 240;
  int rectHeight = 120;
  int rectX = (240 - rectWidth) / 2;
  int rectY = (240 - rectHeight) / 2;

  tft.fillRect(rectX, rectY, rectWidth, rectHeight, TFT_BLACK);

  // 显示标题
  tft.setTextSize(2);
  int titleWidth = tft.textWidth("Random Event:");
  int titleX = (240 - titleWidth) / 2;
  tft.setCursor(titleX, rectY + 10);
  tft.setTextColor(TFT_WHITE);
  tft.print("Random Event:");

  // 处理词条文本
  tft.setTextSize(3);
  const char* text = effects[currentEffect];
  String words = String(text);
  int maxWidth = rectWidth - 20; // 留出左右边距

  // 分割单词
  String line1 = "";
  String line2 = "";
  int spaceIndex = words.indexOf(' ');

  if (spaceIndex != -1) {
    // 如果有空格，尝试分两行显示
    line1 = words.substring(0, spaceIndex);
    line2 = words.substring(spaceIndex + 1);
  } else {
    // 如果没有空格，整个词条作为一行
    line1 = words;
  }

  // 显示第一行
  tft.setTextColor(TFT_RED);
  int textWidth = tft.textWidth(line1.c_str());
  int textX = (240 - textWidth) / 2;
  tft.setCursor(textX, rectY + 50);
  tft.print(line1);

  // 如果有第二行，显示第二行
  if (line2.length() > 0) {
    textWidth = tft.textWidth(line2.c_str());
    textX = (240 - textWidth) / 2;
    tft.setCursor(textX, rectY + 80);
    tft.print(line2);
  }
}

void startupAnimation() {//开机动画函数
  if (!backgroundUpdated) {
    tft.fillScreen(TFT_BLACK);
    backgroundUpdated = true;
  }

  // 固定使用 p_1 作为开机时显示的图片
  const uint16_t* startupImage = p_1;
  int imageWidth = 116; // p_1 的宽度
  int imageHeight = 152; // p_1 的高度

  // 计算居中位置
  int xOffset = (240 - imageWidth) / 2; // 水平居中
  int yOffset = (240 - imageHeight) / 2; // 竖直居中

  // 定义移动参数
  int leftStartX = -imageWidth; // 左侧图片起始位置
  int rightStartX = 240; // 右侧图片起始位置
  int topStartY = -imageHeight; // 上侧图片起始位置
  int bottomStartY = 240; // 下侧图片起始位置
  int speed = 4;   // 移动速度

  // 动画循环，直到所有图片到达中心位置
  while (leftStartX < xOffset || rightStartX > xOffset || topStartY < yOffset || bottomStartY > yOffset) {



    // 绘制从左侧移动的图片
    if (leftStartX < xOffset) {
      tft.pushImage(leftStartX, yOffset, imageWidth, imageHeight, startupImage);
      leftStartX += speed;  // 图片向右移动
    }

    // 绘制从右侧移动的图片
    if (rightStartX > xOffset) {
      tft.pushImage(rightStartX, yOffset, imageWidth, imageHeight, startupImage);
      rightStartX -= speed;  // 图片向左移动
    }

    // 绘制从上侧移动的图片
    if (topStartY < yOffset) {
      tft.pushImage(xOffset, topStartY, imageWidth, imageHeight, startupImage);
      topStartY += speed;  // 图片向下移动
    }

    // 绘制从下侧移动的图片
    if (bottomStartY > yOffset) {
      tft.pushImage(xOffset, bottomStartY, imageWidth, imageHeight, startupImage);
      bottomStartY -= speed;  // 图片向上移动
    }

    delay(1);  // 控制动画速度

    // 防止ESP8266看门狗触发重启
    yield();
  }

  // 确保完整显示图片在中心位置

  tft.pushImage(xOffset, yOffset, imageWidth, imageHeight, startupImage);
  delay(1000);  // 显示1秒
}

void updateLocationByIP() {//根据ip地址获取天气信息

  if (isInSleepMode) return; // 休眠模式下不更新显示

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;

    // 使用ipapi.co的免费API获取位置信息
    String url = "http://ip-api.com/json/";

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      // 使用ArduinoJson解析返回的JSON数据
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // 获取城市名称
        const char* city = doc["city"];
        const char* regionName = doc["regionName"];
        if (city) {
          reqLocation = String(regionName) + " " + String(city);
          Serial.println("Updated location to: " + reqLocation);

          // 更新心知天气配置
          weatherNow.config(reqUserKey, reqLocation, reqUnit);
          forecast.config(reqUserKey, reqLocation, reqUnit);

          // 立即更新天气信息
          getWeather();
        }
      }
    }

    http.end();
  }
}

void updateTimeDisplay(bool updateBackground) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  // 设置文本颜色和大小
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);

  // 显示日期
  char dateStr[20];
  sprintf(dateStr, "%04d-%02d-%02d",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday);

  int dateWidth = tft.textWidth(dateStr);
  int dateX = (240 - dateWidth) / 2;
  int dateY = 80;

  // 只在需要时更新背景
  if (updateBackground) {
    tft.fillRect(0, dateY - 20, 240, 80, TFT_BLACK);
  } else {
    // 仅清除文本区域
    tft.fillRect(dateX - 5, dateY - 5, dateWidth + 10, 40, TFT_BLACK);
  }
  tft.setCursor(dateX, dateY);
  tft.print(dateStr);

  // 显示时间
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d",
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);

  int timeWidth = tft.textWidth(timeStr);
  int timeX = (240 - timeWidth) / 2;
  int timeY = 140;

  // 只在需要时更新背景
  if (updateBackground) {
    tft.fillRect(0, timeY - 40, 240, 80, TFT_BLACK);
  } else {
    // 仅清除文本区域
    tft.fillRect(timeX - 5, timeY - 5, timeWidth + 10, 40, TFT_BLACK);
  }
  tft.setCursor(timeX, timeY);
  tft.print(timeStr);
}

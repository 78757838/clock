/*@autor: nikzin
基于CAD文件和原始代码：https://www.instructables.com/Hollow-Clock-4/
基于shiura的伟大工作。感谢您的这个伟大项目。
与shiura的原始版本中的Arduino Nano不同，我使用了ESP8266 D1 Mini，这也（勉强）适合提供的盒子中。
通过使用ESP，我可以从NTP服务器获取实时时间。我还添加了夏令时（因为在德国我们仍然有夏令时）。不过我没有测试这个功能。
当您启动时钟时，请将时间设置为12点，并等待直到它连接到WiFi并设置自己。
之后它每10秒钟从服务器更新时间。
如果没有互联网连接，则时钟将像原始版本一样运行，直到可以重新连接到WiFi。然后它会再次将自己设置为正确的时间。
平均功耗约为50mA（包括WiFi通信和每分钟运行的电机）
要连接到WiFi，只需在下面添加您的WiFi凭据。
玩得开心，使用这个中空时钟4版本！
*/

#include <ESP8266WiFi.h>
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <sys/time.h>

// 如果时钟获得或丢失，请调整以下值。理论上，该值的标准值为60000。
#define MILLIS_PER_MIN 60000 // 每分钟的毫秒数（如果没有互联网连接，则仍在使用）

// 电机和时钟参数
// 4096 * 90 / 12 = 30720
#define STEPS_PER_ROTATION 30745 // 调整每分钟转子的全圈步数（这对我来说与原始值不同（30720））

#define MY_TZ "CST-8" // 为您的时区添加正确的字符串：https://werner.rothschopf.net/202011_arduino_esp8266_ntp_en.htm
#define MY_NTP_SERVER "pool.ntp.org"

// 等待步进电机的单个步骤
int delaytime = 2;

time_t now; // 这是时代
tm tm; // 结构tm以更方便的方式保存时间信息

// 用于控制步进电机的端口
// 如果您的电机旋转到相反的方向，
// 更改顺序为{D5，D6，D7，D8}；
int port[4] = {D5, D7, D6, D5};

// 步进电机控制序列
int seq[8][4] = {
  {  LOW, HIGH, HIGH,  LOW},
  {  LOW,  LOW, HIGH,  LOW},
  {  LOW,  LOW, HIGH, HIGH},
  {  LOW,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW,  LOW},
  { HIGH, HIGH,  LOW,  LOW},
  {  LOW, HIGH,  LOW,  LOW}
};

// 定义NTP客户端以获取时间
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


void setTimezone(String timezone){
  setenv("TZ",timezone.c_str(),1);  // 现在调整时区
  tzset();
}



// 保存日期、时间和其他必要参数的变量
int Minute, Hour, currHour, currMinute, hourDiff, minuteDiff, stepsToGo;

bool skip = true;


void rotate(int step) { // shiura的原始函数
  static int phase = 0;
  int i, j;
  int delta = (step > 0) ? 1 : 7;
  int dt = 20;

  step = (step > 0) ? step : -step;
  for(j = 0; j < step; j++) {
    phase = (phase + delta) % 8;
    for(i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]);
    }
    delay(dt);
    if(dt > delaytime) dt--;
  }
  // 切断电源
  for(i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}

void rotateFast(int step) { //这只是为了更快地将时钟旋转到当前时间，当时钟启动时
  static int phase = 0;
  int i, j;
  int delta = (step > 0) ? 1 : 7;
  int dt = 1;

  step = (step > 0) ? step : -step;
  for(j = 0; j < step; j++) {
    phase = (phase + delta) % 8;
    for(i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]);
    }
    delay(dt);
    if(dt > delaytime) dt--;
  }
 // 切断电源
  for(i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}


void updateTime(){

  time(&now);                       // 读取当前时间
  localtime_r(&now, &tm);           // 更新当前时间的tm结构

  Hour = tm.tm_hour;
  Minute = tm.tm_min;

}

void getTimeDiff(){
  updateTime();

  if(currHour != Hour){
    if(Hour == 12 || Hour == 24){
      currHour = Hour;
      hourDiff = 0;
    }else{
      if(Hour > 12){
        hourDiff = Hour - 12;
        currHour = Hour;
      }else{
        currHour = Hour;
        hourDiff = Hour;
      }
     
    }
     
  }

  if(currMinute != Minute){
    minuteDiff = Minute;
    currMinute = Minute;
  }

}

//Ticker update;


void initWifi(){
  int counter=0;
  configTime(MY_TZ, MY_NTP_SERVER);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin("hass", "yhnko123"); // 在这里添加您的WiFi SSID和密码

  while (WiFi.status() != WL_CONNECTED && counter < 120) {  //  等待连接，但最多1分钟
      delay(500);
      counter += 1;
  }

  if(counter == 120){ // 如果连接失败，因为已经过去了一分钟，将当前时间加1并继续
    currMinute += 1;
  }  

}

void setup() {
  Serial.begin(9600);

  // 在启动时，时钟预计设置为12点钟
  currHour = 0;
  currMinute = 0;

  initWifi();
  
  
  pinMode(port[0], OUTPUT);
  pinMode(port[1], OUTPUT);
  pinMode(port[2], OUTPUT);
  pinMode(port[3], OUTPUT);

  
  getTimeDiff();  // 第一次启动时，时钟期望它设置为12点钟，并将自己设置为当前时间

  rotate(-20);  // 用于接近运行
  rotate(20); // 没有重负载的接近运行
  rotateFast((STEPS_PER_ROTATION*hourDiff));
  rotateFast(((minuteDiff*STEPS_PER_ROTATION)/60));

  hourDiff = 0;
  minuteDiff = 0;
  
  
}



void loop() {
  if(WiFi.status() != WL_CONNECTED){   // 如果没有WiFi连接，时钟将运行原始版本
    static long prev_min = 0, prev_pos = 0;
    long min;
    static long pos;
    
    min = millis() / MILLIS_PER_MIN;
    if(prev_min == min) {
      return;
    }
    prev_min = min;
    pos = (STEPS_PER_ROTATION * min) / 60;
    rotate(-20); // 用于接近运行
    rotate(20); // 没有重负载的接近运行
    if(pos - prev_pos > 0 && skip == false) {
      rotate(pos - prev_pos);
      
      currMinute += 1;
      if(currMinute == 60){
        currMinute = 0;
        currHour += 1;
        if(currHour == 24){
          currHour = 0;
        }
      }

    }
    prev_pos = pos;
    skip = false;
  }

  if(WiFi.status() == WL_CONNECTED){  //  if there is a WiFi connection the clock will check if there is a time difference
    updateTime();
    skip = true;

    if(currMinute != 59 && currHour != Hour){ //  some conversion of the time to fit the 12h clock
      int newCurrHour;
      if(Hour > 12){
        Hour = Hour - 12;
      }
      newCurrHour = currHour;
      if(currHour > 12){
        newCurrHour = currHour - 12;
      }
      if(Hour > newCurrHour){
        hourDiff == Hour - newCurrHour;
      }else if(newCurrHour > Hour){
        hourDiff = 12 - (newCurrHour - Hour);
      }
    }

    if(currMinute != Minute){
      if(Minute < currMinute){
        minuteDiff = 60 - currMinute + Minute;
      }else{
        minuteDiff = Minute - currMinute;
        // currMinute = Minute;
      }

      currMinute = Minute;
      currHour = Hour;

      
      if(minuteDiff >= 0){  //  this is used to make sure the clock does not go backwards, since its not really working backwards
        rotate(-20); // for approach run
        rotate(20); // approach run without heavy load
        rotate(((minuteDiff*STEPS_PER_ROTATION)/60));
      }
      if(hourDiff >= 0){
        rotate((STEPS_PER_ROTATION*hourDiff));
      }
      
      minuteDiff = 0;
      hourDiff = 0;

    }

    delay(10000);
  }
}

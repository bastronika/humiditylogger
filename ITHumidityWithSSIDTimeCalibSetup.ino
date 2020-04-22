#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <FS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

String MAC;
const char* ssid = "GWI H&T Monitor";  // SSID for access point mode
const char* password = "q1w2e3r4";  //Password for access point mode

Adafruit_BME280 bme;
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

float temperature, humidity, pressure, altitude;
unsigned long oldTime;
boolean mode = 0;
int port = 81;
long int angka;

int sign, lastSign;

int fail = 1;
String destination;

int adjHumidity = 0;
int adjTemperature = 0;

WebSocketsServer* webSocket;
AsyncWebServer server(80);
DateTime now;
HTTPClient http;

void postData(float temp, float humidity);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void writeString(char add, String data);
String read_String(char add);

String processor(const String& var) {
  Serial.println(var);
  Serial.println("server Called");
  return String();
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(D4, D3);
  lcd.begin();
  rtc.begin();
  bme.begin(0x76);
  EEPROM.begin(512);

  // Turn on the blacklight and print a message.
  lcd.backlight();

  if (digitalRead(D2) == LOW) {
    mode = 1;
    if (!SPIFFS.begin()) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
    lcd.print("Setup Mode");
    Serial.println();
    Serial.println("Configuring access point...");

    // You can remove the password parameter if you want the AP to be open.
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    server.begin();

    Serial.println("Server started");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/index.html", String(), false, processor);
    });

    // Route to load style.css file
    server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/bootstrap.min.css", "text/css");
    });

    //route to javascript
    server.on("/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/bootstrap.min.js", "text/css");
    });

    server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/jquery.min.js", "text/css");
    });

    server.on("/gwilogo.png", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/gwilogo.png", "img");
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/favicon.ico", "img");
    });


    webSocket = new WebSocketsServer(81);
  }

  else {
    lcd.print("Running!!");
    delay(2000);
    String dataRead = read_String(10);

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(dataRead);

    String ssidrec = root["s"];

    String passwordrec = root["p"];

    String destinationrec = root["d"];

    String dataRead2 = read_String(2000);
    JsonObject& root2 = jsonBuffer.parseObject(dataRead2);

    adjHumidity = root2["ah"];
    adjTemperature = root2["at"];

    Serial.print("adjHumidity = ");
    Serial.println(adjHumidity);
    Serial.print("adjTemperature = ");
    Serial.println(adjTemperature );

    destination = destinationrec;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ssidrec);
    lcd.setCursor(0, 1);
    lcd.print(passwordrec);
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(destinationrec);
    delay(2000);
    lcd.clear();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidrec, passwordrec);
    lcd.setCursor(0, 0);
    lcd.print("Connecting......");

    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
    MAC = WiFi.macAddress();
    IPAddress ip = WiFi.localIP();
    int iptiga = ip[3];
    port = (iptiga * 100) + 81;
    lcd.setCursor(0, 0);
    lcd.print(MAC);
    lcd.setCursor(0, 1);
    lcd.print(ip);
    lcd.print(":");
    lcd.print(port);

    delay(5000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connected");
    lcd.setCursor(10, 0);
    lcd.print(port);
    lcd.setCursor(0, 1);
    lcd.print(ip);
    delay(5000);
    lcd.clear();
    webSocket = new WebSocketsServer(port);

  }
  webSocket->begin();
  webSocket->onEvent(webSocketEvent);

}

void loop()
{
  webSocket->loop();

  if (mode == 0) {
    if ((millis() - oldTime) > 2000)   // Only process counters once per second
    {
      now = rtc.now();       //Menampilkan RTC pada variable now
      lcd.setCursor(0, 1);

      lcd.print(now.day() / 10);      //Menampilkan Tanggal
      lcd.print(now.day() % 10);
      lcd.print("/");
      lcd.print(now.month() / 10);    //Menampilkan Bulan
      lcd.print(now.month() % 10);
      lcd.print("/");
      lcd.print(now.year());       //Menampilkan Tahun
      lcd.setCursor(11, 1);
      lcd.print(now.hour() / 10);     //Menampilkan Jam
      lcd.print(now.hour() % 10);
      lcd.print(":");
      lcd.print(now.minute() / 10);
      lcd.print(now.minute() % 10);   //Menampilkan Menit

      temperature = bme.readTemperature() + adjTemperature;
      humidity = bme.readHumidity() + adjHumidity;

      //pressure = bme.readPressure() / 100.0F;
      //altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);

      String json = "{\"t\":";
      json += temperature;
      json += ",\"h\":";
      json += humidity;
      json += ",\"j\":";
      json += now.hour();
      json += ",\"m\":";
      json += now.minute();
      json += "}";

      webSocket->broadcastTXT(json.c_str(), json.length());

      lcd.setCursor(0, 0);
      lcd.print("T=");
      lcd.print(temperature);
      lcd.setCursor(9, 0);
      lcd.print("H=");
      lcd.print(humidity);

      sign = now.minute();
      if (sign != lastSign)postData(temperature, humidity);
      lastSign = sign;

      oldTime = millis();
    }

  }

}

void postData(float temp, float humidity)
{

  http.begin(destination);   //Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header
  String temps = String(temp);
  String humiditys = String(humidity);
  String dataPost = "mac=" + MAC + "&temperature=" + temps + "&humidity=" + humiditys;
  int httpCode = http.POST(dataPost);
  String payload = http.getString();  //Get the response payload
  http.end();   //Close connection
  if (payload == " 1")fail = 0;
  else fail = 2;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {

    String dataRec = (const char *) &payload[0];

    if (dataRec == "re") {
      String dataEEPROM = read_String(10);
      webSocket->broadcastTXT(dataEEPROM.c_str(), dataEEPROM.length());
    }

    else if (dataRec == "read-adj") {
//      Serial.println("read adj requested");
      String dataRead2 = read_String(2000);
//      StaticJsonBuffer<200> jsonBuffer;
//      
//      JsonObject& root2 = jsonBuffer.parseObject(dataRead2);
//
//      adjHumidity = root2["ah"];
//      adjTemperature = root2["at"];
//
//      Serial.print("adjHumidity = ");
//      Serial.println(adjHumidity);
//      Serial.print("adjTemperature = ");
//      Serial.println(adjTemperature );
       webSocket->broadcastTXT(dataRead2.c_str(), dataRead2.length());
    }

    else {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(dataRec);

      Serial.println(dataRec);

      const char* s = root["s"];
      if (s) {

        Serial.println("write SSID data");

        String ssidreceived = root["s"];
        String passwordreceived = root["p"];
        String destinationreceived = root["d"];

        writeString(10, dataRec);

        String json = read_String(10);
        webSocket->broadcastTXT(json.c_str(), json.length());

        if (dataRec == json) {
          String resp = "{\"resp\":\"Ok\"}";
          webSocket->broadcastTXT(resp.c_str(), resp.length());
        }
        else {
          String resp = "{\"resp\":\"Fail\"}";
          webSocket->broadcastTXT(resp.c_str(), resp.length());
        }
      }

      const char* dy = root["dy"];

      if (dy) {
        int dy = root["dy"];
        int dm = root["dm"];
        int dd = root["dd"];
        int dh = root["dh"];
        int di = root["di"];
        rtc.adjust(DateTime(dy, dm, dd, dh, di, 0));
        Serial.println("write Time data");
      }

      const char* ah = root["ah"];

      if (ah) {
        int ahum = root["ah"];
        int atemp = root["at"];
        Serial.println("write Adjust data");
        Serial.print("Humidity : ");
        Serial.print(ahum);
        Serial.print(" Temperature : ");
        Serial.println(atemp);

        writeString(2000, dataRec);
        Serial.println("write Adjust data finished");

        adjHumidity = ahum;
        adjTemperature = atemp;

      }

    }

  }

}


void writeString(char add, String data)
{
  int _size = data.length();
  int i;
  for (i = 0; i < _size; i++)
  {
    EEPROM.write(add + i, data[i]);
  }
  EEPROM.write(add + _size, '\0'); //Add termination null character for String Data
  EEPROM.commit();
}


String read_String(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len = 0;
  unsigned char k;
  k = EEPROM.read(add);
  while (k != '\0' && len < 500) //Read until null character
  {
    k = EEPROM.read(add + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

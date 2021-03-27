#include "secrets.h" //Remember to rename secrets_orig.h and add your network/certificates info
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <M5StickC.h>
#include <EEPROM.h>
#include "WiFi.h"
#include "time.h"

#define AWS_MAX_RECONNECT_TRIES 10
#define AWS_IOT_PUBLISH_TOPIC   "M5StickC-PowerDetector/pub" //AWS IoT publish topic
#define AWS_IOT_SUBSCRIBE_TOPIC "M5StickC-PowerDetector/sub" //AWS IoT subscribe topic

const char* ntpServer = "pool.ntp.org";
const long  secGMTOffset = 7200; //Set your timezone offset - Israel is GMT+2 or 7200 sec
const int   secDaylightOffset = 3600; //Set your daylight offset

long loopTime, startTime = 0;
char localTime[32] = "";
int color;
time_t localStamp;
uint8_t sleepCount = 0;
uint8_t wifiCount = 0;
int printDelay = 500;
int checkDelay = 15000; //Set time between power checks
time_t lastError;
char lastErrorString[32];
long lastErrorSec;
char powerStatus[8] = "NA"; 

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

void LCD_Clear() 
{
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(2);
}

void connectWiFi()
{
    // Connect to WiFi
    M5.Lcd.printf("Connecting to %s ", ssid);
    delay(1000);
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        M5.Lcd.printf(".");
        // Serial.print(".");
        if (wifiCount > 20) 
        {
            M5.Axp.DeepSleep(SLEEP_SEC(5));
        }
        wifiCount++;
    }

    M5.Lcd.println("\n\rCONNECTED!");
}

void getLocalTime()
{
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    strftime(localTime,32,"%d-%m-%Y %H:%M:%S",&timeinfo);
    localStamp = mktime(&timeinfo);
}

void resetEEPROM(){
    for (int i = 0; i < 128; i++) 
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}

void powerLoss_detected() 
{    
    sleepCount++;
    LCD_Clear();
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 2, 2);
    M5.Lcd.setTextColor(WHITE, RED);
    M5.Lcd.printf("**ERROR**\n\rPOWER DOWN\n\r");
    M5.Lcd.println(sleepCount);

    getLocalTime();

    Serial.println(powerStatus);
    if (strcmp(powerStatus, "ERROR") != 0) 
    {
        strncpy( powerStatus, "ERROR", sizeof(powerStatus) );
        EEPROM.writeInt(0, localStamp);
        EEPROM.commit();
        connectAWS();
        publishMessage();
    }
    delay(2000);
    
    if(sleepCount >= 5) //sleep device after 5 * [checkDelay] msec
    {
        sleepCount = 0;
        M5.Axp.DeepSleep(SLEEP_SEC(60 * 5)); //time to wakeup
    }
}
    
void connectAWS()
{
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
    client.begin(AWS_IOT_ENDPOINT, 8883, net);

    // Create a message handler
    client.onMessage(messageHandler);

    int retries = 0;
    Serial.println("Connecting to AWS IOT");

    while (!client.connect(THINGNAME) && retries < AWS_MAX_RECONNECT_TRIES) 
    {
        Serial.print(".");
        delay(100);
        retries++;
    }

    if(!client.connected())
    {
        Serial.println("AWS IoT Timeout!");
        M5.Lcd.println("AWS IoT Timeout!");
        return;
    }

    // Subscribe to a topic
    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    Serial.println("AWS IoT Connected!");
    M5.Lcd.println("AWS IoT Connected!");
    delay(2000);
}

void publishMessage()
{
    StaticJsonDocument<200> doc;
    doc["sms"] = "+972547999502";
    doc["message"] = "Ziniman Home Power Down";
    //strcat(doc["message"],lastErrorString);
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer); // print to client

    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

    Serial.println("Published alert to AWS IoT");
}

void messageHandler(String &topic, String &payload) 
{
    Serial.println("incoming: " + topic + " - " + payload);

    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    const char* message = doc["message"];

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextFont(2);
    M5.Lcd.println(message);
    delay(5000);
}

void setup() 
{
    M5.begin();
    M5.Lcd.setRotation(3);
    LCD_Clear();
    
    connectWiFi();

    // Init and get the time
    configTime(secGMTOffset, secDaylightOffset, ntpServer);
    getLocalTime();
    M5.Lcd.println(localTime);
    delay(2000);

    EEPROM.begin(512);
    lastError = EEPROM.readInt(0);
    Serial.println(lastError);
    
    if(M5.Axp.GetVBusVoltage()<4)
    {
        strncpy( powerStatus, "ERROR", sizeof(powerStatus) );
    }

}

void loop() 
{
    loopTime = millis();
    
    if(startTime < (loopTime - checkDelay)) //Check power status every [checkDelay] msec
    {
        if(M5.Axp.GetVBusVoltage()<4)
        {
            powerLoss_detected();
        }
        else
        {
            strncpy( powerStatus, "OK", sizeof(powerStatus) );
            sleepCount = 0;
            
            if (lastError>0)
            {
                LCD_Clear();
                M5.Lcd.setCursor(0, 2, 2);
                M5.Lcd.setTextColor(WHITE, RED);
                M5.Lcd.printf("Last power down:\n\r");
                struct tm  ts;
                lastError = EEPROM.readInt(0);
                time_t default_time = lastError;
                ts = *localtime(&default_time);
                strftime(lastErrorString, 32, "%d-%m-%Y %H:%M:%S", &ts);
                M5.Lcd.printf("%s\n\r", lastErrorString);
                //M5.Lcd.printf("(%.2f seconds)", difftime(localStamp, lastError)); 
                lastErrorSec = difftime(localStamp, lastError);
                M5.Lcd.printf("(%02.0fH:%02.0fM:%02.0fS)", floor(lastErrorSec/3600.0), floor(fmod(lastErrorSec,3600.0)/60.0), fmod(lastErrorSec,60.0));
                Serial.printf("[DEBUG] Last Error %s\r\n", lastErrorString);
                Serial.printf("%02.0fH:%02.0fM:%02.0fS\r\n", floor(lastErrorSec/3600.0), floor(fmod(lastErrorSec,3600.0)/60.0), fmod(lastErrorSec,60.0));
                delay(5000);    
            }
        }
        startTime = loopTime;
    }
    LCD_Clear();
    M5.Lcd.setCursor(0, 0, 1);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextColor(WHITE, BLUE);
    M5.Lcd.printf("Power Status - %s\n\r", powerStatus);
    getLocalTime();
    M5.Lcd.println(localTime);
    M5.Lcd.printf("Battery: %.2fV\r\n", M5.Axp.GetBatVoltage());
    M5.Lcd.printf("aps: %.2fV\r\n", M5.Axp.GetAPSVoltage());
    //M5.Lcd.printf("Warning level: %d\r\n", M5.Axp.GetWarningLevel());
    M5.Lcd.printf("USB in: V:%.2fV I:%.2fma\r\n", M5.Axp.GetVBusVoltage(), M5.Axp.GetVBusCurrent());
    delay(printDelay);
}
  
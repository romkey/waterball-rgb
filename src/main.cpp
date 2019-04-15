#include <Arduino.h>

#include <Esp.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Ticker.h>

#include "config.h"

#include <ArduinoOTA.h>

WiFiMulti wifiMulti;

#include "tcs34725_sensor.h"
TCS34725_Sensor tcs34725(MQTT_UPDATE_FREQUENCY, 0, 0, false);

#include "tsl2561_sensor.h"
TSL2561_Sensor tsl2561(MQTT_UPDATE_FREQUENCY, 0, 0, false);

#include "uptime.h"
Uptime uptime;

#include <PubSubClient.h>
static WiFiClient wifi_mqtt_client;
void mqtt_callback(char* topic, byte* payload, unsigned int length);

static PubSubClient mqtt_client(MQTT_SERVER, MQTT_PORT, mqtt_callback, wifi_mqtt_client);

#include <IFTTTWebhook.h>
IFTTTWebhook ifttt(IFTTT_API_KEY, IFTTT_EVENT_NAME);

static Ticker update_mqtt;
static Ticker update_heartbeat;
static bool update_mqtt_flag = true;
static bool update_heartbeat_flag = true;

static char hostname[sizeof(WATERBALL_HOSTNAME) + 9];


#include <rom/rtc.h>

const char* reboot_reason(int code) {
  switch(code) {
    case 1 : return "POWERON_RESET";          /**<1, Vbat power on reset*/
    case 3 : return "SW_RESET";               /**<3, Software reset digital core*/
    case 4 : return "OWDT_RESET";             /**<4, Legacy watch dog reset digital core*/
    case 5 : return "DEEPSLEEP_RESET";        /**<5, Deep Sleep reset digital core*/
    case 6 : return "SDIO_RESET";             /**<6, Reset by SLC module, reset digital core*/
    case 7 : return "TG0WDT_SYS_RESET";       /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : return "TG1WDT_SYS_RESET";       /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : return "RTCWDT_SYS_RESET";       /**<9, RTC Watch dog Reset digital core*/
    case 10 : return "INTRUSION_RESET";       /**<10, Instrusion tested to reset CPU*/
    case 11 : return "TGWDT_CPU_RESET";       /**<11, Time Group reset CPU*/
    case 12 : return "SW_CPU_RESET";          /**<12, Software reset CPU*/
    case 13 : return "RTCWDT_CPU_RESET";      /**<13, RTC Watch dog Reset CPU*/
    case 14 : return "EXT_CPU_RESET";         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : return "RTCWDT_BROWN_OUT_RESET";/**<15, Reset when the vdd voltage is not stable*/
    case 16 : return "RTCWDT_RTC_RESET";      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : return "NO_MEAN";
  }
}
  
void setup() {
  byte mac_address[6];

  delay(500);

  Serial.begin(115200);
  Serial.println("Hello World!");

  wifiMulti.addAP(WIFI_SSID1, WIFI_PASSWORD1);
  wifiMulti.addAP(WIFI_SSID2, WIFI_PASSWORD2);
  wifiMulti.addAP(WIFI_SSID3, WIFI_PASSWORD3);

  WiFi.macAddress(mac_address);
  snprintf(hostname, sizeof(hostname), "%s-%02x%02x%02x", WATERBALL_HOSTNAME, (int)mac_address[3], (int)mac_address[4], (int)mac_address[5]);
  Serial.printf("Hostname is %s\n", hostname);

  WiFi.setHostname(hostname);
  while(wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("[wifi]");

  ifttt.trigger("reboot", reboot_reason(rtc_get_reset_reason(0)),  reboot_reason(rtc_get_reset_reason(1)));
  Serial.println("[IFTTT]");

  if(!MDNS.begin(hostname))
    Serial.println("Error setting up MDNS responder!");

  Serial.println("[mDNS]");

  mqtt_client.connect(MQTT_SERVER, MQTT_USERNAME, MQTT_PASSWORD);
  Serial.println("[MQTT]");

  configTime(TZ_OFFSET, DST_OFFSET, "pool.ntp.org", "time.nist.gov");
  Serial.println("[NTP]");

#ifdef ESP32
   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
#endif

  ArduinoOTA.begin();
  Serial.println("[ota]");

  tcs34725.begin();
  Serial.println("[tcs34725]");

  tsl2561.begin();
  Serial.println("[tsl2561]");

  update_mqtt.attach(MQTT_UPDATE_FREQUENCY, []() { update_mqtt_flag = true; });
  update_heartbeat.attach(HEARTBEAT_UPDATE_FREQUENCY, []() { update_heartbeat_flag = true; });

  delay(500);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) { }

void loop() {
  if(!mqtt_client.connected())
    mqtt_client.connect(hostname, MQTT_USERNAME, MQTT_PASSWORD);

  mqtt_client.loop();

  ArduinoOTA.handle();


  if(update_mqtt_flag) {
    update_mqtt_flag = false;

    tcs34725.handle();

    uint16_t red = -1, green = -1, blue = -1;
    uint16_t lux = -1;
    uint16_t lux2 = -1, ir = -1, full = -1, visible = -1;

    tcs34725.handle();

    red = tcs34725.red();
    green = tcs34725.green();
    blue = tcs34725.blue();
    lux = tcs34725.lux();

    tsl2561.handle();
    lux2 = tsl2561.lux();
    full = tsl2561.full();
    visible = tsl2561.visible();
    ir = tsl2561.ir();

    Serial.printf("lux %d, red %d, green %d, blue %d\n", lux, red, green, blue);
    Serial.printf("lux2 %d, full %d, visible %d, ir %d\n", lux2, full, visible, ir);
 
#define BUF_SIZE 4 + 4*5 + 11 + 1
    char buf[BUF_SIZE];

    snprintf(buf, BUF_SIZE, "%hd %hd %hd %hd %lu %hd %hd %hd %hd", red, green, blue, lux, time(NULL), lux2, full, visible, ir);
    Serial.println(buf);
    mqtt_client.publish(MQTT_TOPIC, buf, true);
  }

  if(update_heartbeat_flag) {
    update_heartbeat_flag = false;

#define HEARTBEAT_BUFFER_SIZE 256
    char buf[HEARTBEAT_BUFFER_SIZE];
    IPAddress local_ip = WiFi.localIP();
    byte mac_address[6];

    WiFi.macAddress(mac_address);

    snprintf(buf, HEARTBEAT_BUFFER_SIZE, "{ \"hostname\": \"%s\", \"ip\": \"%d.%d.%d.%d\", \"mac_address\": \"%02x:%02x:%02x:%02x:%02x:%02x\", \"freeheap\": %d, \"uptime\": %lu, \"timestamp\": %lu }",
	     hostname,
	     local_ip[0], local_ip[1], local_ip[2], local_ip[3],
	     mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
	     ESP.getFreeHeap(), uptime.uptime()/1000,
	     time(NULL));

    Serial.printf("heartbeat %s\n", buf);

    if(!mqtt_client.publish("/heartbeat", buf))
      Serial.println("heartbeat publish failed");
  }
}




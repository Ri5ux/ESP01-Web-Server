#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <AsyncEventSource.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>
#include <SPIFFSEditor.h>
#include <FS.h>

#include <Wire.h>

int connectionTime = 0;
File file;
char mac[18];
boolean tryConnection = false;

String ssid = "";
String pswd = "";

AsyncWebServer server(80);

void setup() {
  Serial.begin(74880);
  Serial.println("ESP-01 Script Initialized");

  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting serial peripheral interface flash file system(SPIFFS).");
    return;
  }
  
  setMAC();
  Serial.print("MAC: ");
  Serial.println(mac);
  
  Serial.print("Connection Mode: ");
  Serial.println(wifi_mode_to_string(WiFi.getMode()));
  Serial.print("Connection Status: ");
  Serial.println(wl_status_to_string(WiFi.status()));
  Serial.println();

  if (WiFi.status() != WL_CONNECTED && strlen(WiFi.SSID().c_str()) > 0) {
    connectToNetwork();
  } else if (WiFi.status() != WL_CONNECTED && strlen(WiFi.SSID().c_str()) == 0) {
    listNetworks();
  }
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil(' ');
    Serial.read();
    
    if (command == "connect") {
      ssid = Serial.readStringUntil(' ');
      Serial.read(); 
      pswd = Serial.readStringUntil('\n');
      connectToNetwork();
    }
    if (command == "disconnect") {
      WiFi.disconnect();
      Serial.println("Disconnected from the wiresless network.");
    }
    if (command == "list_networks") {
      listNetworks();
    }
    if (command == "reset") {
      hardwareReset();
    }
    if (command == "i2c_scan") {
      Wire.begin(12, 14);
      check_if_exist_I2C();
    }
  }
}

void connectToNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pswd.c_str());
  WiFi.setAutoConnect(true);
  Serial.print("Connecting to wireless access point with SSID ");
  Serial.print(WiFi.SSID().c_str());
  Serial.print(" ");
  while (WiFi.status() != WL_CONNECTED && connectionTime < 60) {
    delay(500);
    Serial.print(".");
    connectionTime++;
  }

  if (connectionTime >= 30) { Serial.println("ERROR"); Serial.println("Connection timed out. Access point may be unavailable."); connectionTime = 0; }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("OK");
    Serial.println();
    Serial.println("Network Configuration ");
    Serial.println("=============================================");
    Serial.print("Hostname: ");
    Serial.println(WiFi.hostname());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Gataway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("DNS: ");
    WiFi.dnsIP().printTo(Serial);
    Serial.println();

    startWebServer();
  }
  else
  {
    listNetworks();
  }
}

/** I2C Serial **/

uint8_t portArray[] = {0, 2, 14, 12, 13};
String portMap[] = {"GPIO0", "GPIO2", "GPIO14", "GPIO12", "GPIO13"};

void scanPorts() { 
  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j){
        Serial.print("Scanning (SDA : SCL) - " + portMap[i] + " : " + portMap[j] + " - ");
        Wire.begin(portArray[i], portArray[j]);
        check_if_exist_I2C();
      }
    }
  }
}

void check_if_exist_I2C() {
  byte error, address;
  int nDevices = 0;
  
  for (address = 1; address < 127; address++ )  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission(); 

    if (error == 0){
      Serial.print("I2C device found at address 0x");
      
      if (address < 16) {
        Serial.print("0");
      }
      
      Serial.print(address, HEX);
      Serial.println(" !");

      nDevices++;
    } else if (error == 4) {
      //Serial.print("Unknow error at address 0x");
      
      if (address < 16) {
        //Serial.print("0");
      }
      
      //Serial.println(address, HEX);
    }
  }
  
  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.println("**********************************\n");
  }
}

/** Web Server **/

void startWebServer() {
  server.serveStatic("/", SPIFFS, "/web/html").setDefaultFile("index.html").setTemplateProcessor(processor);
  server.serveStatic("/client", SPIFFS, "/web/client");
  
  server.on("/wireless_signal", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(WiFi.RSSI()));
  });
   
  server.begin();
}

String processor(const String& var)
{
  if(var == "ESPVAL_MAC"){ return String(mac); }
  if(var == "ESPVAL_HOST"){ return String(WiFi.hostname()); }
  if(var == "ESPVAL_IP"){ return String(WiFi.localIP().toString().c_str()); }
  if(var == "ESPVAL_GATEWAY"){ return String(WiFi.gatewayIP().toString().c_str()); }
  if(var == "ESPVAL_SUBNET"){ return String(WiFi.subnetMask().toString().c_str()); }
  if(var == "ESPVAL_DNS"){ return String(WiFi.dnsIP().toString().c_str()); }
  if(var == "ESPVAL_RSSI"){ return String(WiFi.RSSI()); }
  if(var == "ESPVAL_BSSID"){ return String(WiFi.BSSIDstr()); }
  if(var == "ESPVAL_SSID"){ return String(WiFi.SSID().c_str()); }
  if(var == "ESPVAL_HTML_NAVIGATION"){
    String data = readFile("/web/html/navigation.html");
    data.replace("%ESP_IP%", WiFi.localIP().toString().c_str());
    
    return data;
  }
  
  return String();
}

String readFile(const String& filePath) {
  if (SPIFFS.exists(filePath)){
    File f = SPIFFS.open(filePath, "r");
    if (f && f.size()) {
      String data;
        
      while (f.available()){
        data += char(f.read());
      }
      f.close();
      return String(data);
    }
  } else {
    Serial.print("ERROR: File not found: ");
    Serial.println(filePath);
  }
  return String();
}

/** Wireless Networking **/

void hardwareReset() {
  Serial.println("Resetting...");
  ESP.eraseConfig();
  WiFi.disconnect();
  delay(3000);
  Serial.println("Device reset.");
}

void setMAC() {
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(mac, "%2X:%2X:%2X:%2X:%2X:%2X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void listNetworks() {
  Serial.print("Scanning for wireless networks...");
  int numSsid = WiFi.scanNetworks();
  
  if (numSsid == -1) {
    Serial.println("No wireless networks in range.");
    while (true);
  }

  Serial.print(numSsid);
  Serial.println(" wireless networks in range.");
  Serial.println();

  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm\t");
    Serial.println(WiFi.SSID(thisNet));
  }
}

const char* wl_status_to_string(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
  }
}

const char* wifi_mode_to_string(int id) {
  switch (id) {
    case 0: return "WIFI_OFF";
    case 1: return "WIFI_STA";
    case 2: return "WIFI_AP";
    case 3: return "WIFI_AP_STA";
  }
}

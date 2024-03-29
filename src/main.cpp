/*
* Copyright 2023 Constantin Dumitru Petre RĂDULICEA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*   http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <vector>

#define ENABLE_SERIAL_PRINT 0
#define ENABLE_WIFI_HOST_DATA_FROM_CLIENT 1
#define ENABLE_INIT_SEQUENCE 1
#define ESCAPED_CHARACTER_AT_BEGINNING_OF_STRING '%'
#define MAX_BUFFER_SIZE 16384
#define INIT_SEQUENCE "%SERIAL2WIFI\r\n"


#define LOWPOWER_MODE_TIMEOUT_ACTIVATION_MS 1000
#define LOWPOWER_MODE_LOOP_DELAY_MS 100


String wifi_ssid = "Off Limits2";
String wifi_password = "J7s2tzvzKzva";
String client_hostname = "192.168.79.243";
int client_port = 6789;

/*
SERIAL2WIFI
Off Limits
J7s2tzvzKzva
192.168.0.227
6789
*/

void removeLastCarrerReturn(String &str){
  if (str.charAt(str.length()-1) == '\r')
  {
    str.remove(str.length() - 1, 1);
  }
}

void escapeFrontCommentCharacter(String &str, char commentCharacter){
  if (str.charAt(0) == commentCharacter)
  {
    str.remove(0, 1);
  }
}

String serialReadStringUntil_blocking(HardwareSerial& serialPort, char terminatorCharacter){
  char tempChar;
  String result = "";
  while (serialPort)
  {
    while (serialPort.available() > 0){
      tempChar = (char)serialPort.read();
      if (tempChar == terminatorCharacter) {
        return result;
      }
      result += tempChar;
    }
  }
  return result;
}

void setup() {
  String initializationTemp = "";
  bool initializationStringFound = false;
  char tempChar;

  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  while (!Serial){
    delay(100);
  }
  
  #if ENABLE_SERIAL_PRINT == 1
    while (Serial.availableForWrite() <= 0){}
    Serial.println("Serial port connected");
  #endif

  #if ENABLE_INIT_SEQUENCE == 1
    while (initializationStringFound == false)
    {
      while (Serial.available() > 0){
        tempChar = (char)Serial.read();
        initializationTemp += tempChar;
        #if ENABLE_SERIAL_PRINT == 1
          Serial.print(tempChar);
        #endif

        if (initializationTemp.length() > strlen(INIT_SEQUENCE)) {
          initializationTemp.remove(0, initializationTemp.length() - (strlen(INIT_SEQUENCE)));
        }
        if (initializationTemp.length() == strlen(INIT_SEQUENCE)) {
          if (strncmp(initializationTemp.c_str(), INIT_SEQUENCE, strlen(INIT_SEQUENCE)) == 0) {
            initializationStringFound = true;
            #if ENABLE_SERIAL_PRINT == 1
              Serial.println();
              Serial.println("Initialization sequence completed!");
            #endif
            break;
          }
        }
      }
    }
  #endif
  
  #if ENABLE_WIFI_HOST_DATA_FROM_CLIENT == 1
    while (Serial.available() <= 0){}
    wifi_ssid = serialReadStringUntil_blocking(Serial, '\n');
    removeLastCarrerReturn(wifi_ssid);
    escapeFrontCommentCharacter(wifi_ssid, ESCAPED_CHARACTER_AT_BEGINNING_OF_STRING);
    #if ENABLE_SERIAL_PRINT == 1
      Serial.println("SSID: " + wifi_ssid);
    #endif

    while (Serial.available() <= 0){}
    wifi_password = serialReadStringUntil_blocking(Serial, '\n');
    removeLastCarrerReturn(wifi_password);
    escapeFrontCommentCharacter(wifi_password, ESCAPED_CHARACTER_AT_BEGINNING_OF_STRING);
    #if ENABLE_SERIAL_PRINT == 1
      Serial.println("Password: " + wifi_password);
    #endif
    

    while (Serial.available() <= 0){}
    client_hostname = serialReadStringUntil_blocking(Serial, '\n');
    removeLastCarrerReturn(client_hostname);
    escapeFrontCommentCharacter(client_hostname, ESCAPED_CHARACTER_AT_BEGINNING_OF_STRING);
    #if ENABLE_SERIAL_PRINT == 1
      Serial.println("Hostname: " + client_hostname);
    #endif

    while (Serial.available() <= 0){}
    String temp_port_str = serialReadStringUntil_blocking(Serial, '\n');
    removeLastCarrerReturn(temp_port_str);
    escapeFrontCommentCharacter(temp_port_str, ESCAPED_CHARACTER_AT_BEGINNING_OF_STRING);
    
    client_port = temp_port_str.toInt();
    #if ENABLE_SERIAL_PRINT == 1
      Serial.println("Port: " + String(client_port));
    #endif
  #endif

  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
      
    #if ENABLE_SERIAL_PRINT == 1
      Serial.print(".");
    #endif
  }
  #if ENABLE_SERIAL_PRINT == 1
    Serial.println("WiFi connected");
    Serial.print("ipv4: ");
    Serial.println(WiFi.localIP());
  #endif
}

static void connectToServer(WiFiClient& server, String hostName, uint16_t port){
  while (1)
  {
    if (!server.connect(hostName, port)) {
      #if ENABLE_SERIAL_PRINT == 1
        Serial.println("Connection failed!");
      #endif
      delay(100);
      continue;
    }
    else{
      break;
    }
  }
}

void sendDataToServer(WiFiClient& server, std::vector<char>& buffer, String hostName, uint16_t port){
  size_t bytesWrittenNow;

    if (buffer.size() <= 0) {
      return;
    }
  if (!server.connected()) {
    connectToServer(server, hostName, port);
  }
  bytesWrittenNow = server.write(buffer.data(), buffer.size());
  buffer.erase(buffer.begin(),buffer.begin()+bytesWrittenNow);
}

void readDataFromServer(WiFiClient& server, std::vector<char>& buffer, String hostName, uint16_t port){
  if (!server.connected()) {
    connectToServer(server, hostName, port);
  }

  size_t bytesRead, bytesAvailable;
  if (server.available() > 0)
  {
    bytesAvailable = (size_t)server.available();

    if ((buffer.size() + bytesAvailable) > MAX_BUFFER_SIZE) {
      buffer.erase(buffer.begin(),buffer.begin()+((buffer.size() + bytesAvailable) - MAX_BUFFER_SIZE));
    }
    
    buffer.resize(buffer.size() + bytesAvailable);
    bytesRead = server.readBytes(buffer.data() + buffer.size() - bytesAvailable, bytesAvailable);
    buffer.resize((int)buffer.size() - (int)((int)bytesAvailable - (int)bytesRead));
  }
}

void readDataFromSerial(HardwareSerial& serialPort, std::vector<char>& buffer){
  size_t bytesAvailableSerial, bytesReadFromSerial;
    if (serialPort.available() > 0)
    {
      bytesAvailableSerial = serialPort.available();

      if ((buffer.size() + bytesAvailableSerial) > MAX_BUFFER_SIZE) {
        buffer.erase(buffer.begin(),buffer.begin()+((buffer.size() + bytesAvailableSerial) - MAX_BUFFER_SIZE));
      }

      buffer.resize(buffer.size() + bytesAvailableSerial);
      bytesReadFromSerial = serialPort.readBytes(buffer.data() + buffer.size() - bytesAvailableSerial, bytesAvailableSerial);
      buffer.resize((int)buffer.size() - (int)((int)bytesAvailableSerial - (int)bytesReadFromSerial));
    }
}

void loop() {
  bool activeLowPowerMode = false;
  bool activityInLastIteration = false;
  unsigned int startTime;
  float inactivityTime_ms = 0.0f;
  WiFiClient server;
  std::vector<char> TXbuffer, RXbuffer;

  TXbuffer.clear();
  RXbuffer.clear();
  TXbuffer.reserve(MAX_BUFFER_SIZE);
  RXbuffer.reserve(MAX_BUFFER_SIZE);

  startTime = millis();
  connectToServer(server, client_hostname, client_port);
  while (1)
  {
    if (activityInLastIteration == true)
    {
      startTime = millis();
    }
    activityInLastIteration = false;

    if (activeLowPowerMode) {
      delay(LOWPOWER_MODE_LOOP_DELAY_MS);
    }

    readDataFromSerial(Serial, TXbuffer);
    if (TXbuffer.size() > 0)
    {
      activityInLastIteration = true;
      #if ENABLE_SERIAL_PRINT == 1
        Serial.print("To server [" + String(TXbuffer.size()) + "]: ");
        Serial.write(TXbuffer.data(), TXbuffer.size());
        Serial.println();
      #endif
      sendDataToServer(server, TXbuffer, client_hostname, client_port);
    }

    readDataFromServer(server, RXbuffer, client_hostname, client_port);

    if (RXbuffer.size() > 0)
    {
      activityInLastIteration = true;
      #if ENABLE_SERIAL_PRINT == 1
        Serial.print("From server: ");
      #endif
      RXbuffer.erase(RXbuffer.begin(),RXbuffer.begin() + Serial.write(RXbuffer.data(), RXbuffer.size()));
      #if ENABLE_SERIAL_PRINT == 1
        Serial.println();
      #endif
    }
    if (activityInLastIteration == false)
    {
      inactivityTime_ms = (float)millis() - (float)startTime;
      #if ENABLE_SERIAL_PRINT == 1
        Serial.print("Inactivity time[ms]: ");
        Serial.println(inactivityTime_ms);
      #endif
      if (inactivityTime_ms >= (float)LOWPOWER_MODE_TIMEOUT_ACTIVATION_MS) {
        activeLowPowerMode = true;
        #if ENABLE_SERIAL_PRINT == 1
          Serial.println("LowPowerMode: ENABLED");
        #endif
      }
    }
    else{
      inactivityTime_ms = 0;
      activeLowPowerMode = false;
    }
  }
}

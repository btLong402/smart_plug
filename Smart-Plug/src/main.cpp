#include <Arduino.h>
#include <Payload.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>

#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <string>
#include <cctype> // for toupper
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include "ACS712.h"

// Data wire is plugged into pin D3 on the ESP8266
#define ONE_WIRE_BUS D3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor1(&oneWire);
// current
ACS712 sensor2(ACS712_05B, D7);

#define ONE_SECOND 1000
#define ONE_MINUTE 60 * ONE_SECOND
#define ONE_HOUR 60 * ONE_MINUTE
// put global variable declarations here:
WiFiClient client;
PubSubClient mqtt_client(client);
ESP8266WebServer server(80);
const int r1 = D1;
const int r2 = D2;
char ssid[64];
char pass[64];
const char mqtt_sever[] = "192.168.50.72";
const int port = 1883;
String userName = "";
String socket1 = "Socket_1";
String socket2 = "Socket_2";
const char *equipmentId = "82876453-ae65-11ee-935a-bce92f7b38bc";
String firstPayload = "";
bool isFirstMSG = true;
String webPage = R"(
  <!DOCTYPE html>
<html lang="en">
    <head>
        <!-- Design by foolishdeveloper.com -->
        <title>Glassmorphism login Form Tutorial in html css</title>
        <!--Stylesheet-->
        <style media="screen">
      *,
*:before,
*:after{
    padding: 0;
    margin: 0;
    box-sizing: border-box;
}
body{
    background-color: #080710;
}
.background{
    width: 430px;
    height: 520px;
    position: absolute;
    transform: translate(-50%,-50%);
    left: 50%;
    top: 50%;
}
.background .shape{
    height: 200px;
    width: 200px;
    position: absolute;
    border-radius: 50%;
}
.shape:first-child{
    background: linear-gradient(
        #1845ad,
        #23a2f6
    );
    left: -80px;
    top: -80px;
}
.shape:last-child{
    background: linear-gradient(
        to right,
        #ff512f,
        #f09819
    );
    right: -30px;
    bottom: -80px;
}
form{
    height: 720px;
    width: 400px;
    background-color: rgba(255,255,255,0.13);
    position: absolute;
    transform: translate(-50%,-50%);
    top: 50%;
    left: 50%;
    border-radius: 10px;
    backdrop-filter: blur(10px);
    border: 2px solid rgba(255,255,255,0.1);
    box-shadow: 0 0 40px rgba(8,7,16,0.6);
    padding: 50px 35px;
}
form *{
    font-family: 'Poppins',sans-serif;
    color: #ffffff;
    letter-spacing: 0.5px;
    outline: none;
    border: none;
}
form h3{
    font-size: 32px;
    font-weight: 500;
    line-height: 42px;
    text-align: center;
    margin-top: 10px;
}

label{
    display: block;
    margin-top: 30px;
    font-size: 16px;
    font-weight: 500;
}
input{
    display: block;
    height: 50px;
    width: 100%;
    background-color: rgba(255,255,255,0.07);
    border-radius: 3px;
    padding: 0 10px;
    margin-top: 8px;
    font-size: 14px;
    font-weight: 300;
}
::placeholder{
    color: #e5e5e5;
}
button{
    margin-top: 50px;
    width: 100%;
    background-color: #ffffff;
    color: #080710;
    padding: 15px 0;
    font-size: 18px;
    font-weight: 600;
    border-radius: 5px;
    cursor: pointer;
}

    </style>
        <script>
    function submitForm() {
      var wifiName = document.getElementById("wifiName").value;
      var wifiPassword = document.getElementById("wifiPassword").value;
      var _username = document.getElementById("username").value;
      var _password = document.getElementById("password").value;

      // Send an HTTP request to the server with the entered values
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/connect?userName=" + _username + "&pass=" + _password + "&ssid=" + wifiName + "&wifiPass=" + wifiPassword, true);

      xhr.send();
    }
</script>
    </head>
    <body>
        <div class="background">
            <div class="shape"></div>
            <div class="shape"></div>
        </div>
        <form>
            <h3>Login Here</h3>

            <label for="username">Username</label>
            <input type="text" placeholder="User name" id="username">

            <label for="password">Password</label>
            <input type="password" placeholder="Password" id="password">
            <h3>WiFi Setup</h3>

            <label for="wifiName">WiFi Name:</label>
            <input type="text" id="wifiName" name="wifiName"
                placeholder="WiFi name" required>
            <label for="wifiPassword">Password:</label>
            <input type="password" id="wifiPassword" placeholder="WiFi password"
                name="wifiPassword" required>
            <button onclick="submitForm() ">Submit</button>
        </form>
    </body>
</html>

)";

// put function declarations here:
void callBack(char *topic, byte *payload, unsigned int length);
void toR1(byte *payload);
void toR2(byte *payload);
void setupAP();
void setupMQTT();
void reconnect();
void timeSetRemote(TimeSetPayload payload);
void countDown(int type, float time);
void remote(RemotePayload payload);
void handle(byte *payload);
int connectWifi();
String fPayload(String userName, String password);
void handleConnect();
void handleLogin();
void getLogin();
void login();
bool wifiConnected = false;

void handleRoot()
{
  server.send(200, "text/html", webPage);
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(r1, OUTPUT);
  digitalWrite(r1, HIGH);
  pinMode(r2, OUTPUT);
  digitalWrite(r2, HIGH);
  setupAP();
  setupMQTT();
  sensor1.begin();
  sensor2.calibrate();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED && strcmp(ssid, "") == 0)
  {
    if (WiFi.softAPIP())
      server.handleClient();
    else
    {
      setupAP();
      server.handleClient();
    }
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    connectWifi();
  }
  else
  {
    if (!mqtt_client.connected())
    {
      reconnect();
    }
    mqtt_client.loop();
    // unsigned long now = millis();
    // mqtt_client.publish("from-client", firstPayload.c_str());

    String tp = userName + '\\' + equipmentId;
    mqtt_client.subscribe(tp.c_str());
  }
  // put your main code here, to run repeatedly:
  sensor1.requestTemperatures();
  // get current temperature and current
  float temperature = sensor1.getTempCByIndex(0);
  float current = sensor2.getCurrentDC();
  
  delay(2000);

}

// put function definitions here:
void callBack(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  std::string topicStr = topic;
  if (topicStr == "CONFIG")
  {
    String tp = userName + '\\' + equipmentId;
    mqtt_client.subscribe(tp.c_str());
  }
  else
  {
    handle(payload);
  }
}

void setupAP()
{
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(3000);
  WiFi.mode(WIFI_AP);
  while (WiFi.softAP("ESP8266 WiFi", "12345678") == false)
  {
    Serial.print(".");
    delay(300);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/connect", HTTP_GET, handleConnect);
  server.begin();
}

void handleConnect()
{
  String ssidParam = server.arg("ssid");
  String passParam = server.arg("wifiPass");
  String userParam = server.arg("userName");
  String _passParam = server.arg("pass");
  ssidParam.toCharArray(ssid, sizeof(ssid));
  passParam.toCharArray(pass, sizeof(pass));
  userName = userParam;
  firstPayload = fPayload(userParam, _passParam);
  server.send(200, "text/plain", "Credentials updated! Reconnecting...");
  wifiConnected = connectWifi() == WL_CONNECTED;
}
int connectWifi()
{
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  Serial.println(pass);
  int c = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (c < 30)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      randomSeed(micros());
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      return WL_CONNECTED;
    }
    delay(500);
    Serial.print(".");
    c++;
  }

  Serial.println();
  Serial.println("Connect timed out!");

  return WL_CONNECT_FAILED;
}
void setupMQTT()
{
  mqtt_client.setServer(mqtt_sever, port);
  mqtt_client.setCallback(callBack);
}

void reconnect()
{
  while (!mqtt_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqtt_client.connect(equipmentId))
    {
      if (isFirstMSG)
      {
        mqtt_client.publish("from-client", firstPayload.c_str());
        isFirstMSG = false;
      }
      Serial.println("connected");
      String tp = userName + '\\' + equipmentId;
      mqtt_client.subscribe(tp.c_str());
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void remote(RemotePayload payload)
{
  if (payload.getDeviceName() == socket1)
  {
    String a = socket1 + '\\' + payload.getData();
    Serial.println(a);
    digitalWrite(r1, payload.getData());
  }
  else
  {
    String a = socket2 + '\\' + payload.getData();
    Serial.println(a);
    digitalWrite(r2, payload.getData());
  }
}
void handle(byte *payload)
{
  String payloadStr = String((char *)payload);
  Payload action = Payload::fromJson(payloadStr);
  if (action.getAction() == "REMOTE")
  {
    RemotePayload remotePayload = RemotePayload::fromJson(payloadStr);
    remote(remotePayload);
  }
  else if (action.getAction() == "SET_TIME")
  {
    std::vector<RemotePayload> listRemote = RemotePayload::fromListJson(payloadStr);
    for (unsigned int i = 0; i < listRemote.size(); i++)
    {
      remote(listRemote[i]);
    }
  }
}

String fPayload(String _userName, String password)
{
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["userName"] = userName;
  jsonDoc["password"] = password;
  jsonDoc["equipmentID"] = equipmentId;

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  return jsonString;
}
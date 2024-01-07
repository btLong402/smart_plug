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
const char *clientId = "smart_plug_1";
unsigned long lastMsg = 0;
enum Topic
{
  ON = 0,
  OFF = 1,
  TO_R1,
  TO_R2,
};
String webPage = R"(<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8">
    <title>WiFi Setup</title>
    <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      background-color: #f4f4f4;
    }
    .container {
      width: 300px;
      padding: 20px;
      background-color: #fff;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
    }
    .form-group {
      margin-bottom: 15px;
    }
    label {
      display: block;
      margin-bottom: 5px;
      font-weight: bold;
    }
    input[type="text"],
    input[type="password"] {
      width: calc(100% - 12px);
      padding: 6px;
      border: 1px solid #ccc;
      border-radius: 4px;
      font-size: 16px;
    }
    input[type="submit"] {
      width: 100%;
      padding: 10px;
      border: none;
      border-radius: 4px;
      font-size: 16px;
      background-color: #007bff;
      color: #fff;
      cursor: pointer;
    }
    input[type="submit"]:hover {
      background-color: #0056b3;
    }
  </style>
    <script>
    function submitForm() {
      var wifiName = document.getElementById("wifiName").value;
      var wifiPassword = document.getElementById("wifiPassword").value;

      // Send an HTTP request to the server with the entered values
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/connect?ssid=" + wifiName + "&pass=" + wifiPassword, true);
      xhr.send();
    }
  </script>
  </head>
  <body>
    <div class="container">
      <h2>WiFi Setup</h2>
      <form id="wifiForm">
        <div class="form-group">
          <label for="wifiName">WiFi Name:</label>
          <input type="text" id="wifiName" name="wifiName" required>
        </div>
        <div class="form-group">
          <label for="wifiPassword">Password:</label>
          <input type="password" id="wifiPassword" name="wifiPassword" required>
        </div>
        <input type="submit" value="Connect" onclick="submitForm() ">
      </form>
    </div>
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
void handle(char *topic, byte *payload);
int connectWifi();
void handleRoot()
{
  server.send(200, "text/html", webPage);
}

void handleConnect()
{
  String ssidParam = server.arg("ssid");
  String passParam = server.arg("pass");

  ssidParam.toCharArray(ssid, sizeof(ssid));
  passParam.toCharArray(pass, sizeof(pass));
  connectWifi();
  server.send(200, "text/plain", "Credentials updated!");
}
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(r1, OUTPUT);
  digitalWrite(r1, HIGH);
  pinMode(r2, OUTPUT);
  digitalWrite(r2, HIGH);
  setupMQTT();
  setupAP();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (WiFi.softAPIP())
      server.handleClient();
    else
    {
      setupAP();
      server.handleClient();
    }
  }
  else
  {
    if (!mqtt_client.connected())
    {
      reconnect();
    }
    mqtt_client.loop();
    // unsigned long now = millis();
    mqtt_client.subscribe("user_name/equ_id/to_r1");
  }
  // put your main code here, to run repeatedly:
}

// put function definitions here:
void callBack(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  std::string topicStr = topic;

  if (topicStr == "to_r1")
  {
    toR1(payload);
  }
  else if (topicStr == "to_r2")
  {
    toR2(payload);
  }
  else
  {
    Serial.println("Unknown Topic");
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
int connectWifi()
{
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  int c = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (c < 20)
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
  mqtt_client.subscribe("to_r1");
  mqtt_client.subscribe("to_r2");
  mqtt_client.publish("eqId", "userName");
}

void reconnect()
{
  while (!mqtt_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqtt_client.connect(clientId))
    {
      Serial.println("connected");
      mqtt_client.subscribe("to_r1");
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

void toR1(byte *payload)
{
  int tmp = static_cast<int>((char)payload[0]) - '0';
  switch (tmp)
  {
  case ON:
    Serial.println("R1 On");
    digitalWrite(r1, HIGH);
    break;
  case OFF:
    Serial.println("R1 Off");
    digitalWrite(r1, LOW);
    break;
  default:
    printf("Unknow code\n");
    break;
  }
}

void toR2(byte *payload)
{
  int tmp = static_cast<int>((char)payload[0]) - '0';
  switch (tmp)
  {
  case ON:
    digitalWrite(r1, HIGH);
    break;
  case OFF:
    digitalWrite(r1, LOW);
    break;
  default:
    printf("Unknow code\n");
    break;
  }
}

void remote(RemotePayload payload)
{
  if (payload.getDeviceName() == "R1")
  {
    digitalWrite(r1, payload.getData());
  }
  else
  {
    digitalWrite(r2, payload.getData());
  }
}
void handle(char *topic, byte *payload)
{
  String payloadStr = String((char *)payload);
  if (strcmp(topic, "REMOTE") == 0)
  {
    RemotePayload remotePayload = RemotePayload::fromJson(payloadStr);
    remote(remotePayload);
  }
  else if (strcmp(topic, "SET_TIME") == 0)
  {
    TimeSetPayload TimeSetPayload = TimeSetPayload::fromJson(payloadStr);
    timeSetRemote(TimeSetPayload);
  }
}

void timeSetRemote(TimeSetPayload payload)
{
  countDown(payload.getType(), payload.getTime());
  if (payload.getDeviceName() == "R1")
  {
    digitalWrite(r1, payload.getData());
  }
  else
  {
    digitalWrite(r2, payload.getData());
  }
}

void countDown(int type, float time)
{
  long tmp;
  switch (type)
  {
  case 1:
    tmp = (long)(time * ONE_MINUTE);
    delay(tmp);
    break;
  case 2:
    tmp = (long)(time * ONE_HOUR);
    delay(tmp);
  default:
    delay(time * ONE_SECOND);
    break;
  }
}
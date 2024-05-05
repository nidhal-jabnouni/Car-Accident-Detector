#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "TinyGPSPlus.h"
#include <iostream>

#define RXD2 16
#define TXD2 17

char gpsLattitude[15], gpsLongitude[15];
TinyGPSPlus gps;


Adafruit_MPU6050 m_p_u;

float maxNormalDec = -3.47;
//100km/h to 0 in 8s = 3.47m/s
//or 125km/h to 0 in 10s
//maxNormalDec: anything slower isn't a detectable accident
float minNormalDec = maxNormalDec * 1.2;
//setting a higher limit of what's detectable is dangerous
//150 to 0 in 10s
float DecelLimit = maxNormalDec;

const char* mqttServer = "broker.hivemq.com";//"z96e4d99.us-east-1.emqx.cloud";
const int mqttPort = 1883;

const char* mqttClient = "projet";

WiFiClient espClient;
PubSubClient client(espClient);

char *TopicButton = (char*)malloc(50);
char *TopicUserPhone = (char*)malloc(50);
char *TopicGuardian = (char*)malloc(50);
char *TopicSpeed = (char*)malloc(50);
char *UserName = (char*)malloc(50);
char *GuardianName = (char*)malloc(50);
char* TmpSsid = (char*)malloc(50);
char* TmpPassword = (char*)malloc(50);
char* ssid = (char*)malloc(50); //AP for Internet Access
char* password = (char*)malloc(50);

int Wifi_ReConnect = 0;
boolean isButtonPushed = false;
boolean isSpeedChanged = false;
boolean isAlertEnabled = true;
boolean insideChangeWifiPassword = false;
boolean insideSetDecelLimit = false;
boolean isWifiChanged = false;
boolean changedSsid = false;
boolean changedPassword = false;
float x, y, z;

void setup() {

  //Topics
  strcpy (TopicButton, mqttClient);
  strcat (TopicButton, "/Button");

  strcpy (TopicUserPhone, mqttClient);
  strcat (TopicUserPhone, "/UserPhone");

  strcpy (TopicGuardian, mqttClient);
  strcat (TopicGuardian, "/Guardian");

  strcpy (TopicSpeed, mqttClient);
  strcat (TopicSpeed, "/Speed");

  strcpy(ssid, "Wokwi-GUEST"); //AP for Internet Access
  strcpy(password, "");

  strcpy(UserName, "nidhal");
  strcpy(GuardianName, "rahma");


  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(2000);
  Serial.println("-- Application launched ---- ");
  delay(2000);

  if (!m_p_u.begin()) {
    while (1) {
      Serial.println("!m_p_u.begin()");
      delay(1000);
    }
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  connectmqtttopic();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Wifi_ReConnect++;
    Serial.print(Wifi_ReConnect);
    Serial.println (" Reconnexions ");
    Serial.println("WIFI CONNECTION LOST ... WAITING");
    reconnect();
  }

  delay(400);
  client.loop();
  delay(400);
  //maybe needs a delay after
  if (isAlertEnabled) {
    detectAccident();
    delay(20);
  }

}

void connectmqtttopic() {

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqttClient )) {//in this case no user, no password are needed
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  //maybe u can subscribe to multiple
  if (client.subscribe(TopicButton)) {
    Serial.print ("Suscribed to Topic: ");
    Serial.println(TopicButton);
  }
  if (client.subscribe(TopicUserPhone)) {
    Serial.print ("Suscribed to Topic: ");
    Serial.println(TopicUserPhone);
  }
  if (client.subscribe(TopicSpeed)) {
    Serial.print ("Suscribed to Topic: ");
    Serial.println(TopicSpeed);
  }
  if (client.subscribe(TopicGuardian)) {
    Serial.print ("Suscribed to Topic: ");
    Serial.println(TopicGuardian);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  char topicpl;
  //change to verify whether he's okay or not

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    topicpl = (char)payload[i];
  }
  String allpayload = String( (char*) payload);
  String thepayload = allpayload.substring(0, length);
  Serial.println(thepayload);

  if (thepayload.equals("yes") && strcmp(topic, TopicButton) == 0)  {
    Serial.println("Button pushed");
    isButtonPushed = true;
  }

  if (thepayload.equals("speed") && strcmp(topic, TopicUserPhone) == 0)  {
    setDecelLimit();
  }
  if (thepayload.equals("change wifi") && strcmp(topic, TopicUserPhone) == 0)  {
    changeWifiPassword();
  }
  if (thepayload.equals("enable alert") && strcmp(topic, TopicUserPhone) == 0)  {
    isAlertEnabled = true;
    client.publish(TopicUserPhone, "alert is enabled");
  }
  if (thepayload.startsWith("name=") && strcmp(topic, TopicUserPhone) == 0) {
    // Extract the new username from the payload
    String newUsername = thepayload.substring(5); // Skip "name="
    strcpy(UserName, newUsername.c_str());

    char *msg = (char*)malloc(100);
    strcpy(msg, "UserName updated to: ");
    strcat(msg, UserName);
    client.publish(TopicUserPhone, msg);
    free(msg);
  }

  if (thepayload.startsWith("name=") && strcmp(topic, TopicGuardian) == 0) {
    // Extract the new name from the payload
    String newGuardianname = thepayload.substring(5); // Skip "name="
    strcpy(GuardianName, newGuardianname.c_str());

    char *msg = (char*)malloc(100);
    strcpy(msg, "GuardianName updated to: ");
    strcat(msg, GuardianName);
    client.publish(TopicGuardian, msg);
    free(msg);
  }
  if (thepayload.startsWith("ssid=") && strcmp(topic, TopicUserPhone) == 0 && insideChangeWifiPassword) {

    strcpy(TmpSsid, thepayload.substring(5).c_str()); // Skip "ssid="
    char *msg = (char*)malloc(100);
    strcpy(msg, "TmpSsid updated to: ");
    strcat(msg, TmpSsid);
    client.publish(TopicUserPhone, msg);
    changedSsid = true;
    free(msg);
  }
  if (thepayload.startsWith("password=") && strcmp(topic, TopicUserPhone) == 0 && insideChangeWifiPassword) {

    strcpy(TmpPassword, thepayload.substring(9).c_str());// Skip "password="
    char *msg = (char*)malloc(100);
    strcpy(msg, "TmpPassword updated to: ");
    strcat(msg, TmpPassword);
    client.publish(TopicUserPhone, msg);
    changedPassword = true;
    free(msg);
  }

  if (thepayload.startsWith("connect") && (strcmp(topic, TopicUserPhone) == 0) && insideChangeWifiPassword && changedPassword && changedSsid) {

    WiFi.begin(TmpSsid, TmpPassword);
    int tries = 0;

    while (WiFi.status() != WL_CONNECTED && tries < 20)
    {
      tries++;
      Serial.println("WAITING WIFI CONNECTION .....");
      delay(1000);
    }
    if (tries < 20) {
      connectmqtttopic();
      strcpy(ssid, TmpSsid);
      strcpy(password, TmpPassword);
    } else {
      reconnect();
      client.publish(TopicUserPhone, "failed to connect to new wifi");
    }
    isWifiChanged = true;
  }

  if ((strcmp(topic, TopicSpeed) == 0) && (isNumeric(thepayload.c_str())) && (insideSetDecelLimit)) {
    int tmp = 0;
    for (int i = 0; i < length; i++) {
      tmp = 10 * tmp + (int)payload[i] - 48;
    }
    if ((tmp >= 125) && (tmp <= 150)) {
      DecelLimit = (-tmp * 1000 / 3600) / 10;
      client.publish(TopicSpeed, "DecelLimit updated");
      isSpeedChanged = true;
    } else {
      client.publish(TopicSpeed, "limit not in range");
    }
  }

  Serial.println();
  Serial.println("-----------------------");
}


void reconnect() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WAITING WIFI CONNECTION .....");
    delay(1000);
  }
  connectmqtttopic();
}

float getAccel() {
  sensors_event_t acc;
  sensors_event_t dummy_gcc, dummy_temp;  // Dummy variables

  m_p_u.getEvent(&acc, &dummy_gcc, &dummy_temp);
  //global variables
  x = acc.acceleration.x;
  y = acc.acceleration.y;
  z = acc.acceleration.z - 1;

  return sqrt(x * x + y * y + z * z);
}

void detectAccident() {
  float a1, a1x, a1y, a1z, a2, a2x, a2y, a2z;
  a1 = getAccel();
  a1x = x;
  a1y = y;
  a1z = z;
  //should be close to 0 or positive
  delay(500);
  a2 = getAccel();
  a2x = x;
  a2y = y;
  a2z = z;
  //should be really negative
  float boundry = 0.2 * abs(maxNormalDec);
  boolean isYZOutOfBound = (a2y * a2y + a2z * a2z > 0.4 * DecelLimit * DecelLimit);

  if ((a1 < boundry) || (a1x > 0 && a1y > 0 && a1z > 0 ) ) { //|| isYZOutOfBound
    if ((-a2 < DecelLimit) && (a2x < DecelLimit || isYZOutOfBound)) {
      sendUserAlert();//it can be sent or the button was pushed
      client.publish(TopicUserPhone, "The alert will be disabled until you re-enable it.");
      isAlertEnabled = false;

    }
  }

}

void sendUserAlert() {
  //publish u ok?
  isButtonPushed = false;
  client.publish(TopicUserPhone, "Are you okay? You have 60s to confirm.");
  delay(400);
  client.loop();
  delay(10000);//60000
  client.loop();
  delay(400);

  if (!isButtonPushed) {
    sendGuardianAlert();
  } else {
    client.publish(TopicUserPhone, "button was pushed");
  }
}



void sendGuardianAlert() {
  //publish to guardian

  char *msg = (char*)malloc(100);

  strcpy(msg, "Dear ");
  strcat(msg, GuardianName); strcat(msg, ", ");
  strcat(msg, UserName);
  strcat(msg, " had an accident\n");
  strcat(msg, getGpsData());
  client.publish(TopicGuardian, msg);
  free(msg);
  delay(400);
  client.loop();
  delay(400);

}

void setDecelLimit() {
  insideSetDecelLimit = true;
  isSpeedChanged = false;
  client.publish(TopicSpeed, R"(What's the deceleration you want?
   it should be between 150km/h to 0 in 10s
   or 125km/h to 0 in 10s)");
  while (!isSpeedChanged) {
    delay(400);
    client.loop();
    delay(400);
  }
  insideSetDecelLimit = false;
}


void changeWifiPassword() {
  insideChangeWifiPassword = true;
  isWifiChanged = false;
  client.publish(TopicUserPhone, R"(What's the new wifi?
   publish  ssid=******
   publish  password=****
   then publish connect)");
  while (!isWifiChanged) {
    delay(400);
    client.loop();
    delay(400);
  }
  insideChangeWifiPassword = false;

}




char* getGpsData() {
  char *result = (char*)malloc(50);
  if (readGpsData(5, 2000) == 1) {
    // Reading GPS Location
    if (gps.location.isValid()) {
      dtostrf(gps.location.lat(), 8, 6, gpsLattitude);
      dtostrf(gps.location.lng(), 8, 6, gpsLongitude);
      strcpy(result, "Latitude: ");
      strcat(result, gpsLattitude);
      strcat(result, "\n");
      strcat(result, "Longitude: ");
      strcat(result, gpsLongitude);
      strcat(result, "\n");
    } else {
      Serial.println(F("INVALID LOCATION"));
    }
  } else {
    Serial.println("NO DATA AVAILABLE FROM SERIAL COM");
  }
  return result;
}

int readGpsData(uint8_t TrialNbr, unsigned long rdinterval) {
  uint8_t ValidState = 0;

  // Flushing Serial before reading
  while (Serial2.available() == 0);
  while (Serial2.available()) {
    char Cflush = Serial2.read();
  }
  while (ValidState == 0 && TrialNbr-- > 0) {
    unsigned long start = millis();
    do {
      while (Serial2.available())   gps.encode(Serial2.read());
    } while (millis() - start < rdinterval);

    if (gps.charsProcessed() < 10) {
      ValidState = 0;
    } else
      ValidState = 1;
  }
  return ValidState;
}

bool isNumeric(const char* str) {
  for (int i = 0; str[i] != '\0'; i++) {
    if (!isdigit(str[i])) {
      return false;
    }
  }
  return true;
}

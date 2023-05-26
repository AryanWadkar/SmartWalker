#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <ThingSpeak.h>

//IO
#define BUTTON_PIN 0  
#define LED 2

//Constants
const unsigned long pairing_interval = 1000UL;
const unsigned long setup_interval = 200UL;

// IO/state variables
int buttonState = 0;        
int lastButtonState = 0;
int flag=0;
unsigned long pairing_previousMillis = 0UL;
unsigned long setup_previousMillis = 0UL;
unsigned long livedata_previousMillis = 0UL;
int ledState = LOW;
int weblaunched=0;
int i = 0;
int statusCode;
String sside;
String passe;
String emaile;
String phonee;
String st;
String content;

// Fall detection variables
const int MPU_addr=0x68;  // I2C address of the MPU-6050
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
float ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
boolean fall = false; //stores if a fall has occurred
boolean trigger1=false; //stores if first trigger (lower threshold) has occurred
boolean trigger2=false; //stores if second trigger (upper threshold) has occurred
boolean trigger3=false; //stores if third trigger (orientation change) has occurred
byte trigger1count=0; //stores the counts past since trigger 1 was set true
byte trigger2count=0; //stores the counts past since trigger 2 was set true
byte trigger3count=0; //stores the counts past since trigger 3 was set true
int angleChange=0;

//IFTTT data
const char *host = "maker.ifttt.com";
const char *privateKey = "dnoquirO130gY3uNI9FjE";

//Function Declaration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);

//Server declaration
ESP8266WebServer server(80);
WiFiClient client;


void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  WiFi.disconnect();
  EEPROM.begin(512);
  delay(10);
  Serial.println("Startup");
  ThingSpeak.begin(client);
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
 
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi())
  {
    Serial.println("Succesfully Connected!!!");
    lastButtonState = 1;
    weblaunched=0;
    initfalldet();
    return;
  }
  else
  {
    Serial.println("Turning the HotSpot On");
    blink_setup();    
    launchWeb();
    weblaunched=1;
    setupAP();
  }

  Serial.println();
  Serial.println("Waiting.");
  
  while ((WiFi.status() != WL_CONNECTED))
  {
    Serial.print(".");
    delay(100);
    server.handleClient();
  }

}


void loop() {
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == 0) {
    if(flag==0)
    {
      flag=1;
      if (lastButtonState ==0) {
        lastButtonState=1;
      } else {
        lastButtonState=0;
      }
    }
  }else{
    flag=0;
    if(lastButtonState==1){
      if(weblaunched==1)
      {
        initfalldet();
      }
      weblaunched=0;
      //Serial.println("in operation mode");
      blink_pairing();
      mpu_read();
      fall_detect();
    }else{
      blink_setup();
      if(weblaunched==0)
      {
        WiFi.disconnect();
        Serial.println("in setup mode");
        Serial.println("Turning the HotSpot On");
        launchWeb();
        weblaunched=1;
        setupAP();
      }
      server.handleClient();
    }
  }
}

bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for WiFi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connection timed out, opening AP or Hotspot");
  return false;
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void createWebServer()
{
    {
      server.on("/", []() {
  
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        scanwifi();
        readwificred();
        readcontact();        
        content= "{\"ipstr\":\"";
        content+=ipStr;
        content+="\",\"st\":";
        content+=st;
        content+=",\"chipID\":\"";
        content+=ESP.getChipId();
        content+="\",\"wifi\":\"";
        content+=sside;
        content+="\",\"email\":\"";
        content+=emaile;
        content+="\",\"phone\":\"";
        content+=phonee;
        content+="\"}";
        /*
        content = "<!DOCTYPE HTML>\r\n<html>ESP8266 WiFi Connectivity Setup ";
        content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
        content += ipStr;
        content += "<p>";
        content += st;
        content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
        content += "</html>";
        */
        server.send(200, "application/json", content);
      });
      server.on("/contactsetting", []() {
        String email = server.arg("email");
        String phone = server.arg("phone");
        if (email.length() > 0 && phone.length() > 0) {
          Serial.println("clearing eeprom");
          for (int i = 96; i < 192; ++i) {
            EEPROM.write(i, 0);
          }  
          Serial.println("writing eeprom ssid:");
          for (int i = 96; i < 128; ++i)
          {
            EEPROM.write(i, phone[i-96]);
            Serial.print("Wrote: ");
            Serial.println(phone[i-96]);
          }
          Serial.println("writing eeprom pass:");
          for (int i = 128; i < 192; ++i)
          {
            EEPROM.write(i, email[i-128]);
            Serial.print("Wrote: ");
            Serial.println(email[i-128]);
          }
          Serial.println("commiting to eeprom");
          EEPROM.commit();
  
          content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
          statusCode = 200;
          
        } else {
          content = "{\"Error\":\"404 not found\"}";
          statusCode = 404;
          Serial.println("Sending 404");
        }
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(statusCode, "application/json", content);
        delay(3000);
        if(statusCode==200)
        {
          ESP.reset();
        }  
      });
  
      server.on("/wifisetting", []() {
        String qsid = server.arg("ssid");
        String qpass = server.arg("pass");
        if (qsid.length() > 0 && qpass.length() > 0) {
          Serial.println("clearing eeprom");
          for (int i = 0; i < 96; ++i) {
            EEPROM.write(i, 0);
          }
          Serial.println(qsid);
          Serial.println("");
          Serial.println(qpass);
          Serial.println("");
  
          Serial.println("writing eeprom ssid:");
          for (int i = 0; i < qsid.length(); ++i)
          {
            EEPROM.write(i, qsid[i]);
            Serial.print("Wrote: ");
            Serial.println(qsid[i]);
          }
          Serial.println("writing eeprom pass:");
          for (int i = 0; i < qpass.length(); ++i)
          {
            EEPROM.write(32 + i, qpass[i]);
            Serial.print("Wrote: ");
            Serial.println(qpass[i]);
          }
          EEPROM.commit();
  
          content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
          statusCode = 200;
          
        } else {
          content = "{\"Error\":\"404 not found\"}";
          statusCode = 404;
          Serial.println("Sending 404");
        }
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(statusCode, "application/json", content);
        delay(1000);
        if(statusCode==200)
        {
          ESP.reset();
        }
      });
  }
}

void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  scanwifi();
  delay(100);
  WiFi.softAP("Intelligent-Walker", "");
  Serial.println("Initializing_Wifi_accesspoint");
  launchWeb();
  Serial.println("over");
}

void scanwifi(){
  int n = WiFi.scanNetworks();
  Serial.println("scan completed");
  if (n == 0)
    Serial.println("No WiFi Networks found");
  else
  {
    Serial.print(n);
    Serial.println(" Networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "[";
  for (int i = 0; i < n-1; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "{\"name\":\"";
    st += WiFi.SSID(i);
    st += "\",\"strength\":";
    st += WiFi.RSSI(i);
    st += "},";
  }
    st += "{\"name\":\"";
    st += WiFi.SSID(n-1);
    st += "\",\"strength\":";
    st += WiFi.RSSI(n-1);
    st += "}";  
  st += "]";
}

void initfalldet(){
 Wire.begin();
 Wire.beginTransmission(MPU_addr);
 Wire.write(0x6B);  // PWR_MGMT_1 register
 Wire.write(0);     // set to zero (wakes up the MPU-6050)
 Wire.endTransmission(true);
}

void mpu_read(){
 Wire.beginTransmission(MPU_addr);
 Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
 Wire.endTransmission(false);
 Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
 AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
 AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
 AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
 Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
 GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
 GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
 GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void send_data(int Amp){
  const char * myWriteAPIKey = "LFVW2F8JMVLS1K3S";
  unsigned long myChannelNumber = 2128374;
  unsigned long currentMillis = millis();
  if (currentMillis - livedata_previousMillis >= 100) {   
  livedata_previousMillis=currentMillis;
  ThingSpeak.writeField(myChannelNumber, 1,String(Amp), myWriteAPIKey);
  }
}

void fall_detect(){
 ax = (AcX-2050)/16384.00;
 ay = (AcY-77)/16384.00;
 az = (AcZ-1947)/16384.00;
 gx = (GyX+270)/131.07;
 gy = (GyY-351)/131.07;
 gz = (GyZ+136)/131.07;
 float Raw_Amp = pow(pow(ax,2)+pow(ay,2)+pow(az,2),0.5);
 int Amp = Raw_Amp * 10; 
 //send_data(Amp);
 Serial.println(AcX);
 Serial.println(AcY);
 Serial.println(AcZ);
 if((AcX==0 && AcY==0 && AcZ==0) || (AcX==-1 && AcY==-1 && AcZ==-1))
 {
   initfalldet();
 }else{
 Serial.println("Amp:");  
 Serial.print(Amp);
 if (Amp<=4 && trigger2==false){ //if AM breaks lower threshold (0.4g) // originally 2 // spam = 8
   trigger1=true;
   Serial.println("TRIGGER 1 ACTIVATED");
   }
 if (trigger1==true){
   trigger1count++;
   if (Amp>=8){ //if AM breaks upper threshold (3g) // 12 orignally //spam =0
     trigger2=true;
     Serial.println("TRIGGER 2 ACTIVATED");
     trigger1=false; trigger1count=0;
     }}
 if (trigger2==true){
   trigger2count++;
   angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5); 
   Serial.println("Angle change (2):");
   Serial.print(angleChange);
   if (angleChange>=15 && angleChange<=400){ //if orientation changes by between 80-100 degrees // oringally 30 & 400 //spam =2
     trigger3=true; trigger2=false; trigger2count=0;
     Serial.println(angleChange);
     Serial.println("TRIGGER 3 ACTIVATED");
       }
   }
 if (trigger3==true){
    trigger3count++;
    if (trigger3count>=10){ 
       angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5);
        Serial.println("Angle change (3):");
        Serial.print(angleChange);
       if ((angleChange>=0) && (angleChange<=300)){ //if orientation changes remains between 0-10 degrees // orignally 0 and 10
           fall=true; trigger3=false; trigger3count=0;
           Serial.println(angleChange);
             }
       else{ //user regained normal orientation
          trigger3=false; trigger3count=0;
          Serial.println("TRIGGER 3 DEACTIVATED");
       }
     }
  }
 if (fall==true){ //in event of a fall detection
   Serial.println("FALL DETECTED");
   send_event("fall_detect"); 
   fall=false;
   }
 if (trigger2count>=6){ //allow 0.5s for orientation change
   trigger2=false; trigger2count=0;
   Serial.println("TRIGGER 2 DECACTIVATED");
   }
 if (trigger1count>=6){ //allow 0.5s for AM to break upper threshold
   trigger1=false; trigger1count=0;
   Serial.println("TRIGGER 1 DECACTIVATED");
   }
 }
  delay(100);

}

void send_event(const char *event){
  Serial.print("Connecting to "); 
  Serial.println(host);
    // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("Connection failed");
    return;
  }
    // We now create a URI for the request
  String url = "/trigger/";
  url += event;
  url += "/with/key/";
  url += privateKey;
  readcontact();
  String value3="prototpe";
  url+= "?value1=";
  url+=phonee;
  url+="&value2=";
  url+=emaile;
  url+="&value3=";
  url+=value3;
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  String data = "{";
  data += "\"value1\": \"" + phonee + "\",";
  data += "\"value2\": \"" + emaile + "\",";
  data += "\"value3\": \"" + value3 + "\"";
  data += "}";
  Serial.println(data);
  
  // This will send the request to the server
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" + 
             "Content-Type: application/x-www-form-urlencoded\r\n" +  // Set the content type to x-www-form-urlencoded
             "Connection: close\r\n" +
             "Content-Length: " + data.length() + "\r\n\r\n" +  // Set the content length to the length of the data
             data);
  while(client.connected())
  {
    if(client.available())
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    } else {
      // No data yet, wait a bit
      delay(50);
    };
  }
  Serial.println();
  Serial.println("closing connection");
  client.stop();
}

void blink_pairing(){
  unsigned long currentMillis = millis();

  if (currentMillis - pairing_previousMillis >= pairing_interval) {
    pairing_previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(LED, ledState);
  }
}

void blink_setup(){
  unsigned long currentMillis = millis();

  if (currentMillis - setup_previousMillis >= setup_interval) {
    setup_previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(LED, ledState);
  }
}

void readwificred(){
  sside="";
  passe="";
  for (int i = 0; i < 32; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    sside += char(EEPROM.read(i));
  }
  for (int i = 32; i < 96; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    passe += char(EEPROM.read(i));
  }
}

void readcontact(){
  emaile="";
  phonee="";
    for(int i=96; i<128; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    phonee+=char(EEPROM.read(i));
  }
  for(int i=128; i<192; ++i)
  {
    if(char(EEPROM.read(i))=='\0')
    {
      break;
    }
    emaile+=char(EEPROM.read(i));
  }  
}




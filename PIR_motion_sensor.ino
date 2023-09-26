//---------------------------------Include the librairies----------------------------//

#include <WiFi.h> 
#include <PubSubClient.h>
#include "time.h" 
#include "sntp.h" 

//---------------------------------Variable declaration------------------------------//

//Define constants
#define sensorPin 33
#define ledPin 15
#define port 1883 // MQTT broker port

//Time struct
struct tm timeinfo;
char myHour[3];
char timeWeekDay[10];

//Time server
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe including daylight adjustment rules 

const char* mqtt_server = "yourBroker.com"; // Broker adress

//WiFi 
const char* ssid = "yourSSID"; 
const char* password = "yourPassword"; 

unsigned long timeToSleepHours = 16; 

int counter = 0; 
bool motion = false;

String Payload;

//Data we want to keep through the deep sleep mode
RTC_DATA_ATTR int bootCount = 0;  
RTC_DATA_ATTR int weeklyCounter = 0;

//Objects declaration 
WiFiClient net;
PubSubClient client(net);

//-----------------------------------Functions----------------------------------------//

//Putting the esp32 to sleep
void goToSleep(){
  Serial.println("Going to sleep now");
  Serial.println("Will wake at 9 AM");
  delay(1000);
  esp_deep_sleep_start();
}

//Date & time info
void printLocalTime(){

  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }  
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S"); //Accessing the data, see time.h documentation
}

//WiFi connection 
void WifiSetup(){ 

  WiFi.mode(WIFI_STA); //  Set the esp32 to wifi connect mode 
  WiFi.begin(ssid,password); // Start the connection 
  Serial.print("Connecting to wifi ...");
  while(WiFi.status() != WL_CONNECTED ){ // WL_CONNECTED flag when connection succeeded 
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnection to wifi successfull \n");
}

//MQTT broker connection
void BrokerSetup(){ 

   if (client.connect("espName")) {
      Serial.println("Connection to broker successful");
   }
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if(!client.connect("espName")) {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(2000);      
    }    
  }
  // Subscribe to MQTT topic
      client.subscribe("yourTopic");  
} 

//Callback function, to print the broker data
void callback(char* topic, byte *payload, unsigned int length) {  

    Serial.println("\n-------new message from broker-----");
    Serial.print("channel:");
    Serial.println(topic);
    Serial.print("data:");  
    Serial.write(payload, length);
    
    Payload = ""; // Resets the payload variable so the next for-loop doesn't add on to the old message.
    for (int i = 0; i < length; i++) {
      Payload += (char)payload[i];      
    }
    Serial.println(); 
}

//Reason the esp32 woke up
void print_wakeup_reason(){

  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

//Date
void getWeekDay(){

  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.print("This is the day of the week : ");
  Serial.println(timeWeekDay);
  Serial.println();
}

//Time
void getTime(){

  strftime(myHour,3, "%H", &timeinfo);
  Serial.print("This is the time : ");
  Serial.println(myHour);
  Serial.println();
}

//---------------------------------Setup function------------------------------//

void setup() {

  Serial.begin(9600);
  delay(1000); // Time to open the Serial monitor
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  client.setServer(mqtt_server, port);
  client.setCallback(callback); // set callback function
  WifiSetup(); 
  BrokerSetup();  
  
  configTzTime(time_zone, ntpServer1, ntpServer2);
  esp_sleep_enable_timer_wakeup(3600000000ULL * timeToSleepHours); //How much time the esp32 is going to sleep for 
  printLocalTime();
  getWeekDay();

  pinMode(sensorPin, INPUT);
  pinMode(ledPin, OUTPUT);

  //Reseting the weekly counter every monday 
  if(String(timeWeekDay) == "Monday"){
    weeklyCounter = 0;
  }
  else{
    Serial.println("No reset today");
  }
    
  bootCount ++;
}

//---------------------------------Loop function------------------------------//

void loop() {
  // To maintain connection with the broker 
  client.loop(); 
  
  //Reconnect to the broker if the connection broke
  if (!client.connected()) {
    BrokerSetup();
  }
  
  // Reconnect to the wifi if lost connection 
  if(WiFi.status() != WL_CONNECTED){ 
    Serial.println("Reconnecting to Wifi...");
    WiFi.disconnect();
    WiFi.reconnect();      
  } 

  getTime();

  // Make the esp32 sleep every day after 5PM until 9 AM the next morning
  if(String(myHour)>"17"){ 
    goToSleep();
  }

  if (digitalRead(sensorPin) == HIGH){
    
    //Only count once if the targer keeps moving
    if (motion==false){
      motion=true;
      counter ++;
      weeklyCounter ++;
    }
    
    digitalWrite(ledPin, HIGH);
    client.publish("yourTopic", "Motion detected");    
  } else{

    digitalWrite(ledPin, LOW);
    client.publish("yourTopic", "No motion");
    motion=false;   
    }
    delay(1000);            
  
  Serial.println(counter);
  Serial.println(weeklyCounter);
  client.publish("yourTopic", String(counter).c_str());
  client.publish("yourTopic", String(weeklyCounter).c_str());
} 
 

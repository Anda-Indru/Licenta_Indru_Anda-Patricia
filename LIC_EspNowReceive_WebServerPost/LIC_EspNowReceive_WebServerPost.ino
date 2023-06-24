#include <TimeLib.h>
#include <espnow.h>
#include <WiFiClient.h>
#include "ESPAsyncWebServer.h"
#include <Arduino_JSON.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

//---WI-FI credentials---
const char* ssid = "Wednesday";
const char* password = "anddda26";

//---OpenWeatherMap---
#define API_KEY "2b7085c014222331192ea1fafb48a19e"
#define CITY "Timisoara"
#define COUNTRY_CODE "RO"
#define NR_DAYS_FORCASTED "5"
#define LATITUDE "45.7538355"
#define LONGITUDE "21.2257474"
#define NR_HOURS "14"

String forecastPath = "http://api.openweathermap.org/data/2.5/forecast/daily?lat=" 
                    + String(LATITUDE) + "&lon=" + String(LONGITUDE) + "&cnt=" 
                    + String(NR_DAYS_FORCASTED) + "&appid=" 
                    + String(API_KEY) + "&units=metric";

String hourlyCloudsForecastPath = "http://api.openweathermap.org/data/2.5/forecast?lat=" 
                                + String(LATITUDE) + "&lon=" + String(LONGITUDE) 
                                + "&appid=" + String(API_KEY) + "&cnt=" + String(NR_HOURS);


//---SOLAR PANELS EFFICIENCY---
//(from how much sunlight it gets)
#define LOWEST_PANEL_EF 25
#define HIGHEST_PANEL_EF 100

#define MAX_BATTERY 3.7
#define MIN_BATTERY 2.8

#define CHARGING_MINUTES_100 540

int outputPV[5];
int batteryPercentage;
int chargingTime[5];

unsigned long lastTimeForecast = 0;
unsigned long timerDelayForecast = 30000; //30 sec

struct Station{
  double temp;
  int humid;
  int rain;
  double batteryVoltage; 
} stationData;

struct Forecast{
  unsigned long unixTime[10];
  double minTemp[10];
  double maxTemp[10];
  double dayTemp[10];
  double morningTemp[10];
  double eveningTemp[10];
  double nightTemp[10];
  
  double humid[10];
  double pressure[10];
  double clouds[10];
  double speed[10];
  double pop[10];

  unsigned long risingHour[10];
  unsigned long dawnHour[10];

  String generalState[10];
  
} forecastData;

struct Time{
  String date[10];
  String dayDate[10];
  String day[10];
}timeData;


struct Today{
  String day;
  int hour[14];
  int clouds[14];  
}today;

struct Tomorrow{
  String day;
  int hour[14];
  int clouds[14];  
}tomorrow;

struct DATomorrow{
  String day;
  int hour[14];
  int clouds[14];  
}daTomorrow;


// Structure example to receive data
// Must match the sender structure
struct Message {
  unsigned int readingId;
  
  double temp;
  double humid;
  int rain;
  
  double batteryVoltage; 
  
} structMessage;

JSONVar board;
JSONVar apiForecast;
JSONVar myObject;

AsyncWebServer server(80);
AsyncEventSource events("/events");

// callback when data is received
void OnDataRecv(uint8_t * mac_addr, uint8_t *incomingData, uint8_t len) { 
 
  memcpy(&structMessage, incomingData, sizeof(structMessage));

  board["temperature"] = structMessage.temp;
  board["humidity"] = structMessage.humid;
  board["rain"] = structMessage.rain;
  board["batteryVoltage"] = structMessage.batteryVoltage;
  
  board["readingId"] = structMessage.readingId; 
 
  String jsonString = JSON.stringify(board);
  events.send(jsonString.c_str(), "new_readings", millis());
}

void sendForecastDataToServer(){
  apiForecast["minTemp0"] = forecastData.minTemp[1];
  apiForecast["maxTemp0"] = forecastData.maxTemp[1];
  apiForecast["pop0"] = forecastData.pop[1];
  apiForecast["generalState0"] = forecastData.generalState[0];

  apiForecast["minTemp1"] = forecastData.minTemp[2];
  apiForecast["maxTemp1"] = forecastData.maxTemp[2];
  apiForecast["pop1"] = forecastData.pop[2];
  apiForecast["generalState1"] = forecastData.generalState[1];

  apiForecast["minTemp2"] = forecastData.minTemp[3];
  apiForecast["maxTemp2"] = forecastData.maxTemp[3];
  apiForecast["pop2"] = forecastData.pop[3];
  apiForecast["generalState2"] = forecastData.generalState[2];

  apiForecast["generalState3"] = forecastData.generalState[3];

  String jsonString = JSON.stringify(apiForecast);
  events.send(jsonString.c_str(), "forecast_data", millis());
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
  <title>WEATHER SERVER</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
    integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      text-align: center;
    }

    p {
      font-size: 1.2rem;
    }

    body {
      background-image: url('https://images5.alphacoders.com/456/456536.jpg');
      background-repeat: no-repeat;
      background-attachment: fixed;
      background-size: cover;
      margin: 0;
    }

    .topnav {
      overflow: hidden;
      background-color: rgba(47, 68, 104, 0.5);
      color: white;
    }

    .forecast {
      overflow: hidden;
      background-color: rgba(47, 68, 104, 0.57);
      color: white;
      font-size: 1.5rem;
      margin: 130px;
      margin-top: 10px;
      padding: 0.5%;
      padding-top: 0%;
    }

    .card {
      background-color: white;
      box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
    }

    .cardForecast {
      background-color: rgba(0, 0, 0, 0.7);
      box-shadow: 2px 2px 17px 2px rgba(140, 140, 140, .5);
      color: #fff200;
    }

    .cards-station {
      max-width: 1200px;
      margin: 0 auto;
      display: grid;
      grid-gap: 2rem;
      grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
      margin-top: 10px;
      opacity: .8;
    }

    .cards-forecast {
      max-width: 1200px;
      margin: 0 auto;
      display: grid;
      grid-gap: 2rem;
      grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
    }

    .reading {
      font-size: 2.8rem;
    }

    .packet {
      color: #bebebe;
    }

    .card.temperature {
      color: #fd7e14;
    }

    .card.humidity {
      color: #1b78e2;
    }

    .card.rain {
      color: #1be232;
    }

    .left {
      text-align: right;
      flex: 0 0 45%;
      height: 100px;
    }

    .right {
      text-align: left;
      flex: 1;
      font-size: 200%;
      height: 100px;
    }

    .col {
      float: left;
      width: 50%;
    }

    h2 {
      font-family: "Courier New", monospace;
    }
  </style>
</head>

<body onload=displayDateTime();>

  <div class="topnav">

    <div style="display: flex; margin-bottom: 0%;">
      <div class="left">
        <img width="20%" id="gsTodayImage" src="https://i.postimg.cc/Qd2FrVYg/loading2.png">
      </div>
      <div class="right">
        <h2>Timisoara</h2>
      </div>
    </div>

    <div style="margin-top: 0%;">
      <h3 style="color: #fffcca">

        <span style="font-size: 150%;" id="today"></span><span style="font-size: 0%;" id="todayDate"></span><br>
        <span style="font-size: 200%;" id="currentTime"></span>

      </h3>
    </div>

  </div>

  <div class="content">
    <div class="cards-station">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURA </h4>
        <p><span class="reading"><span id="t"></span> &deg;C</span></p>
        <p class="packet">Reading ID: <span id="idT"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> UMIDITATE </h4>
        <p><span class="reading"><span id="h"></span> &percnt;</span></p>
        <p class="packet">Reading ID: <span id="idH"></span></p>
      </div>
      <div class="card rain">
        <h4><i class="fa fa-umbrella" aria-hidden="true"></i> PLOAIE </h4>
        <p><span class="reading"><span id="r"></span> &percnt;</span></p>
        <p class="packet">Reading ID: <span id="idR"></span></p>
      </div>
    </div>
  </div>

  <div class="forecast">
    <h2>Prognoza</h2>

    <div class="cards-forecast">

      <div class="cardForecast">
        <div class="col">
          <div><img width="100%" id="gsTomorrowImage" src="https://i.postimg.cc/Qd2FrVYg/loading2.png"></div>
        </div>
        <div class="col">
          <div>
            <h2><span id="tomorrow"></span></h2>
          </div>
          <div>
            <p><i class="fas fa-thermometer-half"></i> &nbsp; <span id="max0"></span> / <span id="min0"></span>
              &deg;C</p>
            <p><i class="fa fa-umbrella" aria-hidden="true"></i> &nbsp;<span id="pop0"></span> &percnt;</p>
          </div>
        </div>
      </div>

      <div class="cardForecast">
        <div class="col">
          <div><img width="100%" id="gsDATomorrowImage" src="https://i.postimg.cc/Qd2FrVYg/loading2.png"></div>
        </div>
        <div class="col">
          <div>
            <h2><span id="daTomorrow"></span></h2>
          </div>
          <div>
            <p><i class="fas fa-thermometer-half"></i> &nbsp; <span id="max1"></span> / <span id="min1"></span>
              &deg;C</p>
            <p><i class="fa fa-umbrella" aria-hidden="true"></i> &nbsp;<span id="pop1"></span> &percnt;</p>
          </div>
        </div>
      </div>

      <div class="cardForecast">
        <div class="col">
          <div><img width="100%" id="gsThirdImage" src="https://i.postimg.cc/Qd2FrVYg/loading2.png"></div>
        </div>
        <div class="col">
          <div>
            <h2><span id="third"></span></h2>
          </div>
          <div>
            <p><i class="fas fa-thermometer-half"></i> &nbsp; <span id="max2"></span>  / <span id="min2"></span>
              &deg;C</p>
            <p><i class="fa fa-umbrella" aria-hidden="true"></i> &nbsp;<span id="pop2"></span> &percnt;</p>
          </div>
        </div>
      </div>

    </div>
  </div>

  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');

      source.addEventListener('open', function (e) {
        console.log("Events Connected");
      }, false);
      source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);

      source.addEventListener('message', function (e) {
        console.log("message", e.data);
      }, false);

      source.addEventListener('new_readings', function (e) {
        console.log("new_readings", e.data);
        var obj = JSON.parse(e.data);
        document.getElementById("t").innerHTML = obj.temperature.toFixed(2);
        document.getElementById("h").innerHTML = obj.humidity;
        document.getElementById("r").innerHTML = obj.rain;

        document.getElementById("idT").innerHTML = obj.readingId;
        document.getElementById("idH").innerHTML = obj.readingId;
        document.getElementById("idR").innerHTML = obj.readingId;

      }, false);


      source.addEventListener('forecast_data', function (e) {
        console.log("forecast_data", e.data);
        var obj = JSON.parse(e.data);

        document.getElementById("min0").innerHTML = obj.minTemp0;
        document.getElementById("max0").innerHTML = obj.maxTemp0;
        document.getElementById("pop0").innerHTML = obj.pop0;

        document.getElementById("min1").innerHTML = obj.minTemp1;
        document.getElementById("max1").innerHTML = obj.maxTemp1;
        document.getElementById("pop1").innerHTML = obj.pop1;

        document.getElementById("min2").innerHTML = obj.minTemp2;
        document.getElementById("max2").innerHTML = obj.maxTemp2;
        document.getElementById("pop2").innerHTML = obj.pop2;


        if (obj.generalState0 == "Clear") {
          document.getElementById('gsTodayImage').src = "https://i.postimg.cc/9Mwg8f4R/sun.png";
        } else if (obj.generalState0 == "Rain") {
          document.getElementById('gsTodayImage').src = "https://i.postimg.cc/Njr4vnsS/rain.png";
        } else if (obj.generalState0 == "Clouds") {
          document.getElementById('gsTodayImage').src = "https://i.postimg.cc/HkKDzn35/clouds.png";
        } else if (obj.generalState0 == "Snow") {
          document.getElementById('gsTodayImage').src = "https://i.postimg.cc/668xgd2S/snow.png";
        }
        else {
          window.alert("State not known");
        }

        if (obj.generalState1 == "Clear") {
          document.getElementById('gsTomorrowImage').src = "https://i.postimg.cc/9Mwg8f4R/sun.png";
        } else if (obj.generalState1 == "Rain") {
          document.getElementById('gsTomorrowImage').src = "https://i.postimg.cc/Njr4vnsS/rain.png";
        } else if (obj.generalState1 == "Clouds") {
          document.getElementById('gsTomorrowImage').src = "https://i.postimg.cc/HkKDzn35/clouds.png";
        } else if (obj.generalState1 == "Snow") {
          document.getElementById('gsTomorrowImage').src = "https://i.postimg.cc/668xgd2S/snow.png";
        }
        else {
          window.alert("State not known");
        }

        if (obj.generalState2 == "Clear") {
          document.getElementById('gsDATomorrowImage').src = "https://i.postimg.cc/9Mwg8f4R/sun.png";
        } else if (obj.generalState2 == "Rain") {
          document.getElementById('gsDATomorrowImage').src = "https://i.postimg.cc/Njr4vnsS/rain.png";
        } else if (obj.generalState2 == "Clouds") {
          document.getElementById('gsDATomorrowImage').src = "https://i.postimg.cc/HkKDzn35/clouds.png";
        } else if (obj.generalState2 == "Snow") {
          document.getElementById('gsDATomorrowImage').src = "https://i.postimg.cc/668xgd2S/snow.png";
        }
        else {
          window.alert("State not known");
        }


        if (obj.generalState3 == "Clear") {
          document.getElementById('gsThirdImage').src = "https://i.postimg.cc/9Mwg8f4R/sun.png";
        } else if (obj.generalState3 == "Rain") {
          document.getElementById('gsThirdImage').src = "https://i.postimg.cc/Njr4vnsS/rain.png";
        } else if (obj.generalState3 == "Clouds") {
          document.getElementById('gsThirdImage').src = "https://i.postimg.cc/HkKDzn35/clouds.png";
        } else if (obj.generalState3 == "Snow") {
          document.getElementById('gsThirdImage').src = "https://i.postimg.cc/668xgd2S/snow.png";
        }
        else {
          window.alert("State not known");
        }

      }, false);

      function displayCurrent() {
        var refresh = 1000; // Refresh rate in milli seconds
        mytime = setTimeout('displayDateTime()', refresh)
      }

      function displayDateTime() {
        var days = ['Duminica', 'Luni', 'Marti', 'Miercuri', 'Joi', 'Vineri', 'Sambata'];

        var date = new Date();

        var dd1 = String(date.getDate()).padStart(2, '0');
        var mm1 = String(date.getMonth() + 1).padStart(2, '0'); //January = 0!
        var yyyy1 = date.getFullYear();
        var hh = String(date.getHours());
        var min = String(date.getMinutes());
        var sec = String(date.getSeconds());

        var today = days[date.getDay()];

        var tomorrow, daTomorrow, third;

        var noDays = 7;

        tomorrow = days[(date.getDay() + 1) % noDays];
        daTomorrow = days[(date.getDay() + 2) % noDays];
        third = days[(date.getDay() + 3) % noDays];

        var todayDate = dd1 + '.' + mm1 + '.' + yyyy1 + " ";

        var currentTime;

        if ((min < 10) || (sec < 10)) {
          if (min < 10 && sec < 10) {
            currentTime = hh + ':' + '0' + min + ':' + '0' + sec;
          }
          else if (sec < 10) {
            currentTime = hh + ':' + min + ':' + '0' + sec;
          }
          else if (min < 10) {
            currentTime = hh + ':' + '0' + min + ':' + sec;
          }
        }
        else {
          currentTime = hh + ':' + min + ':' + sec;
        }

        document.getElementById("today").innerHTML = today;
        document.getElementById("tomorrow").innerHTML = tomorrow;
        document.getElementById("daTomorrow").innerHTML = daTomorrow;
        document.getElementById("third").innerHTML = third;
        document.getElementById("todayDate").innerHTML = todayDate;
        document.getElementById("currentTime").innerHTML = currentTime;
        displayCurrent();
      }

    }
  </script>
</body>

</html>)rawliteral";

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Setare rol SLAVE
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  //Inregistrare calback
  esp_now_register_recv_cb(OnDataRecv);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
   
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}

void batteryManagement()
{
  double batteryValue = structMessage.batteryVoltage;
  double discharge;
  int chargingMinutesEfficiency[5];

  //Voltage discharge (V)
  discharge = MAX_BATTERY - batteryValue;

  batteryPercentage = (100 * batteryValue)/MAX_BATTERY;

  for(int i=2; i<=4; i++)
  {
    //percentage of panel efficiency depending on % of clouds in a day
    outputPV[i] = map(forecastData.clouds[i], 0, 100, HIGHEST_PANEL_EF, LOWEST_PANEL_EF);    
  }

  for(int i=0; i<=4; i++)
  {
    Serial.print("---OUTPUT PV---  ");
    Serial.println(i);
    Serial.println(outputPV[i]);
    
    chargingMinutesEfficiency[i] = (CHARGING_MINUTES_100 * HIGHEST_PANEL_EF) / outputPV[i];
    chargingTime[i] = (chargingMinutesEfficiency[i] * discharge)/3.7;

    Serial.println("---chargingMinutesEfficiency---");
    Serial.println(chargingMinutesEfficiency[i]);
    Serial.println("---chargingTime---");
    Serial.println(chargingTime[i]);
  }
}

String convertUnixToDate(unsigned long t)
{
  char date[32];
  sprintf(date, "%02d.%02d.%02d", day(t), month(t), year(t));

  return String(date);
}

String convertUnixToDay(unsigned long t)
{
  String dayName[7] = {"Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata"};

  return dayName[weekday(t) - 1];
}

int getUnixHour(unsigned long t)
{
  return hour(t);
}

String getUnixDateDay(unsigned long t)
{
  return String(day(t));
}


void setTimeData()
{
   for(int i=0; i<=4; i++)
   {
    timeData.date[i] = convertUnixToDate(forecastData.unixTime[i]);
    timeData.dayDate[i] = getUnixDateDay(forecastData.unixTime[i]);
    timeData.day[i] = convertUnixToDay(forecastData.unixTime[i]);
   }
}

void setForecastParameters(JSONVar document)
{
   for(int i=0; i<=4; i++)
      {
        forecastData.unixTime[i] = document["list"][i]["dt"];
        
        forecastData.dayTemp[i] = document["list"][i]["temp"]["day"];
        forecastData.minTemp[i] = document["list"][i]["temp"]["min"];
        forecastData.maxTemp[i] = document["list"][i]["temp"]["max"];
        
        forecastData.morningTemp[i] = document["list"][i]["temp"]["morn"];
        forecastData.eveningTemp[i] = document["list"][i]["temp"]["eve"];
        forecastData.nightTemp[i] = document["list"][i]["temp"]["night"];
        
        forecastData.humid[i] = document["list"][i]["humidity"];
        forecastData.pressure[i] = document["list"][i]["pressure"];
        
        forecastData.clouds[i] = document["list"][i]["clouds"];
        
        forecastData.speed[i] = document["list"][i]["speed"];
        forecastData.speed[i] = round(forecastData.speed[i] * 3.6); // m/s -> km/h
        
        forecastData.pop[i] = document["list"][i]["pop"];
        forecastData.pop[i] = round(forecastData.pop[i] * 100); // %
        
        forecastData.generalState[i] = String(document["list"][i]["weather"][0]["main"]); 

        forecastData.risingHour[i] = document["list"][i]["sunrise"];
        
        forecastData.dawnHour[i] = document["list"][i]["sunset"];
        
      }
}

void setHourlyCloudsParameters()
{
   //Serial.println(myObject);
   int j = 0;
   int z = 0;
   int w = 0;

   int nrOfHoursToday = 0;
   int nrOfHoursTomorrow = 0;

   int effToday = 0;
   int effTomorrow = 0;
   
   for(int i=0; i<=14; i++)
   {
      unsigned long unixDate;
      String date;
      
      unixDate = myObject["list"][i]["dt"]; 
      date = getUnixDateDay(unixDate);
      
      if(date == timeData.dayDate[0])
      {   
        //today
        today.day = date;

        today.hour[j] = getUnixHour(unixDate);
        today.clouds[j] = myObject["list"][i]["clouds"]["all"];

        if(today.hour[j] >= getUnixHour(forecastData.risingHour[0])+3
           && today.hour[j]<= getUnixHour(forecastData.dawnHour[0])+3)
        {
           Serial.print(nrOfHoursToday);
           nrOfHoursToday++;
        }

        Serial.print(today.hour[j]);
        Serial.print("  ...  ");
        Serial.println(today.clouds[j]);

        // day section 1 & day section 3
        // rising -> 8 am & 15 -> dawn
        if((today.hour[j] >= getUnixHour(forecastData.risingHour[0])+3 && today.hour[j] < 8)
            || (today.hour[j] > 15 && today.hour[j] <= getUnixHour(forecastData.dawnHour[0])+3))
        {
          if(today.clouds[j] < 50)
            {
              effToday = effToday + 75;
            }
          else if(today.clouds[j] >= 50 && today.clouds[j] <= 75 )
          {
            effToday = effToday + 50;
          }
          else if(today.clouds[j] > 75)
          {
            effToday = effToday + 25;
          }
        }

        // day section 2 - peak hours
        // 8:00 -> 15:00
        if(today.hour[j] >= 8 && today.hour[j] <= 15)
        {
          if(today.clouds[j] < 50)
            {
              effToday = effToday + 100;
            }
          else if(today.clouds[j] >= 50 && today.clouds[j] <= 75 )
          {
            effToday = effToday + 75;
          }
          else if(today.clouds[j] > 75)
          {
            effToday = effToday + 50;
          }
        }        

        j++;     
      }
      else if(date == timeData.dayDate[1])
      {
        //tomorrow
        
        tomorrow.day = date;

        tomorrow.hour[z] = getUnixHour(unixDate);
        tomorrow.clouds[z] = myObject["list"][i]["clouds"]["all"];

        if(tomorrow.hour[z] >= getUnixHour(forecastData.risingHour[1])+3
           && tomorrow.hour[z]<= getUnixHour(forecastData.dawnHour[1])+3)
        {
           nrOfHoursTomorrow++;
        }

        Serial.print("   TMR  ");
        Serial.print(tomorrow.hour[z]);
        Serial.print("  ...  ");
        Serial.println(tomorrow.clouds[z]);

        // day section 1 & day section 3
        // rising -> 8 am & 15 -> dawn
        if((tomorrow.hour[z] >= getUnixHour(forecastData.risingHour[1])+3 && tomorrow.hour[z] < 8)
            || (tomorrow.hour[z] > 15 && tomorrow.hour[z] <= getUnixHour(forecastData.dawnHour[1])+3))
        {
          if(tomorrow.clouds[z] < 50)
            {
              effTomorrow = effTomorrow + 75;
            }
          else if(tomorrow.clouds[z] >= 50 && tomorrow.clouds[z] <= 75 )
          {
            effTomorrow = effTomorrow + 50;
          }
          else if(tomorrow.clouds[z] > 75)
          {
            effTomorrow = effTomorrow + 25;
          }
        }

        // day section 2 - peak hours
        // 8:00 -> 15:00
        if(tomorrow.hour[z] >= 8 && tomorrow.hour[z] <= 15)
        {
          if(tomorrow.clouds[z] < 50)
            {
              effTomorrow = effTomorrow + 100;
            }
          else if(tomorrow.clouds[z] >= 50 && tomorrow.clouds[z] <= 75 )
          {
            effTomorrow = effTomorrow + 75;
          }
          else if(tomorrow.clouds[z] > 75)
          {
            effTomorrow = effTomorrow + 50;
          }
        }
        
        z++;

      }
      else
      {
        //day after tomorrow
        
        daTomorrow.day = date;

        daTomorrow.hour[w] = getUnixHour(unixDate);
        daTomorrow.clouds[w] = myObject["list"][i]["clouds"]["all"];        

        w++; 

      }
   }

   outputPV[0] = effToday / nrOfHoursToday;
   outputPV[1] = effTomorrow /nrOfHoursTomorrow;   
}

String httpGETRequest(const char* server) 
{
  WiFiClient client;
  HTTPClient http;
    
  http.begin(client, server);

  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

 // Serial.println("PAYLOAD: ");
 // Serial.println(payload);
  return payload;
}

void jsonParserAPI(String serverPath)
{     
      String jsonBuffer;
      myObject = undefined;

      jsonBuffer = httpGETRequest(serverPath.c_str());

      myObject = JSON.parse(jsonBuffer);

      //Serial.println(jsonBuffer);
      //Serial.println(myObject);
      Serial.println(JSON.typeof(myObject));
  
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }

      if(serverPath == forecastPath)
      {
        Serial.println("---JSON  ZILE---");
        Serial.println(myObject);
        setForecastParameters(myObject);
        setTimeData(); 
      }
      else if(serverPath == hourlyCloudsForecastPath)
      {
        Serial.println("---JSON  ORE---");
        Serial.println(myObject);
        
        setHourlyCloudsParameters();
      }
}

void endNextionCommand()
{
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
}

void sendNextionDays()
{
  String data;
  
  endNextionCommand();
  data = "day0.txt=\""+String(timeData.day[0])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1.txt=\""+String(timeData.day[1])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2.txt=\""+String(timeData.day[2])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3.txt=\""+String(timeData.day[3])+"\"";
  Serial.print(data);
  endNextionCommand();

  //page 5
  data = "day0Bat.txt=\""+String(timeData.day[0])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Bat.txt=\""+String(timeData.day[1])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Bat.txt=\""+String(timeData.day[2])+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Bat.txt=\""+String(timeData.day[3])+"\"";
  Serial.print(data);
  endNextionCommand();
}

void sendNextionData()
{
  String data;

  sendNextionDays();

  /////-page 0
  data = "currentTemp.txt=\""+String(int(structMessage.temp))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "currentHumid.txt=\""+String(int(structMessage.humid))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "currentRain.txt=\""+String(int(structMessage.rain))+"\"";
  Serial.print(data);
  endNextionCommand();

  /////-page1
  data = "day1Tmax.txt=\""+String(int(forecastData.maxTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Tmin.txt=\""+String(int(forecastData.minTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Rain.txt=\""+String(int(forecastData.pop[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Wind.txt=\""+String(int(forecastData.speed[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Hum.txt=\""+String(int(forecastData.humid[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1Press.txt=\""+String(int(forecastData.pressure[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1TM.txt=\""+String(int(forecastData.morningTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1TD.txt=\""+String(int(forecastData.dayTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1TE.txt=\""+String(int(forecastData.eveningTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1TN.txt=\""+String(int(forecastData.nightTemp[1]))+"\"";
  Serial.print(data);
  endNextionCommand();

   /////-page2
  data = "day2Tmax.txt=\""+String(int(forecastData.maxTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Tmin.txt=\""+String(int(forecastData.minTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Rain.txt=\""+String(int(forecastData.pop[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Wind.txt=\""+String(int(forecastData.speed[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Hum.txt=\""+String(int(forecastData.humid[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2Press.txt=\""+String(int(forecastData.pressure[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2TM.txt=\""+String(int(forecastData.morningTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2TD.txt=\""+String(int(forecastData.dayTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2TE.txt=\""+String(int(forecastData.eveningTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2TN.txt=\""+String(int(forecastData.nightTemp[2]))+"\"";
  Serial.print(data);
  endNextionCommand();

   /////-page3
  data = "day3Tmax.txt=\""+String(int(forecastData.maxTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Tmin.txt=\""+String(int(forecastData.minTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Rain.txt=\""+String(int(forecastData.pop[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Wind.txt=\""+String(int(forecastData.speed[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Hum.txt=\""+String(int(forecastData.humid[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3Press.txt=\""+String(int(forecastData.pressure[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3TM.txt=\""+String(int(forecastData.morningTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3TD.txt=\""+String(int(forecastData.dayTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3TE.txt=\""+String(int(forecastData.eveningTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3TN.txt=\""+String(int(forecastData.nightTemp[3]))+"\"";
  Serial.print(data);
  endNextionCommand();


  //page 4
  data = "battery.val="+String(batteryPercentage);
  Serial.print(data);
  endNextionCommand();
  data = "batteryVal.txt=\""+String(batteryPercentage)+"\"";
  Serial.print(data);
  endNextionCommand();

  int chargingTimeHour;
  int chargingTimeMinutes;

  chargingTimeHour = chargingTime[0] / 60;
  chargingTimeMinutes = chargingTime[0] % 60;

  data = "chargingTime.txt=\""+ String(chargingTimeHour)+ ":" + String(chargingTimeMinutes) +"\"";
  Serial.print(data);
  endNextionCommand();

  
  //page 5
  data = "day0BatVal.txt=\""+String(int(outputPV[0]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day1BatVal.txt=\""+String(int(outputPV[1]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day2BatVal.txt=\""+String(int(outputPV[2]))+"\"";
  Serial.print(data);
  endNextionCommand();
  data = "day3BatVal.txt=\""+String(int(outputPV[3]))+"\"";
  Serial.print(data);
  endNextionCommand();
  
}

 
void loop() {
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }
  
  if ((millis() - lastTimeForecast) > timerDelayForecast) { 
     jsonParserAPI(forecastPath);
     delay(5000);
     jsonParserAPI(hourlyCloudsForecastPath);
     batteryManagement();
     sendForecastDataToServer();
     
     lastTimeForecast = millis();
  }
  
  sendNextionData();

}

/**
  Example taken from
    http://www.esp8266learning.com/vl53l0x-time-of-flight-sensor-and-esp8266.php

  Datasheet:
    https://www.st.com/resource/en/datasheet/vl53l0x.pdf

  Features of sensor:
    Measuring Field of view covered (FOV = 25 degrees)
    Measuring Range: (normal 1.2m) (long distance 2m)
    Measuring Time:
      Total time including processing: 33ms(typical)
      Measuring only: 20ms(default Accurycy +/-5%) 300ms(High Accuracy +/-3%)
    Operating voltage 2.6 to 3.5 V
    940 nm laser
    I2C Up to 400 kHz (FAST mode) serial bus
    I2C Address: 0x52

  Buy:
    https://www.aliexpress.com/item/32842745623.html
    Price: 3.15€

**/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Adafruit_VL53L0X.h"
#include <Servo.h>
#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager
#include "htmlFormHandler.h"

ESP_WiFiManager ESP_wifiManager("ESP_Configuration");


#define PIN_SERVO_AZ D8
#define PIN_SERVO_EL D7
#define SERVO_MAX_VALUES 300

int servoPosAzMin = 10;
int servoPosAzMax = 180;
int servoStepAz = 20;
int servoDirAz = 1;
int servoPosAz = servoPosAzMin;

int servoPosElMin = 80;
int servoPosElMax = 110;
int servoStepEl = 10;
int servoDirEl = 1;
int servoPosEl = servoPosElMin;

float servoToDegree = 1.0;
float servoOffsetDegree = 0.0;



#define DIST_MIN 1
#define DIST_MAX 500*10

#define SIZE_2D_GRAPH 600

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

Servo servo_az;
Servo servo_el;

#define ACTIVATE_WEBSERVER true

ESP8266WebServer server(80);

/**
   Result Values in mm
   nagative values are invalid/out of range
*/
int result[SERVO_MAX_VALUES];

boolean handleParameters() {
  boolean changes = false;
  changes |= parseParameter(server, "servoPosAzMin",    servoPosAzMin);
  changes |= parseParameter(server, "servoPosAzMax",    servoPosAzMax);
  changes |= parseParameter(server, "servoStepAz",      servoStepAz);
  changes |= parseParameter(server, "servoToDegree",  servoToDegree);
  changes |= parseParameter(server, "servoOffsetDegree",  servoOffsetDegree);
  return changes;
}

void drawRoomLayout() {
  String out;
  out.reserve(2600);
  char temp[70];

  int height = SIZE_2D_GRAPH;
  int width  = SIZE_2D_GRAPH;

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" + String(width) + "\" height=\"" + String(height) + "\">\n";
  out += "<rect width=\"" + String(width) + "\" height=\"" + String(height) + "\" fill=\"rgb(50, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = -1;
  int x = -1;

  int maxVal = resultMax();

  for ( int i = 0 ; i <= servoPosAzMax; i += 1 ) {
    int dist = result[i];

    if ( ( dist >= DIST_MIN) && ( dist < DIST_MAX) ) {
      float degree = 0 - (i * servoToDegree) + servoOffsetDegree;
      float rad = (degree * 71) / 4068;
      float distPixel = dist * max(height, width) / maxVal / 2;
      int y2 = (width / 2)  + cos(rad) * distPixel;
      int x2 = (height / 2) + sin(rad) * distPixel;

      /*
            Serial.print(i);
            Serial.print("\tdeg: ");
            Serial.print(degree);
            Serial.print("\trad: ");
            Serial.print(rad);
            Serial.print("\tdist: ");
            Serial.print(dist);
            Serial.print("\tdistPixel: ");
            Serial.print(distPixel);
            Serial.print(x2);
            Serial.print("\t");
            Serial.print(y2);
            Serial.println();
      */
      if ( i > 0 && x >= 0 && y >= 0 && x2 >= 0 && y2 >= 0) {
        sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, y, x2, y2);
        out += temp;
      }
      y = y2;
      x = x2;
    }
  }
  out += "<line x1=\"" + String(width / 2) + "\" y1=\"0\" x2=\"" + String(width / 2) + "\" y2=\"" + String(height) + "\" stroke=\"white\" stroke-width=\"1\" />\n";
  out += "<line x1=\"0\" y1=\"" + String(height / 2) + "\" x2=\"" + String(width) + "\" y2=\"" + String(height / 2) + "\" stroke=\"white\" stroke-width=\"1\" />\n";
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}


void printStatus(int i , float dist_cm) {
  Serial.print("# az=");
  Serial.print(servoPosAz );
  // Serial.print("Distance (cm): ");
  Serial.print("\t");
  Serial.print(" d=");

  Serial.print(dist_cm );
  Serial.println();
}


String upTimeString() {
  char temp[100];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 100, "Uptime: %02d:%02d:%02d ", hr, min % 60, sec % 60 );
  String result = String(temp);
  return result;
}

String inputForms() {
  String output = "\n";

  // border: 1px solid green;
  output += "<div style=\"text-align:left; margin:8px; \">\n";
  output += "Parameters:\n";
  output += "<form action=\"/\">\n";

  output += "       <table>\n";
  output += formString("servoPosAzMin",       servoPosAzMin);
  output += formString("servoPosAzMax",       servoPosAzMax);
  output += formString("servoStepAz",         servoStepAz);
  output += formString("servoToDegree",     servoToDegree);
  output += formString("servoOffsetDegree", servoOffsetDegree);

  if ( false) {
    output += "    <div class=\"slidecontainer\"> \n\n";
    output += "          <input type=\"range\" min=\"1\" max=\"100\" value=\"50\" class=\"slider\" id=\"myRange\">\n\n";
    output += "    </div>\n\n";
  }

  output += "       </table>\n";
  output += "       <input type=\"submit\" value=\"Submit\">\n";
  output += "     </form><br>\n";
  output += "   </div>\n\n";

  return output;

}

void resetResults() {
  Serial.println("Reset Results");
  for ( int i = 0 ; i <= SERVO_MAX_VALUES; i += 1 ) {
    result[i] = -1;
  }
}

void handleRoot() {

  if (   handleParameters()) {
    resetResults();
  }

  String output = "<html>\n\
  <head>\n\
    <meta http-equiv='refresh' content='5'/>\n\
    <title>ESP8266 Distance</title>\n\
    <style>\n\
      body {\n\
       background-color: #cccccc;\n\
       font-family: Arial, Helvetica, Sans-Serif;\n\
       Color: #000088;\n\
       }\n\
    </style>\n\
  </head>\n\n";

  output += "  <body>\n";
  output += "\n";
  output += "  <h1>ESP8266 2D-Scanner</h1>\n\n";

  {
    output += "  <div style=\"text-align:left; margin:8px;\">\n";

    // Uptime
    output += "      <p>" + upTimeString() + "</p>\n";

    // Open Scad Reference
    output += "      <p>\n";
    output += "         <a href=\"/scan-2D.scad\">scan-2D.scad</a>\n";
    output += "      </p>\n\n";
    output += "   </div>\n\n";
  }

  {
    output += "   <div style=\"float:left; width:100%; margin:8px;\">\n";
    {
      // Room Layout img Reference
      output += "\n";
      output += "      <div style=\"float:left; height:" + String( SIZE_2D_GRAPH + 40 ) + "; width:" + String(SIZE_2D_GRAPH ) + "\">\n";
      output += "          <p>Room Layout:</p>\n";
      output += "          <img src=\"/roomLayout.svg\" />\n";
      output += "      </div>\n";
    }

    {
      // HTML Forms
      output += "      <div style=\"text-align:right; margin: 8px;\">\n";
      output +=           inputForms();
      output += "      </div>\n\n";
    }

    output += "    </div>\n";
    output += "\n";
  }

  // Distances Graf img reference
  output += "\n";
  output += "       <div style=\"float:left; margin:8px;\">\n";
  output += "          <p>Distances:<p/>\n";
  output += "          <img src=\"/distGraph.svg\" />\n";
  output += "       </div>\n";
  output += "\n";

  // html End
  output += "\n\n";
  output += "  </body>\n";
  output += "</html > \n";
  server.send(200, "text / html", output);

}

void handleScad() {
  String output;
  output.reserve(64);

  output = "\n";
  output += "\n";
  output += "\n";
  output += "// ===========================================================\n";
  output += "// Distance Sensor\n";
  output += "// ===========================================================\n";

  for ( int i = servoPosAzMin ; i <= servoPosAzMax; i++) {
    output += "segment(i=";
    output += i ;
    output += "\t";
    output += ",dist=";
    output += result[i];
    output += ");\n";
  }

  output += "\n";
  output += "\n";
  output += "\n";
  output += "module segment(i=0,d=20){\n";
  output += "  if ( d > 0 && dist <800 ) {\n";
  output += "    sinVal=0.422618261740699;\n";
  output += "    deltaDist= .1;\n";
  output += "    dist1= dist-deltaDist;\n";
  output += "\n";
  output += "    rotate( [0,90,i] )\n";
  output += "       translate([0,0,dist1])\n";
  output += "            cylinder(d1=sinVal*dist1,d2=sinVal*dist,h=deltaDist);\n";
  output += "    }\n";
  output += "}\n";
  output += "\n";
  output += "\n";
  server.sendContent(output);
  server.chunkedResponseFinalize();
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  Serial.println("Sent 2D Scad File");
}


int resultMax() {
  int max = 0;
  for ( int i = 0 ; i <= servoPosAzMax; i += 1 ) {
    int dist = result[i];
    if ( dist < DIST_MAX && dist > max) {
      max = dist;
    }
  }
  return max;
}

void distanceGraph() {
  String out;
  out.reserve(2600);
  char temp[70];
  int width = 900;
  int height = 300;

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"";
  out += String(width) + "\" height=\"" + String( height) + "\">\n";

  out += "<rect width=\"" + String(width) + "\" height=\"" + String(height) + "\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = 0;
  int x = 0;

  int maxVal = 10 + resultMax();

  for ( int i = 0 ; i <= servoPosAzMax; i += 1 ) {
    int y2 = result[i] * height / maxVal;
    if ( y2 < DIST_MAX && y2 > 0 ) {
      int x2 = i * width / servoPosAzMax;
      if ( y > 0) {
        sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, height - y, x2, height - y2);
        out += temp;
      }
      y = y2;
      x = x2;
    }
  }
  out += "</g>\n";
  out += "</svg>\n";

  server.send(200, "image/svg+xml", out);
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("2D/3D-Scanner .... ");


  Serial.println("Reset Results ...");
  resetResults();

  Serial.println("Wifi Manager ...");
  ESP_wifiManager.setDebugOutput(true);

  // ESP_wifiManager.setMinimumSignalQuality(-1);

  // To reset the configuration simply diconnect from the current Wifi
  // WiFi.disconnect();

  ESP_wifiManager.autoConnect();
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Attaching Servo ...");
  servo_az.attach(PIN_SERVO_AZ);  // attaches the servo
  servo_el.attach(PIN_SERVO_EL);  // attaches the servo
  servo_az.write(servoPosAz );
  servo_el.write(servoPosEl );

  // VL53L0X
  Serial.println("Conneting Adafruit VL53L0X ...");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    delay(5 * 1000);
    while (1);
  }
  // power
  Serial.println(F("VL53L0X API Simple Ranging example\n\n"));




  if (MDNS.begin("2d-SCANNER")) {
    Serial.println("MDNS responder started");
    Serial.println("http://2d-scanner.fritz.box/");
  }

  // Register URLs to answer
  if ( ACTIVATE_WEBSERVER ) {
    server.on("/", handleRoot);
    server.on("/distGraph.svg", distanceGraph);
    server.on("/scan-2D.scad", handleScad);
    server.on("/roomLayout.svg", drawRoomLayout);
    server.onNotFound(handleNotFound);
    server.begin();
  }
  Serial.println("HTTP server started");
}


void measure(){

  VL53L0X_RangingMeasurementData_t measure;

  int retryCount = 0;
  int dist_mm = -1;
  boolean doRetry = true;
  do {
    // Serial.print("Reading a measurement... ");
    lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
    dist_mm = measure.RangeMilliMeter;
    if ( dist_mm > 1 && dist_mm > 800 && measure.RangeStatus != 4) {
      doRetry = false;
      if ( retryCount > 2 ) {
        Serial.println("Retry succeded");
      }
    } else if ( retryCount++ < 2 ) {
      Serial.println("EL: "+String(servoPosEl) + " AZ: " + String(servoPosAz) + ": Retry " + String(retryCount));
      delay(100);
    } else {
      doRetry = false;
    }
  } while ( doRetry );

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    // printStatus(servoPosAz, dist_mm);
    if ( dist_mm > 800) {
      result[servoPosAz] = -1;
    } else {
      result[servoPosAz] = dist_mm;
    }
  } else {
    result[servoPosAz] = -1;
    // printStatus(servoPosAz, -1);
  }

  // Move Servo
  servoPosAz += servoDirAz * servoStepAz;
  if ( ( servoPosAz > servoPosAzMax) || (servoPosAz < servoPosAzMin ) ) {
    servoPosEl += servoDirEl * servoStepEl;
    if ( servoPosEl > servoPosElMax ) {
      servoPosEl = servoPosElMax;
      servoDirEl = -1;
    }
    if ( servoPosEl < servoPosElMin ) {
      servoPosEl = servoPosElMin;
      servoDirEl = 1;
    }
    Serial.println("New El: "+String(servoPosEl));
    servo_el.write(servoPosEl );
  }

  if ( servoPosAz > servoPosAzMax ) {
    servoPosAz = servoPosAzMax;
    // servoPosAz = servoPosAzMin ;
    servoDirAz = -1;
  }
  if ( servoPosAz < servoPosAzMin ) {
    servoPosAz = servoPosAzMin;
    servoDirAz = 1;
  }

  servo_az.write(servoPosAz );
  delay(10);
}

void loop() {
  
  measure();
  

  if (ACTIVATE_WEBSERVER) {
    server.handleClient();
    MDNS.update();
  }
}

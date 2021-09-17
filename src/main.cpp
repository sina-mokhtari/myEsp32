#include <Arduino.h>
#include <WiFi.h>
#include "SPIFFS.h"

#define USER_BUTTON 0

int i = 1, sumC = 0, sumV = 0, tmpIdx;
float current, voltage, avgC, avgV, valC, valV;

unsigned long previousMillis;
unsigned long currentMillis;
unsigned long lastStateMillis = 0;

const char *ssid = "ESP32-Access-Point";
const char *password = "123456789";

String header, // Variable to store the HTTP request
    tmpStr,
    currentLine;

volatile bool turnOnAP = false;

// Set web server port number to 80
WiFiServer server(80);

WiFiClient client;

enum stateTypes
{
  MONITORING,
  AP_ON,
  TURNING_AP_OFF,
  PRESSING_BUTTON_ON_MONITOR,
  PRESSING_BUTTON_ON_AP,
  DEBUG
};

volatile stateTypes state = MONITORING;

hw_timer_t *timer = NULL;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

File file;

unsigned int totalBytes;
unsigned int usedBytes;

String to6digit(int a)
{
  String tmp;

  if (a < 10)
    tmp = "00000";
  else if (a < 100)
    tmp = "0000";
  else if (a < 1000)
    tmp = "000";
  else if (a < 10000)
    tmp = "00";
  else if (a < 100000)
    tmp = "0";
  else if (a < 1000000)
    tmp = "";

  tmp += a;

  return tmp;
}

String to5digit(float a)
{
  String tmp = "";

  if (a < 10.0)
  {
    tmp += "0";
  }

  tmp += a;

  return tmp;
}

void listFilesInDir(File dir, int numTabs = 1)
{
  while (true)
  {

    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files in the folder
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      listFilesInDir(entry, numTabs + 1);
    }
    else
    {
      // display zise for file, nothing for directory
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void monitor()
{
  currentMillis = millis();

  if (currentMillis - previousMillis > 100)
  {
    previousMillis = currentMillis;

    if (i < 100)
    {
      valC = analogReadMilliVolts(26);
      valV = analogReadMilliVolts(25);

      voltage = valV * 0.01036 + 0.2;
      current = (valC < 1620) ? 0.0 : (valC * 0.0439 - 70.24);

      sumC += current;
      sumV += voltage;
      i++;
    }
    else
    {
      avgC = sumC / 100.0;
      avgV = sumV / 100.0;

      tmpIdx++;

      tmpStr = "\n Record ";
      tmpStr += tmpIdx;
      tmpStr += ":\n  current: ";
      tmpStr += avgC;
      tmpStr += "\n  voltage: ";
      tmpStr += avgV;
      tmpStr += "\n";

      Serial.println(tmpStr);

      //noInterrupts();

      //portENTER_CRITICAL();

      //Serial.println("delay");
      //delay(5000);
      file = SPIFFS.open("/log.txt", "a");
      if (!file)
      {
        Serial.println("Failed to open log file");
        return;
      }

      tmpStr = to6digit(tmpIdx) + to5digit(avgC) + to5digit(avgV);

      if (!file.println(tmpStr))
      {
        Serial.println("File write failed!");
        while (1)
          ;
      }

      file.close();
      //taskEXIT_CRITICAL();
      // interrupts();

      Serial.println();

      i = 1;
      sumC = 0;
      sumV = 0;
    }
  }
  /*
  usedBytes = SPIFFS.usedBytes();

  Serial.println("Remaining space: ");
  Serial.print(totalBytes - usedBytes);
  Serial.println(" byte");
*/
}

void handleAP()
{
  if (turnOnAP)
  {
    Serial.println("Setting AP (Access Point)…");

    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.begin();
    turnOnAP = false;
  }
  client = server.available(); // Listen for incoming clients
  if (client)
  {                                // If a new client connects,
    Serial.println("New Client."); // print a message out in the serial port
    currentLine = "";              // make a String to hold incoming data from the client
    while (client.connected())
    {                         // loop while the client's connected
      if (client.available()) // if there's bytes to read from the client,
      {
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;

        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /clear") >= 0)
            {
              SPIFFS.remove("/log.txt");
              tmpIdx = 0;
              Serial.println("log file deleted");
            }

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("table.minimalistBlack{border:3px solid #000;width:100%;text-align:left;border-collapse:collapse}table.minimalistBlack td,table.minimalistBlack th{border:1px solid #000;padding:5px 4px}");
            client.println("table.minimalistBlack tbody td{font-size:13px}table.minimalistBlack thead{background:#CFCFCF;background:-moz-linear-gradient(top,#dbdbdb 0%,#d3d3d3 66%,#CFCFCF 100%);background:-webkit-linear-gradient(top,#dbdbdb 0%,#d3d3d3 66%,#CFCFCF 100%);background:linear-gradient(to bottom,#dbdbdb 0%,#d3d3d3 66%,#CFCFCF 100%);border-bottom:3px solid #000}");
            client.println("table.minimalistBlack thead th{font-size:15px;font-weight:700;color:#000;text-align:left}table.minimalistBlack tfoot{font-size:14px;font-weight:700;color:#000;border-top:3px solid #000}table.minimalistBlack tfoot td{font-size:14px}");
            client.println(".button {border:none;color:white;padding:15px 32px;text-align:center;text-decoration:none;display:inline-block;font-size:16px;margin:4px 2px;cursor:pointer;border-radius:10px;}");
            client.println("</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            client.println("<p style=\"text-align:center;\">");
            client.println("<a href=\"/\"><button class=\"button\" style=\"background-color:#008CBA;\">Refresh</button></a>");
            client.println("<a href=\"/clear\"><button class=\"button\" style=\"background-color:#f44336;\">Clear</button></a>");
            client.println("<a href=\"#endOfPage\"><button class=\"button\"  style=\"background-color:#555555;\">End of page</button></a>");
            client.println("</p>");

            client.println("<table class=\"minimalistBlack\"><thead><tr><th>Index</th><th>Current</th><th>Voltage</th></tr></thead>");

            noInterrupts();
            file = SPIFFS.open("/log.txt", "r");
            while (file.available())
            {
              tmpStr = file.readStringUntil('\n');
              client.println("<tbody><tr><td>" + String(tmpStr.substring(0, 6).toInt()) + "</td><td>" + String(tmpStr.substring(6, 11).toFloat()) + "</td><td>" + String(tmpStr.substring(11).toFloat()) + "</td></tr>");
            }
            file.close();
            interrupts();
            client.println("</tbody></table>");
            client.println("<p id=\"endOfPage\"></p>");
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
      else
        break;
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void IRAM_ATTR turnOffAP()
{
  WiFi.softAPdisconnect(true);
  Serial.println("Access point turned off!");
  state = MONITORING;
}

void IRAM_ATTR isr()
{
  //portENTER_CRITICAL_ISR(&timerMux);

  if (timerAlarmEnabled(timer))
  {
    //int tmp = timerRead(timer);
    //Serial.println(tmp);
    if (state == PRESSING_BUTTON_ON_MONITOR)
    {
      //   if (tmp < 1000000 && tmp > 10000) // probably useless
      //{
      //timerRestart(timer);
      timerAlarmDisable(timer);
      Serial.println("btn press on monitor fail");
      state = MONITORING;
      //}
    }
    else if (state == PRESSING_BUTTON_ON_AP)
    {
      //if (tmp < 3000000 && tmp > 10000) // probably useless
      //{
      //timerRestart(timer);
      timerAlarmDisable(timer);
      Serial.println("btn press on AP fail");
      state = AP_ON;
      //}
    }

    return;
  }

  if (millis() - lastStateMillis < 30)
    return;

  switch (state)
  {
  case MONITORING:
    if (digitalRead(USER_BUTTON))
      return;

    state = PRESSING_BUTTON_ON_MONITOR;
    timerRestart(timer);
    timerAlarmEnable(timer);
    Serial.println("waiting before turning on AP...");
    break;
  case PRESSING_BUTTON_ON_MONITOR:
    state = AP_ON;
    timerAlarmDisable(timer);
    turnOnAP = true;
    break;
  case AP_ON:
    if (digitalRead(USER_BUTTON))
      return;

    state = PRESSING_BUTTON_ON_AP;
    timerRestart(timer);
    timerAlarmEnable(timer);
    Serial.println("waiting before turning off AP...");
    break;
  case PRESSING_BUTTON_ON_AP:
    state = TURNING_AP_OFF;
    //turnOffAP();
    timerAlarmDisable(timer);
    break;
  default:
    break;
  }

  lastStateMillis = millis();
  //portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR buttonHeldTrigger()
{
  //portENTER_CRITICAL_ISR(&timerMux);

  if (state == PRESSING_BUTTON_ON_MONITOR)
  {
    Serial.println("btn press on monitor success");
    state = AP_ON;
    timerAlarmDisable(timer);
    turnOnAP = true;
  }
  else if (state == PRESSING_BUTTON_ON_AP)
  {
    Serial.println("btn press on AP success");
    state = TURNING_AP_OFF;
    //turnOffAP();
    timerAlarmDisable(timer);
  }

  //portEXIT_CRITICAL_ISR(&timerMux);
}

void setup()
{
  previousMillis = millis();
  Serial.begin(115200);

  attachInterrupt(USER_BUTTON, isr, CHANGE);

  timer = timerBegin(0, 80, true);

  timerAttachInterrupt(timer, &buttonHeldTrigger, true);

  // Sets an alarm to sound every 3 second
  timerAlarmWrite(timer, 3000000, true);

  Serial.println(F("Inizializing FS..."));
  if (SPIFFS.begin(true))
  {
    Serial.println(F("SPIFFS mounted correctly."));
  }
  else
  {
    Serial.println(F("!An error occurred during SPIFFS mounting"));
  }

  //SPIFFS.format();

  totalBytes = SPIFFS.totalBytes();
  usedBytes = SPIFFS.usedBytes();

  Serial.println("===== File system info =====");

  Serial.print("Total space:      ");
  Serial.print(totalBytes);
  Serial.println("byte");

  Serial.print("Total space used: ");
  Serial.print(usedBytes);
  Serial.println("byte");

  Serial.println();

  // Open dir folder
  File dir = SPIFFS.open("/");
  // List file at root
  listFilesInDir(dir);

  file = SPIFFS.open("/log.txt", "a");
  unsigned long pos = file.position();
  file.close();

  if (pos == 0)
    tmpIdx = 0;
  else
  {
    pos -= 18;
    file = SPIFFS.open("/log.txt", "r");
    file.seek(pos);
    tmpStr = file.readStringUntil('.');
    tmpIdx = tmpStr.substring(0, 6).toInt();
    file.close();
  }
}

void loop()
{
  switch (state)
  {
  case MONITORING:
    monitor(); 
    break;
  case AP_ON:
    handleAP();
    break;
  case TURNING_AP_OFF:
    turnOffAP();
    break;
    case DEBUG:

    break;
  default:
    break;
  }

  if (Serial.available())
  {
    String serialCmd = Serial.readStringUntil('\n');
    if (serialCmd.equals("PAYANAK_LISTENING"))
    {
      state = DEBUG;
      Serial.println("65849173");
      Serial.println("DeviceName");
    }
  }
}
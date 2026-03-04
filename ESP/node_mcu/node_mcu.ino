String wifiStatus = "";
String serverResponse = "";
String errorCode = "";
String cameraStatus = "";
String ipAddress = "";
String unknownMsg = "";
String predictionValue = "";

bool awaitingJson = false;  // flag to combine server response lines

// ---- ADC Config ----
const int analogPin = A0;   // NodeMCU analog input pin
float batteryVoltage = 0.0;
float powerValue = 0.0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // send every 2 seconds

// ---- ADC calibration (NodeMCU ESP8266) ----
// A0 reads 0–1.0 V, returns 0–1023
// If you use a voltage divider for higher battery voltages,
// adjust this constant accordingly.
const float maxBatteryVoltage = 12.0;   // example: 12V battery with divider scaling to 1.0V
const float adcReference = 1023.0;      // 10-bit ADC max value

void setup() {
  Serial.begin(115200);   // USB Serial Monitor / UART to ESP32-CAM
  Serial.println("NodeMCU Ready...");
}

void loop() {
  // ---- Handle messages coming from ESP32-CAM ----
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');  
    msg.trim(); 

    if (msg.indexOf("WiFi connected!") >= 0) {
      wifiStatus = msg;   
      Serial.print("wifiStatus = \"");
      Serial.print(wifiStatus);
      Serial.println("\"");
    } 
    else if (msg.startsWith("IP address:")) {
      ipAddress = msg;
      Serial.print("ipAddress = \"");
      Serial.print(ipAddress);
      Serial.println("\"");
    }
    else if (msg.indexOf("Server response:") >= 0) {
      awaitingJson = true;  // expect JSON next
      serverResponse = msg;
    }
    else if (awaitingJson && msg.startsWith("{")) {
      serverResponse += " " + msg;
      
      int predIndex = msg.indexOf("\"prediction\":");
      if (predIndex >= 0) {
        predIndex += 13;
        int endIndex = msg.indexOf("}", predIndex);
        if (endIndex == -1) endIndex = msg.length();
        predictionValue = msg.substring(predIndex, endIndex);
        predictionValue.trim();
        Serial.print("predictionValue = \"");
        Serial.print(predictionValue);
        Serial.println("\"");
      }

      Serial.print("serverResponse = \"");
      Serial.print(serverResponse);
      Serial.println("\"");
      awaitingJson = false;
    }
    else if (msg.indexOf("Error code:") >= 0) {
      errorCode = msg;
      Serial.print("errorCode = \"");
      Serial.print(errorCode);
      Serial.println("\"");
    }
    else if (msg.indexOf("Camera") >= 0) {
      cameraStatus = msg;
      Serial.print("cameraStatus = \"");
      Serial.print(cameraStatus);
      Serial.println("\"");
    }
    else {
      unknownMsg = msg;
      Serial.print("unknownMsg = \"");
      Serial.print(unknownMsg);
      Serial.println("\"");
    }
  }

  // ---- Periodically calculate and send power ----
  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= sendInterval) {
    lastSendTime = currentMillis;
    
    int adcValue = analogRead(analogPin);  // 0–1023
    batteryVoltage = (adcValue / adcReference) * maxBatteryVoltage;  // scaled battery voltage
    powerValue = batteryVoltage * 1.0;     // P = V * I, I = 1A constant

    // format as pv=<powerValue>
    String pvMessage = "pv=" + String(powerValue, 2);  // 2 decimals
    Serial.println(pvMessage);  // send to ESP32-CAM

  }
}

// Target platform: This project is intended to run on the ESP32 microcontroller.
// It uses the ESP32's built-in Wi-Fi capabilities to create a web server that can control GPIO pins.
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Replace with your network credentials
const char* ssid = "iPhone de Samady";
const char* password = "sepamadyn";

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;
String output26State = "off";
String output27State = "off";

// Create a web server object
WebServer server(80);

// Creating FreeRTOS task handles
TaskHandle_t mainTask;
TaskHandle_t serverTask;
// Function to handle the main task
void MainTask(void *pvParameters);
void ServerTask(void *pvParameters);

// Function to handle the root URL and show the current states
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

  // Display GPIO 26 controls
  html += "<p>GPIO 26 - State " + output26State + "</p>";
  if (output26State == "off") {
    html += "<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  // Display GPIO 27 controls
  html += "<p>GPIO 27 - State " + output27State + "</p>";
  if (output27State == "off") {
    html += "<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Function to handle turning GPIO 26 on
void handleGPIO26On() {
  output26State = "on";
  digitalWrite(output26, HIGH);
  handleRoot();
}

// Function to handle turning GPIO 26 off
void handleGPIO26Off() {
  output26State = "off";
  digitalWrite(output26, LOW);
  handleRoot();
}

// Function to handle turning GPIO 27 on
void handleGPIO27On() {
  output27State = "on";
  digitalWrite(output27, HIGH);
  handleRoot();
}

// Function to handle turning GPIO 27 off
void handleGPIO27Off() {
  output27State = "off";
  digitalWrite(output27, LOW);
  handleRoot();
}

void setup() {
  Serial.begin(115200);

  // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);

  // Connect to Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up the web server to handle different routes
  server.on("/", handleRoot);
  server.on("/26/on", handleGPIO26On);
  server.on("/26/off", handleGPIO26Off);
  server.on("/27/on", handleGPIO27On);
  server.on("/27/off", handleGPIO27Off);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  xTaskCreatePinnedToCore(MainTask, "mainTask", 8192, NULL, 1, &mainTask, 0);
  xTaskCreatePinnedToCore(ServerTask, "serverTask", 4096, NULL, 3, &serverTask, 1);

}

void loop() {
  // Handle incoming client requests
  //server.handleClient();
}

void MainTask(void *pvParameters) {
  //taskCount++;
  // while (true) {
  //   if ((millis() - SensorUpdate) >= 50) {
  //     SensorUpdate = millis();
  //     BitsA0 = analogRead(PIN_A0);
  //     BitsA1 = analogRead(PIN_A1);
  //     VoltsA0 = BitsA0 * 3.3 / 4096;
  //     VoltsA1 = BitsA1 * 3.3 / 4096;
  //   }
    vTaskDelay(pdMS_TO_TICKS(10));  // Adjust the delay as needed
  //}
}

void ServerTask(void *pvParameters) {
  //taskCount++;
  while (true) {
    // Update the server task core indicators
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
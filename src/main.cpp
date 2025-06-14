// Target platform: This project is intended to run on the ESP32 microcontroller.
// It uses the ESP32's built-in Wi-Fi capabilities to create a web server that can control GPIO pins.
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <FirebaseESP32.h>
#include <RTClib.h>

// Replace with your network credentials
const char* ssid = "iPhone de Samady"; // Mantenha suas credenciais
const char* password = "sepamadyn";    // Mantenha suas credenciais

// Defining the API Key and the Firebase RTDB URL
const char* firebase_api_key =  "";
const char* database_url = "";

// Define the user Email and password that is used to authenticate with Firebase
const char* user_email = "";
const char* user_password = "";

// Assign output variables to GPIO pins
const int ledPin = 25;
const int output26 = 26;
const int output27 = 27;
String output26State = "off"; // Estado inicial do GPIO 26
String output27State = "off"; // Estado inicial do GPIO 27

// Create a web server object
WebServer server(80);

// Create an RTC object
RTC_DS3231 rtc;
const int CLOCK_INTERRUPT_PIN = 2;
volatile bool interruptFlag = false;

char diasDaSemana[7][12] = {"Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado"};

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

// Creating FreeRTOS task handles
TaskHandle_t mainTaskHandle;   // Renomeado para clareza (Handle no final)
TaskHandle_t serverTaskHandle; // Renomeado para clareza
TaskHandle_t RTCTaskHandle;
TaskHandle_t FirebaseTaskHandle;

// Function prototypes for tasks
void MainTask(void *pvParameters);
void ServerTask(void *pvParameters);
void RTCTask(void *pvParameters);
void FirebaseTask(void *pvParameters);

// Function to handle the root URL and show the current states
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server com FreeRTOS</h1>"; // Pequena alteração no título

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
  Serial.println("GPIO 26 turned ON");
  handleRoot(); // Redireciona para a página principal para mostrar o novo estado
}

// Function to handle turning GPIO 26 off
void handleGPIO26Off() {
  output26State = "off";
  digitalWrite(output26, LOW);
  Serial.println("GPIO 26 turned OFF");
  handleRoot();
}

// Function to handle turning GPIO 27 on
void handleGPIO27On() {
  output27State = "on";
  digitalWrite(output27, HIGH);
  Serial.println("GPIO 27 turned ON");
  handleRoot();
}

// Function to handle turning GPIO 27 off
void handleGPIO27Off() {
  output27State = "off";
  digitalWrite(output27, LOW);
  Serial.println("GPIO 27 turned OFF");
  handleRoot();
}

void tratarInterrupcao() {
  interruptFlag = true;
}

void imprimirDataHora(DateTime momento) {
  Serial.print("Data: ");
  Serial.print(momento.day(), DEC);
  Serial.print('/');
  Serial.print(momento.month(), DEC);
  Serial.print('/');
  Serial.print(momento.year(), DEC);
  Serial.print(" / Dia da semana: ");
  Serial.print(diasDaSemana[momento.dayOfTheWeek()]);
  Serial.print(" / Hora: ");
  if (momento.hour() < 10) Serial.print("0");
  Serial.print(momento.hour(), DEC);
  Serial.print(':');
  if (momento.minute() < 10) Serial.print("0");
  Serial.print(momento.minute(), DEC);
  Serial.print(':');
  if (momento.second() < 10) Serial.print("0");
  Serial.print(momento.second(), DEC);
  Serial.print(" / Temperatura: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" °C");
  return;
}

void agendarProximoCiclo() {
  DateTime agora = rtc.now();
  
  // Próximo Alarme 1 (LIGA) em 30 segundos a partir de agora
  DateTime proximoAlarme1 = agora + TimeSpan(0, 0, 0, 30);
  
  // Configura os alarmes
  rtc.setAlarm1(proximoAlarme1, DS3231_A1_Date);
  
  Serial.println("\n--- Próximo ciclo agendado ---");
  Serial.print("Próximo Alarme 1 (LIGA): ");
  imprimirDataHora(proximoAlarme1);
  Serial.print("Próximo Alarme 2 (DESLIGA): ");

  return;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Setup...");

  // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  pinMode(ledPin, OUTPUT);
  // Set outputs to LOW initially
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);

  // Connect to Wi-Fi network --------------------------------------------------------------------------------------
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attemptCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attemptCount++;
    if (attemptCount > 20) { // Timeout para conexão Wi-Fi
        Serial.println("\nFalha ao conectar ao Wi-Fi. Verifique as credenciais ou a rede.");
        // Você pode querer tratar isso de forma diferente, como reiniciar ou entrar em modo de configuração.
        // Por agora, apenas paramos aqui para não prosseguir sem Wi-Fi.
        while(true) { delay(1000); }
    }
  }
  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());

  // Set up the web server to handle different routes
  server.on("/", HTTP_GET, handleRoot); // Especificando HTTP_GET para clareza
  server.on("/26/on", HTTP_GET, handleGPIO26On);
  server.on("/26/off", HTTP_GET, handleGPIO26Off);
  server.on("/27/on", HTTP_GET, handleGPIO27On);
  server.on("/27/off", HTTP_GET, handleGPIO27Off);

  // Start the web server
  server.begin();
  Serial.println("Servidor HTTP iniciado");

  // Criando as tasks do FreeRTOS --------------------------------------------------------------------------------------
  // Para ESP32, o Core 0 (PRO_CPU) é frequentemente usado para Wi-Fi e tarefas de sistema.
  // O Core 1 (APP_CPU) é geralmente para código de aplicação.
  // É uma boa prática executar tarefas de aplicação como o servidor web no Core 1.
  
  Serial.println("Criando tasks do FreeRTOS...");
  xTaskCreatePinnedToCore(
    MainTask,         // Função da Task
    "MainTask",       // Nome da Task
    4096,             // Tamanho da Stack (pode ser ajustado conforme necessário)
    NULL,             // Parâmetros da Task
    1,                // Prioridade da Task (0 é a mais baixa)
    &mainTaskHandle,  // Handle da Task
    0                 // Core onde a Task será executada (0 para PRO_CPU)
  );

  xTaskCreatePinnedToCore(
    ServerTask,       // Função da Task
    "ServerTask",     // Nome da Task
    4096,             // Tamanho da Stack (Aumentar para 8192 se houver problemas de stack com WebServer)
    NULL,             // Parâmetros da Task
    2,                // Prioridade da Task (maior que MainTask para responsividade do servidor)
    &serverTaskHandle,// Handle da Task
    1                 // Core onde a Task será executada (1 para APP_CPU)
  );

  xTaskCreatePinnedToCore(
    RTCTask,       // Função da Task
    "RTCTask",     // Nome da Task
    4096,             // Tamanho da Stack (Aumentar para 8192 se houver problemas de stack com WebServer)
    NULL,             // Parâmetros da Task
    1,                // Prioridade da Task (maior que MainTask para responsividade do servidor)
    &RTCTaskHandle,// Handle da Task
    1                 // Core onde a Task será executada (1 para APP_CPU)
  );

  xTaskCreatePinnedToCore(
    FirebaseTask,       // Função da Task
    "FirebaseTask",     // Nome da Task
    4096,             // Tamanho da Stack (Aumentar para 8192 se houver problemas de stack com WebServer)
    NULL,             // Parâmetros da Task
    1,                // Prioridade da Task (maior que MainTask para responsividade do servidor)
    &FirebaseTaskHandle,// Handle da Task
    1                 // Core onde a Task será executada (1 para APP_CPU)
  );
  
  Serial.println("Tasks criadas. Setup concluído.");
  // O scheduler do FreeRTOS já está rodando neste ponto no ESP32 Arduino Core.
  // setup() será finalizada, e as tasks começarão a executar.

  // Firebase Setup --------------------------------------------------------------------------------------
  // Assign the api key (required) 
  config.api_key = firebase_api_key;

  // Assign the user sign in credentials
  auth.user.email = user_email;
  auth.user.password = user_password;

  // Assign the RTDB URL (required) 
  config.database_url = database_url;
  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;

  // Inicializa o RTC  --------------------------------------------------------------------------------------
  if (!rtc.begin()) {
    Serial.println("RTC não encontrado!");
    while (1);
  }

  // Se perdeu energia, ajusta a hora atual para a hora da compilação
  if (rtc.lostPower()) {
    Serial.println("RTC perdeu a hora. Ajustando...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Desativa saída 32kHz e alarmes antigos
  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF); // Garante que INT/SQW está no modo interrupção

  // Configura pino da interrupção e função de tratamento
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), tratarInterrupcao, FALLING);

  // Configura os alarmes iniciais
  DateTime agora = rtc.now();
  
  // Alarme 1 (LIGA) - Ativa em 1 minuto
  DateTime alarme1 = agora + TimeSpan(0, 0, 0, 30); // +30 segundos

  // Configura os alarmes no RTC
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  rtc.setAlarm1(alarme1, DS3231_A1_Date);

  Serial.println("Sistema iniciado!");
  Serial.println("Modo de operação: Alarme 1 LIGA, Alarme 2 DESLIGA");
  Serial.print("Alarme 1 (LIGA) agendado para: ");
  imprimirDataHora(alarme1);

}

void loop() {
  // A função loop() pode ser deixada vazia, pois as tasks do FreeRTOS
  // estão cuidando de toda a lógica principal.
  // Ou pode ter um vTaskDelay para garantir que a loopTask (se ativa) não consuma CPU desnecessariamente.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// Implementação da MainTask
void MainTask(void *pvParameters) {
  Serial.println("MainTask: Iniciada no Core " + String(xPortGetCoreID()));
  Serial.println("MainTask: GPIO " + String(ledPin) + " configurado como OUTPUT para piscar LED.");

  bool ledState = LOW;

  while (true) {
    ledState = !ledState; // Inverte o estado do LED
    digitalWrite(ledPin, ledState);

    if (ledState == HIGH) {
      Serial.println("MainTask: LED ON"); // Descomente para log detalhado
    } else {
      Serial.println("MainTask: LED OFF"); // Descomente para log detalhado
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Pisca a cada 500ms (meio segundo ON, meio segundo OFF)
  }
}

// Implementação da ServerTask
void ServerTask(void *pvParameters) {
  Serial.println("ServerTask: Iniciada no Core " + String(xPortGetCoreID()));
  while (true) {
    server.handleClient(); // Processa requisições HTTP
    
    // Pequeno delay para não sobrecarregar o processador e permitir que outras tasks executem.
    // 10ms é um valor comum. Se a resposta do servidor parecer lenta, pode reduzir para 1ms.
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}


void RTCTask(void *pvParameters) {
  Serial.println("RTCTask: Iniciada no Core " + String(xPortGetCoreID()));
  while (true) {
      if (rtc.alarmFired(1)) {
        // Se o alarme 1 disparou, execute a ação desejada
        rtc.clearAlarm(1);
        digitalWrite(output26, !digitalRead(output26));
        Serial.println("\n>>> ALARME 1 DISPARADO - LED LIGADO <<<");
        Serial.print("Hora do disparo: ");
        imprimirDataHora(rtc.now());
        agendarProximoCiclo();
    }
    
    // Pequeno delay para não sobrecarregar o processador e permitir que outras tasks executem.
    // 10ms é um valor comum. Se a resposta do servidor parecer lenta, pode reduzir para 1ms.
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

void FirebaseTask(void *pvParameters) {
  Serial.println("FirebaseTask: Iniciada no Core " + String(xPortGetCoreID()));
  int ledState;
  while (true) {
    Serial.println("Firebase ESP32 Client v4.4.0");
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)){
      sendDataPrevMillis = millis();
      if(Firebase.RTDB.getInt(&fbdo, "/led1/state", &ledState)){
        digitalWrite(ledPin, ledState);
      }else{
        Serial.println(fbdo.errorReason().c_str());
      }
    }
    // Pequeno delay para não sobrecarregar o processador e permitir que outras tasks executem.
    vTaskDelay(pdMS_TO_TICKS(1000)); // Verifica a cada segundo
  }
}
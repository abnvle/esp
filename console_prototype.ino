#include <WiFi.h>
#include <EEPROM.h>
#include <pgmspace.h>
#include <map>

#define EEPROM_SIZE 512
#define WIFI_SSID_ADDR 0
#define WIFI_PASS_ADDR 100
#define PWD_ADDR 200

const char WELCOME_MSG[] PROGMEM = "Witaj w konsoli ESP32!";
const char LOGIN_PROMPT[] PROGMEM = "Użyj komendy 'login', aby uzyskać dostęp do konfiguracji.";
const char SETUP_COMPLETE_MSG[] PROGMEM = "Zapisano konfigurację WiFi. Łączenie z siecią...";
const char UNKNOWN_CMD_MSG[] PROGMEM = "Nieznane polecenie";
const char LOGIN_REQ_MSG[] PROGMEM = "Musisz się zalogować. Użyj komendy 'login'.";
const char LOGIN_SUCCESS[] PROGMEM = "Zalogowano pomyślnie!";
const char LOGIN_FAIL[] PROGMEM = "Błędne hasło!";
const char LOGIN_MSG[] PROGMEM = "Podaj haslo:";
const char WIFI_SET_SSID[] PROGMEM = "Podaj nazwę sieci WiFi:";
const char WIFI_SET_PASS[] PROGMEM = "Podaj hasło do sieci WiFi:";
const char WIFI_STATUS_MSG[] PROGMEM = "Konfiguracja WiFi:";
const char WIFI_CONNECTED[] PROGMEM = "Połączono z WiFi!";
const char WIFI_CONNECT_FAIL[] PROGMEM = "Nie udało się połączyć z WiFi.";
const char WIFI_NOT_CONNECTED[] PROGMEM = "Nie połączono z WiFi.";
const char PWD_SET_PROMPT[] PROGMEM = "Podaj nowe hasło:";
const char PWD_SET_SUCCESS[] PROGMEM = "Hasło ustawione.";
const char PWD_CHANGE_PROMPT[] PROGMEM = "Podaj bieżące hasło:";
const char PWD_CHANGE_SUCCESS[] PROGMEM = "Hasło zostało zmienione.";
const char PWD_CHANGE_FAIL[] PROGMEM = "Błędne hasło. Nie można zmienić hasła.";
const char PWD_IS_SET_MSG[] PROGMEM = "Hasło jest już ustawione. Aby zmienić hasło, użyj komendy 'pwd_change'.";
const char REBOOT_MSG[] PROGMEM = "Restartowanie ESP...";
const char EXIT_MSG[] PROGMEM = "Wylogowano z trybu ustawień.";
const char ERASE_PROMPT[] PROGMEM = "Podaj hasło, aby potwierdzić wyczyszczenie ustawień:";
const char ERASE_FAIL[] PROGMEM = "Błędne hasło. Nie można wyczyścić ustawień.";
const char ERASE_SUCCESS[] PROGMEM = "Ustawienia zostały wyczyszczone.";
const char HELP_MSG[] PROGMEM = 
  "Dostępne polecenia:\n"
  "login - Zaloguj się do trybu ustawień.\n"
  "wifi_set - Ustawienia połączenia WiFi.\n"
  "wifi_status - Wyświetla status połączenia WiFi.\n"
  "pwd_set - Ustaw nowe hasło logowania.\n"
  "pwd_change - Zmień istniejące hasło logowania.\n"
  "reboot - Zrestartuj urządzenie.\n"
  "exit - Wyjdź z trybu ustawień.\n"
  "erase - Skasuj wszystkie ustawienia z EEPROM.\n"
  "help - Wyświetl listę dostępnych poleceń.";

char wifiSSID[32];
char wifiPass[32];
char loginPassword[32];

bool loggedIn = false;

typedef void (*CommandFunction)();
std::map<String, CommandFunction> cmd;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Wczytaj ustawienia z EEPROM
  readEEPROM(WIFI_SSID_ADDR, wifiSSID, sizeof(wifiSSID));
  readEEPROM(WIFI_PASS_ADDR, wifiPass, sizeof(wifiPass));
  readEEPROM(PWD_ADDR, loginPassword, sizeof(loginPassword));

  if (strlen(wifiSSID) > 0 && strlen(wifiPass) > 0) {
    connectWiFi();
  }

  setupSerial();

  cmd["wifi_set"] = handleWiFiSet;
  cmd["wifi_status"] = handleWiFiStatus;
  cmd["pwd_set"] = handlePwdSet;
  cmd["pwd_change"] = handlePwdChange;
  cmd["reboot"] = handleReboot;
  cmd["exit"] = handleExit;
  cmd["erase"] = handleErase;
  cmd["help"] = handleHelp;
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (loggedIn) {
      auto it = cmd.find(command);
      if (it != cmd.end()) {
        it->second();
      } else {
        Serial.println(FPSTR(UNKNOWN_CMD_MSG));
      }
    } else if (command == "login") {
      handleLogin();
    } else {
      Serial.println(FPSTR(LOGIN_REQ_MSG));
    }
  }
}

void setupSerial() {
  Serial.println(FPSTR(WELCOME_MSG));
  Serial.println(FPSTR(LOGIN_PROMPT));
}

void handleLogin() {
  if (strlen(loginPassword) > 0) {
    Serial.println(FPSTR(LOGIN_MSG));
    while (!Serial.available());
    String password = Serial.readStringUntil('\n');
    password.trim();

    if (password == loginPassword) {
      loggedIn = true;
      Serial.println(FPSTR(LOGIN_SUCCESS));
    } else {
      Serial.println(FPSTR(LOGIN_FAIL));
    }
  } else {
    loggedIn = true;
    Serial.println(F("Brak ustawionego hasła. Zalogowano bez hasła."));
  }
}

void handleWiFiSet() {
  Serial.println(FPSTR(WIFI_SET_SSID));
  while (!Serial.available());
  String ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println(FPSTR(WIFI_SET_PASS));
  while (!Serial.available());
  String pass = Serial.readStringUntil('\n');
  pass.trim();

  ssid.toCharArray(wifiSSID, sizeof(wifiSSID));
  pass.toCharArray(wifiPass, sizeof(wifiPass));
  saveEEPROM(WIFI_SSID_ADDR, wifiSSID, sizeof(wifiSSID));
  saveEEPROM(WIFI_PASS_ADDR, wifiPass, sizeof(wifiPass));

  Serial.println(FPSTR(SETUP_COMPLETE_MSG));

  connectWiFi();
}

void connectWiFi() {
  WiFi.begin(wifiSSID, wifiPass);
  Serial.print(F("Łączenie z WiFi"));
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(FPSTR(WIFI_CONNECTED));
    Serial.print(F("Adres IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(FPSTR(WIFI_CONNECT_FAIL));
  }
}

void handleWiFiStatus() {
  Serial.println(FPSTR(WIFI_STATUS_MSG));
  Serial.print(F("SSID: "));
  Serial.println(wifiSSID);
  Serial.print(F("Hasło: "));
  Serial.println(wifiPass);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Połączono z siecią WiFi."));
    Serial.print(F("Adres IP: "));
    Serial.println(WiFi.localIP());
    
    long rssi = WiFi.RSSI();
    Serial.print(F("Siła sygnału: "));
    Serial.print(rssi);
    Serial.println(F(" dBm"));
  } else {
    Serial.println(FPSTR(WIFI_NOT_CONNECTED));
  }
}

void handlePwdSet() {
  if (strlen(loginPassword) > 0) {
    Serial.println(FPSTR(PWD_IS_SET_MSG));
    return;
  }

  Serial.println(FPSTR(PWD_SET_PROMPT));
  while (!Serial.available());
  String password = Serial.readStringUntil('\n');
  password.trim();

  password.toCharArray(loginPassword, sizeof(loginPassword));
  saveEEPROM(PWD_ADDR, loginPassword, sizeof(loginPassword));
  Serial.println(FPSTR(PWD_SET_SUCCESS));
}

void handlePwdChange() {
  if (strlen(loginPassword) > 0) {
    Serial.println(FPSTR(PWD_CHANGE_PROMPT));
    while (!Serial.available());
    String currentPassword = Serial.readStringUntil('\n');
    currentPassword.trim();

    if (currentPassword != loginPassword) {
      Serial.println(FPSTR(PWD_CHANGE_FAIL));
      return;
    }
  }

  Serial.println(FPSTR(PWD_SET_PROMPT));
  while (!Serial.available());
  String newPassword = Serial.readStringUntil('\n');
  newPassword.trim();

  newPassword.toCharArray(loginPassword, sizeof(loginPassword));
  saveEEPROM(PWD_ADDR, loginPassword, sizeof(loginPassword));

  Serial.println(FPSTR(PWD_CHANGE_SUCCESS));
}

void handleReboot() {
  Serial.println(FPSTR(REBOOT_MSG));
  delay(1000);
  ESP.restart();
}

void handleExit() {
  loggedIn = false;
  Serial.println(FPSTR(EXIT_MSG));
}

void handleErase() {
  if (strlen(loginPassword) > 0) {
    Serial.println(FPSTR(ERASE_PROMPT));
    while (!Serial.available());
    String password = Serial.readStringUntil('\n');
    password.trim();

    if (password == loginPassword) {
           clearEEPROM();
      Serial.println(FPSTR(ERASE_SUCCESS));
    } else {
      Serial.println(FPSTR(ERASE_FAIL));
    }
  } else {
    clearEEPROM();
    Serial.println(FPSTR(ERASE_SUCCESS));
  }
}

void clearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void saveEEPROM(int addr, char* data, int len) {
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.commit();
}

void readEEPROM(int addr, char* data, int len) {
  for (int i = 0; i < len; i++) {
    data[i] = EEPROM.read(addr + i);
  }
}

void handleHelp() {
  Serial.println(FPSTR(HELP_MSG));
}

#include <WiFi.h>
#include "esp_wpa2.h"  // WPA2 Enterprise
#include "esp_wifi.h"

// ===== University of Vermont Wi-Fi SSID =====
const char* ssid = "UVM";

// ===== UVM Login Info =====
// Replace ONLY your password.
#define EAP_IDENTITY "MY_IDENTITY"   
#define EAP_USERNAME "USERNAME"           
#define EAP_PASSWORD "ENTER YOUR PASSWORD"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- UVM WiFi Connection (WPA2-Enterprise) ---");

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);

  // Configure WPA2 Enterprise parameters
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));

  // Enable WPA2 Enterprise (no config struct needed)
  esp_wifi_sta_wpa2_ent_enable();

  // Start connection
  Serial.printf("Connecting to SSID: %s\n", ssid);
  
  WiFi.begin(ssid);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(1000);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nSuccessfully connected to UVM Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nConnection failed.");
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
  }
}

void loop() {
  // nothing here
}

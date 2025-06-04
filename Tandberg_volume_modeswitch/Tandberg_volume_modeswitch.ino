/**
 * Tandberg Volume Control with Bluesound Integration
 * Upgraded for modern ESP8266 Arduino Core (3.x)
 * 
 * Hardware:
 * - NodeMCU ESP8266
 * - Rotary Encoder on pins D1, D2
 * - Mode switch button on pin D3
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Encoder.h>

// Network configuration
const char* ssid = "Eljest";        // Replace with your WiFi name
const char* password = "1111111119"; // Replace with your WiFi password

// Bluesound configuration
const String ipAddress = "http://192.168.50.49:11000/";
const String setAnalogInput = "Play?url=Capture%3Aplughw%3A0%2C0%2F48000%2F24%2F2%2Finput0";

// Hardware pins
Encoder volumeEncoder(D1, D2);
const int buttonPin = D3;

// Global variables
WiFiClient wifiClient;
HTTPClient http;
long oldPositionVolume = 0;
int oldButtonState = HIGH; // Initialize to HIGH for INPUT_PULLUP
int buttonState = HIGH;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50; // Debounce delay in milliseconds

// Connection status
bool wifiConnected = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 30000; // Check WiFi every 30 seconds

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Tandberg Volume Control Starting ===");
    
    // Initialize hardware
    pinMode(buttonPin, INPUT_PULLUP);
    
    // Connect to WiFi
    connectToWiFi();
    
    Serial.println("Setup complete!");
}

void loop() {
    // Check WiFi connection periodically
    checkWiFiConnection();
    
    // Handle button press with debouncing
    handleButton();
    
    // Handle volume encoder
    handleVolumeEncoder();
    
    // Small delay to prevent excessive polling
    delay(10);
}

void connectToWiFi() {
    // Debug info
    Serial.println();
    Serial.print("Attempting to connect to: ");
    Serial.println(ssid);
    Serial.print("Password length: ");
    Serial.println(strlen(password));
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // Clear any previous connection
    delay(1000);
    
    WiFi.begin(ssid, password);
    
    Serial.print("Connecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        // Print status every 10 attempts
        if (attempts % 10 == 0) {
            Serial.println();
            Serial.print("WiFi Status: ");
            printWiFiStatus();
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println();
        Serial.println("WiFi Connected Successfully!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal strength (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        wifiConnected = false;
        Serial.println();
        Serial.println("Failed to connect to WiFi");
        Serial.print("Final status: ");
        printWiFiStatus();
        
        // Scan for available networks
        scanNetworks();
    }
}

void printWiFiStatus() {
    switch(WiFi.status()) {
        case WL_IDLE_STATUS:
            Serial.println("WL_IDLE_STATUS");
            break;
        case WL_NO_SSID_AVAIL:
            Serial.println("WL_NO_SSID_AVAIL - Network not found");
            break;
        case WL_SCAN_COMPLETED:
            Serial.println("WL_SCAN_COMPLETED");
            break;
        case WL_CONNECTED:
            Serial.println("WL_CONNECTED");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("WL_CONNECT_FAILED - Wrong password?");
            break;
        case WL_CONNECTION_LOST:
            Serial.println("WL_CONNECTION_LOST");
            break;
        case WL_DISCONNECTED:
            Serial.println("WL_DISCONNECTED");
            break;
        default:
            Serial.print("Unknown status: ");
            Serial.println(WiFi.status());
            break;
    }
}

void scanNetworks() {
    Serial.println("Scanning for available networks...");
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks == 0) {
        Serial.println("No networks found");
    } else {
        Serial.print("Found ");
        Serial.print(numNetworks);
        Serial.println(" networks:");
        
        for (int i = 0; i < numNetworks; i++) {
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(" dBm) ");
            Serial.print(WiFi.encryptionType(i) == ENC_TYPE_NONE ? "Open" : "Encrypted");
            Serial.println();
        }
    }
    Serial.println();
}

void checkWiFiConnection() {
    unsigned long currentTime = millis();
    if (currentTime - lastWiFiCheck >= wifiCheckInterval) {
        lastWiFiCheck = currentTime;
        
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected) {
                Serial.println("WiFi connection lost, attempting to reconnect...");
                wifiConnected = false;
            }
            connectToWiFi();
        } else {
            wifiConnected = true;
        }
    }
}

void handleButton() {
    int reading = digitalRead(buttonPin);
    unsigned long currentTime = millis();
    
    // Debounce the button
    if (reading != buttonState && (currentTime - lastButtonPress) > debounceDelay) {
        buttonState = reading;
        lastButtonPress = currentTime;
        
        // Button pressed (LOW because of INPUT_PULLUP)
        if (buttonState == LOW && oldButtonState == HIGH) {
            Serial.println("Button pressed - switching to analog input");
            controlVolume(999); // Special code for analog input switch
        }
        
        oldButtonState = buttonState;
    }
}

void handleVolumeEncoder() {
    long newPositionVolume = volumeEncoder.read() / 4;
    
    if (newPositionVolume != oldPositionVolume) {
        long deltaVolume = oldPositionVolume - newPositionVolume;
        oldPositionVolume = newPositionVolume;
        
        // Get current volume
        int currentVolume = controlVolume(0);
        Serial.print("Current volume: ");
        Serial.println(currentVolume);
        
        Serial.print("Volume delta: ");
        Serial.println(deltaVolume);
        
        // Calculate new volume
        int newVolume = currentVolume;
        
        // Fix for Bluesound bug where volume 1 is reported as 0
        if (currentVolume == 0 && deltaVolume == 1) {
            newVolume = 2;
        } else {
            newVolume += deltaVolume;
        }
        
        // Constrain volume to valid range (0-100)
        newVolume = constrain(newVolume, 0, 100);
        
        // Set new volume
        int updatedVolume = controlVolume(newVolume);
        Serial.print("Updated volume: ");
        Serial.println(updatedVolume);
    }
}

int controlVolume(int newVolume) {
    int currentVolume = 0;
    
    if (!wifiConnected) {
        Serial.println("WiFi not connected, cannot control volume");
        return currentVolume;
    }
    
    String url;
    
    // Determine the URL based on the requested action
    if (newVolume == 999) {
        // Switch to analog input
        url = ipAddress + setAnalogInput;
        Serial.println("Switching to analog input");
    } else if (newVolume == 0) {
        // Get current volume
        url = ipAddress + "Volume";
    } else {
        // Set new volume
        url = ipAddress + "Volume?level=" + String(newVolume);
        Serial.print("Setting volume to: ");
        Serial.println(newVolume);
    }
    
    // Make HTTP request
    http.begin(wifiClient, url);
    http.setTimeout(5000); // 5 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            currentVolume = parseVolumeResponse(payload);
        } else {
            Serial.printf("HTTP request failed with code: %d\n", httpCode);
        }
    } else {
        Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return currentVolume;
}

int parseVolumeResponse(String content) {
    // Find the <volume> tag and extract the value
    int startTag = content.indexOf("<volume");
    if (startTag == -1) {
        Serial.println("Volume tag not found in response");
        return 0;
    }
    
    int endTag = content.indexOf("</volume>");
    if (endTag == -1) {
        Serial.println("Volume end tag not found in response");
        return 0;
    }
    
    // Find the closing > of the opening tag
    int tagClose = content.indexOf(">", startTag);
    if (tagClose == -1) {
        Serial.println("Malformed volume tag");
        return 0;
    }
    
    // Extract the volume value
    String volumeString = content.substring(tagClose + 1, endTag);
    volumeString.trim(); // Remove any whitespace
    
    Serial.print("Parsed volume string: '");
    Serial.print(volumeString);
    Serial.println("'");
    
    int volume = volumeString.toInt();
    
    // Validate volume range
    if (volume < 0 || volume > 100) {
        Serial.println("Warning: Volume out of expected range");
        volume = constrain(volume, 0, 100);
    }
    
    return volume;
}
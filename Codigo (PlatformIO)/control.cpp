#include <BLEDevice.h>
#include <Arduino.h>

#define VRX_PIN 0
#define VRY_PIN 1
#define SW_PIN  2

// UUIDs (Debe coincidir con el bot, aquí usamos el corto)
static BLEUUID serviceUUID("B07A");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean connected = false;
static BLEAdvertisedDevice* myDevice;
BLERemoteCharacteristic* pRemoteCharacteristic;

typedef struct { 
  int x; 
  int y; 
  bool btn; 
} JoystickData;

JoystickData myData;

// --- CALLBACK DEL CLIENTE (Para saber si el bot se apagó) ---
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}
  
  void onDisconnect(BLEClient* pclient) {
    Serial.println("¡Perdimos al Bot! Reiniciando búsqueda... 🔍");
    connected = false; // Corta el envío de datos y fuerza el escáner de nuevo
  }
};

// --- CALLBACK DEL ESCÁNER (Para encontrar al bot en el aire) ---
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.println("¡Bot encontrado en el aire!");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

// Función para enlazar
bool connectToServer() {
  Serial.println("Conectando al servidor...");
  BLEClient* pClient = BLEDevice::createClient();
  
  // Le asignamos el callback de desconexión al cliente
  pClient->setClientCallbacks(new MyClientCallback());
  
  if (!pClient->connect(myDevice)) return false;
  
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    return false;
  }
  
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    pClient->disconnect();
    return false;
  }
  
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  
  Serial.println("Iniciando BLE Client (Joystick)...");
  BLEDevice::init("");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("¡Conectado 100% y listo para enviar! 😎");
      connected = true;
    } else {
      Serial.println("Fallo al conectar...");
    }
    doConnect = false;
  }

  if (connected) {
    myData.x = analogRead(VRX_PIN);
    myData.y = analogRead(VRY_PIN);
    myData.btn = !digitalRead(SW_PIN);
    
    // Escribir datos
    pRemoteCharacteristic->writeValue((uint8_t*)&myData, sizeof(myData));
    delay(50); 
  } else {
    // Si se corta, escanea de nuevo
    BLEDevice::getScan()->start(5, false);
    delay(1000);
  }
}
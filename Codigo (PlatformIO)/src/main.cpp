#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Arduino.h>

// UUIDs de conexión (Deben coincidir con tu Emisor)
#define SERVICE_UUID        "B07A"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// 📌 DECLARACIÓN DE PINES PARA EL TB6612FNG (Ajusta según tu PCB)
#define PIN_PWMA 2 //5
#define PIN_AIN1 3 //0
#define PIN_AIN2 4 // 1
#define PIN_PWMB 5 //2
#define PIN_BIN1 1 //4
#define PIN_BIN2 0 //3

#define MAX_SPEED 220

// --- TU STRUCT MOTOR SHIELD ---
struct MotorShield {
    int left_pwm_pin, left_a_pin, left_b_pin;
    int right_pwm_pin, right_a_pin, right_b_pin;
    int MAX_SPEED_2 = 255;

    MotorShield(){}

    void init(int pwm_A, int a_1, int a_2, int pwm_B, int b_1, int b_2, int max_speed=255){
        left_pwm_pin = pwm_A;
        left_a_pin = a_1;
        left_b_pin = a_2;
        
        right_pwm_pin = pwm_B;
        right_a_pin = b_1;
        right_b_pin = b_2;
        
        MAX_SPEED_2 = max_speed;
        
        pinMode(left_pwm_pin, OUTPUT); pinMode(left_a_pin, OUTPUT); pinMode(left_b_pin, OUTPUT);
        pinMode(right_pwm_pin, OUTPUT); pinMode(right_a_pin, OUTPUT); pinMode(right_b_pin, OUTPUT);
        stopMotors();
    }

    void stopMotors(){
        controlMotors(0, 0);
    }

    void setMotorSpeed(int motor, int speed){
        bool reverse = speed<0;
        speed = constrain(abs(speed), 0, MAX_SPEED);

        if(motor == 0){
            //Left
            analogWrite(left_pwm_pin, speed);
            digitalWrite(left_a_pin, !reverse);
            digitalWrite(left_b_pin, reverse);
        }
        else{
            //Right
            analogWrite(right_pwm_pin, speed);
            digitalWrite(right_a_pin, !reverse);
            digitalWrite(right_b_pin, reverse);
        }
    }

    void controlMotors(int left_speed, int right_speed){
        setMotorSpeed(0, left_speed);
        setMotorSpeed(1, right_speed);
    }
};

// Instanciamos tu shield de motores
MotorShield shield;

// Estructura de datos del joystick
typedef struct { 
  int x; 
  int y; 
  bool btn; 
} JoystickData;

// --- CALLBACK PARA RECONEXIÓN AUTOMÁTICA Y SEGURIDAD ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("¡Joystick conectado! 🎮");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Se cortó la señal. ¡Freno de mano activado! 🚨");
      shield.stopMotors(); // ⚠️ SEGURIDAD: Si se desconecta, el bot se frena en el acto
      BLEDevice::startAdvertising(); 
    }
};

// --- CALLBACK DE CONTROL (Aquí ocurre la magia cinemática) ---
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() == sizeof(JoystickData)) {
        JoystickData* myData = (JoystickData*)rxValue.data();
        
        int rawX = myData->x;
        int rawY = myData->y;

        int forwardSpeed = 0;
        int steeringSpeed = 0;

        // 🛠️ FILTRO DE PUNTO MUERTO (DEADZONE)
        // Centro del ESP32 ADC es ~2048. Tolerancia de +/- 300
        if (abs(rawY - 2048) > 300) {
          // Mapeamos Y: Valores bajos -> Adelante (255), Valores altos -> Atrás (-255)
          forwardSpeed = map(rawY, 0, 4095, MAX_SPEED, -MAX_SPEED);
        }
        
        if (abs(rawX - 2048) > 300) {
          // Mapeamos X: Valores bajos -> Izquierda (-255), Valores altos -> Derecha (255)
          steeringSpeed = map(rawX, 0, 4095, -MAX_SPEED, MAX_SPEED);
        }

        // 🏎️ MEZCLA CINEMÁTICA (Arcade Drive)
        int leftMotor = forwardSpeed + steeringSpeed;
        int rightMotor = forwardSpeed - steeringSpeed;

        // Aseguramos que los valores calculados no se escapen de los límites del Shield
        leftMotor = constrain(leftMotor, -MAX_SPEED, MAX_SPEED);
        rightMotor = constrain(rightMotor, -MAX_SPEED, MAX_SPEED);

        // Si el joystick está en el centro absoluto, apagamos motores por completo
        if (forwardSpeed == 0 && steeringSpeed == 0) {
          shield.stopMotors();
        } else {
          // Mandamos la velocidad calculada a tu objeto
          shield.controlMotors(leftMotor, rightMotor);
        }

        // Monitor serie para debuggear en vivo
        Serial.printf("Joystick -> X:%hd Y:%hd | Motores -> L:%d R:%d\n", rawX, rawY, leftMotor, rightMotor);
      }
    }
};

void setup() {
  delay(3000); 
  Serial.begin(115200);

  // Inicializar tu objeto MotorShield con los pines definidos arriba
  shield.init(PIN_PWMA, PIN_AIN1, PIN_AIN2, PIN_PWMB, PIN_BIN1, PIN_BIN2, 255);

  Serial.println("Iniciando Bot con TB6612FNG...");

  // Inicializar servidor BLE
  BLEDevice::init("Bot A");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
                                       
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  
  Serial.println("¡Bot listo para las carreras! Esperando mando... 🏎️🔥");
}

void loop() {
  // Nada por aquí, el callback procesa los datos del puente H de manera asíncrona
  delay(100);
}
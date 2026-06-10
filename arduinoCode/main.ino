#include <SPI.h>
#include <Ethernet.h>

#define STEPS_PER_MM 1000
#define STEPS_PER_MM_LONG 22.219718800571

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 2);
unsigned int serverPort = 5010;
EthernetServer server(serverPort);
EthernetClient g_client;

// Arduino pin numbers
int dir = 8;
int stepPin = 9;
int dirLong = 3;
int stepPinLong = 2;
int lm1 = 5;
int lm1Long = 6;
int scanner = 4;

// Limit switches array - value not pointer
int limitSw[2] = {5, 6};

long stepCount[2] = {0, 0};
long maxSteps[2] = {150, 500};
float stepsPerMM[2] = {STEPS_PER_MM, STEPS_PER_MM_LONG};

// Register pins - axis 0 (short)
volatile uint8_t* stepR = &PORTB;
uint8_t stepRP = PB1;  // pin 9
volatile uint8_t* dirR = &PORTB;
uint8_t dirRP = PB0;  // pin 8

// Register pins - axis 1 (long)
volatile uint8_t* stepRL = &PORTD;
uint8_t stepRPL = PD2;  // pin 2
volatile uint8_t* dirRL = &PORTD;
uint8_t dirRPL = PD3;  // pin 3

// Scanner trigger
volatile uint8_t* trigPort = &PORTD;
uint8_t trigPin = PD4;  // pin 4

// Direction register arrays for easy indexing
volatile uint8_t* dirPorts[2] = { &PORTB, &PORTD };
uint8_t dirPins[2] = { PB0, PD3 };
volatile uint8_t* stepPorts[2] = { &PORTB, &PORTD };
uint8_t stepPins[2] = { PB1, PD2 };

unsigned long stepsCount = 0;

void setup() {
  setupComm();
  setupMotionCTRL();
}

void loop() {
  movenOnCommand();
  // homingBoth();
}

void moveInDis(float dis, unsigned int speed, int idx, int divisor, int record) {
  long maxSt = maxSteps[idx] * stepsPerMM[idx];
  int warmup[2] = {1000, 100};

  int direction = dis < 0 ? -1 : 1;
  dis < 0 ? *dirPorts[idx] &= ~(1 << dirPins[idx]) : *dirPorts[idx] |= (1 << dirPins[idx]);
  dis = dis < 0 ? -dis : dis;

  unsigned long st = (unsigned long)(dis * stepsPerMM[idx]);
  unsigned long moved = 0;
  

  while (moved < st && stepCount[idx] < maxSt) {
    if (digitalRead(limitSw[idx]) && moved > warmup[idx]) { break; }

    oneStep(speed, stepPorts[idx], stepPins[idx]);

    moved++;
    stepCount[idx] += direction;

    if (moved % divisor == 0 && idx != 1 && record == 1) {
      *trigPort |= (1 << trigPin);
      delayMicroseconds(200);
      *trigPort &= ~(1 << trigPin);
    }
  }
}


void oneStep(unsigned int us, volatile uint8_t* port, uint8_t pin) {
  TCNT1 = 0;
  OCR1A = us * 16 - 1;

  *port |= (1 << pin);

  while (!(TIFR1 & (1 << OCF1A)));
  TIFR1 |= (1 << OCF1A);
  *port &= ~(1 << pin);

  TCNT1 = 0;

  while (!(TIFR1 & (1 << OCF1A)));
  TIFR1 |= (1 << OCF1A);
}


void homing(int idx) {
  int speed[2] = {100, 500};
  *dirPorts[idx] &= ~(1 << dirPins[idx]);

  while (!digitalRead(limitSw[idx])) {
    oneStep(speed[idx], stepPorts[idx], stepPins[idx]);
  }

  stepCount[idx] = 0;
}

void homingBoth(){
  homing(0);
  delay(1000);
  homing(1);
  delay(1000);
}


void setupComm() {
  Serial.begin(9600);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.println("Server started, waiting for clients...");
}


void setupMotionCTRL() {
  pinMode(stepPin, OUTPUT);pinMode(dir, OUTPUT);
  pinMode(stepPinLong, OUTPUT);pinMode(dirLong, OUTPUT);
  pinMode(scanner, OUTPUT);digitalWrite(scanner, LOW);

  for (int i = 0; i < 2; i++){ pinMode(limitSw[i], INPUT_PULLUP); };

  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10);
  sei();
}


void movenOnCommand() {
    String command = receiveData();

    Serial.println(command);

    if (command.startsWith("MF") || command.startsWith("MB")) {
        float    dis    = command.substring(2, 5).toFloat();
        int      idx    = command.substring(6, 7).toInt();
        unsigned speed  = command.substring(8, 12).toInt();
        bool     record = command.substring(13).toInt();
        dis = command.substring(1, 2) == "F" ? dis : -1 * dis;

        moveInDis(dis, speed, idx, 40, record);
        g_client.println("DONE");  // send DONE after move finishes
    }

    if (command.startsWith("HO")) {
        homingBoth();
        g_client.println("DONE");
    }

    delay(100);
}

String receiveData() {
    if (!g_client || !g_client.connected())
        g_client = server.available();

    if (g_client && g_client.available()) {
        String command = g_client.readStringUntil('\n');
        command.trim();
        return command;  // no OK echo
    }
    return "-1";
}


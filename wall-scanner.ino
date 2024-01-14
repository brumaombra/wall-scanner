// codice per leggere il tempo che intercorre tra un fronte e l altro dei segnali del metal detector quando una certa distanza è stata misurata con il mouse ottico

// codice per ESP32

// il clock dell'ESP32 di default è 240MHz

// ATTENZIONE! Se il mouse non è collegato e/o non è propriamente alimentato, il codice si blocca nel setup aspettando che il mouse risponda

// per l'ESP32, quando richiama una funzione da interrupt, questa deve essere precaricata in RAM tramite l'attributo IRAM_ATTR (invece che ICACHE_RAM_ATTR per ESP826)

// coefficiente per mouse: 0.01151

// esempio di testo per creare un'immagine con python:
// dati = [((0, 0), 255), ((1, 0), 128), ((2, 0), 0),
//        ((0, 1), 0), ((1, 1), 64), ((2, 1), 255)]

#include <PS2MouseHandler.h>
#define MOUSE_DATA 5
#define MOUSE_CLOCK 17

PS2MouseHandler mouse(MOUSE_CLOCK, MOUSE_DATA, PS2_MOUSE_REMOTE);

#define TXPIN 22
#define RXPIN 21
#define REDLED 19
#define GREENLED 18
#define BUTTON 23
#define REDCH 0
#define YELLCH 1
#define NCM 3  // numero di cm ogni quanto fare una misura

// Dichiarazioni
unsigned int stato = 0;  // a che funzione sono, switch

volatile unsigned long timerCounter = 0;
volatile float delta = 0;
volatile float Fi0 = 0;
volatile int elapsedTime = 0;
volatile bool timerRunning = false;
volatile bool printed = true;
unsigned int i = 0;  // per fare la media
float soglia = 0.5;  // soglia per sensibilità per i LED

unsigned long prevMillis = millis();
int XVal = 0, YVal = 0;
int Xprec = 0, Yprec = 0;
float Xcm = 0, Ycm = 0;

bool first = true;  // per capire se è la prima iterazione
bool firstPython = true;
       

void IRAM_ATTR startTimer() {
  noInterrupts();
  if (!timerRunning) {
// se il timer non è attivo, azzero il contatore
    timerCounter = ESP.getCycleCount();
    timerRunning = true;
  }
  interrupts();
}

void IRAM_ATTR stopTimer() {
  if (timerRunning) {
    elapsedTime = ESP.getCycleCount()-timerCounter;
    noInterrupts();
    timerRunning = false;
    printed = false;
  }
}

void LedPWM() {
  if(delta>Fi0+soglia) {
    ledcWrite(REDCH, 10*(delta-(Fi0+soglia)));
    ledcWrite(YELLCH, 0);
  }
  if(delta<Fi0-soglia) {
    ledcWrite(YELLCH, 30*(Fi0-soglia-delta));
    ledcWrite(REDCH, 0);
  }
  if((delta>Fi0-soglia) && (delta<Fi0+soglia)) {
    ledcWrite(REDCH, 0);
    ledcWrite(YELLCH, 0);
  }
}

void stampaPython(int X, int Y, float mag){
    if(firstPython){
        firstPython = false;
        i = 0;
        Serial.print("dati = [");
    }
    Serial.print("((");
    Serial.print(X);
    Serial.print(", ");
    Serial.print(Y);
    Serial.print("), ");
    Serial.print(int((mag*8)-140));
    Serial.print("), ");
    i++;
    if(i = 3) {
        i = 0;
        Serial.println(" ");
    }
}

void stampaNormale(int X, int Y, float mag){
    //Serial.print("X: ");       // leggibile
    Serial.print(X);
    Serial.print(",");         // per CSV
    //Serial.print(" , Y: ");        // leggibile
    Serial.print(Y);
    Serial.print(",");            // per CSV
    //Serial.print(" , Mag: "); // leggibile
    Serial.println(mag, 1);
}
           
void setup() {
   
  noInterrupts();
       
  Serial.begin(115200);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(TXPIN, INPUT);
  pinMode(RXPIN, INPUT);
  pinMode(REDLED, OUTPUT);
  pinMode(GREENLED, OUTPUT);
   
  ledcSetup(REDCH, 1000, 8);
  ledcAttachPin(REDLED, REDCH);
  ledcSetup(YELLCH, 1000, 8);
  ledcAttachPin(GREENLED, YELLCH);

  if(mouse.initialise() != 0){
  // mouse error
  Serial.println("mouse error");
  };
   
  interrupts();
}

void loop() {
   
  switch(stato) {
    case 0:    // configurazione WiFi
      // qui configuro il wifi
      if(!digitalRead(BUTTON)){
        first = true;
        Serial.println("Pulsante premuto!");
        stato = 1;
        attachInterrupt(TXPIN, startTimer, RISING);
        attachInterrupt(RXPIN, stopTimer, FALLING);
      }
    break;
       
    case 1:    // taro la bobina
      if(first){
        first = false;
        delay(1000);
      }
      if(!printed) {
        if(i < 60){
          delta = delta + elapsedTime/240; // in micros
          elapsedTime = 0;
          i++;
          delayMicroseconds(200);
        } else {
            i = 0;
            delta = delta/61;
            Serial.println(delta, 1);
            printed = true;
            LedPWM();
            stato = 1;
           }
      }
      if(!digitalRead(BUTTON)){
        first = true;
        stato = 2;
        Serial.println("----------");
      }
      interrupts();
       
      break;
       
    case 2:    // Taro il delay
      if(!printed) {
        if(i < 5000){
          Fi0 = Fi0 + float(elapsedTime)/240; // in micros
          elapsedTime = 0;
          i++;
          delayMicroseconds(200);
          } else {
              i = 0;
              Fi0 = Fi0/5035;
              Serial.println(Fi0, 1);
              delay(1);
              Serial.println("----------");
              LedPWM();
              delay(1000);
              printed = true;
              first = true;
              stato = 3;
             }
       }
        interrupts();
        break;
       
        case 3:    // Controllo se mi sono mosso
        if(first){
            first = false;
            noInterrupts();
          }
       
        if (millis() - prevMillis > 200) {
    prevMillis = millis();
    LedPWM();
    mouse.get_data();

    XVal += mouse.x_movement();
    YVal += mouse.y_movement();
    Xcm = XVal*0.01151;
    Ycm = YVal*0.01151;
           
    if(Xprec != int(Xcm)/NCM | Yprec != int(Ycm)/NCM){
      Xprec = int(Xcm)/NCM;
      Yprec = int(Ycm)/NCM;
      stato = 4;
      interrupts();
    }
  }
        break;
       
        case 4:  // Misuro magnetismo e torno a misurare
          if(!printed) {
            if(i < 500){
              delta = delta + elapsedTime/240; // in micros
              elapsedTime = 0;
              i++;
              delayMicroseconds(200);
            } else {
                i = 0;
                delta = delta/501;
                stampaNormale(int(Xcm/NCM), int(Ycm/NCM), delta);
                //stampaPython(int(Xcm/NCM), int(Ycm/NCM), delta);
                printed = true;
                LedPWM();
                stato = 5;
                first = true;
               }
          }
        break;
       
        case 5:    // Invio file con WiFi
        // invio
        stato = 3;
        break;
       
        case 6:    //
       
        break;
    }
}
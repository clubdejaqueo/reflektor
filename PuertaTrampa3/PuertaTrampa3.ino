#include <AccelStepper.h>
#include <Wire.h>
#include <Stepper595.h>
#include <display16.h>
#include "variables.h"

#include <Adafruit_NeoPixel.h>

enum { PRENDIENDO, APAGANDO };

const int TIEMPO_ENCENDIDO = 1000;

class LuzCarga {
public:
  Adafruit_NeoPixel pixels;
  int intensidad;
  int estado;
  int sensor;
  unsigned long start_time;

  LuzCarga(int pin_datos, int input_sensor) : pixels(8, pin_datos, NEO_GRB + NEO_KHZ800), intensidad(0), estado(APAGANDO), sensor(input_sensor) {
  }
  
  void setup() {
    pixels.begin();
  }

  void llenar () {
    estado = PRENDIENDO;
    start_time = millis();
  }

  void reset() {
    estado = APAGANDO;
    intensidad = 0;
    actualizar();
  }
  
  void loop() {
    if (estado==APAGANDO && intensidad > 0) {
      intensidad--;
      actualizar();
    }
    if (estado==PRENDIENDO && intensidad < 255) {
      intensidad++;
      actualizar();
    }
    if (intensidad == 255 && millis() > start_time + TIEMPO_ENCENDIDO) {
      estado = APAGANDO;
    }
  }
  
  void actualizar() {
    for(uint16_t i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, pixels.Color(0, intensidad, 0));
    }
    pixels.show();
  }

  int lectura_sensor() {
    if (intensidad == 0) {
      int lectura = analogRead(sensor);
      return lectura;
    } else {
      // si la tira de leds esta prendida, ignorar el valor
      return 0;
    }
  }
};

// PENDIENTES:
// demorar un ratito en ganar [  ]
// usar variables en todos lados [./]

#define DEBUG

const byte FIN_CARRERA_PIN = 7;
const byte BOTON_CAMBIO = 5;
const byte BOTON_IZQUIERDO = 6;
const byte BOTON_DERECHO = 4;

const byte DISPLAY_LEN = 5;

const byte ENABLE_5484 = 9;
const byte CLOCK_5484  = 12;
const byte DATA_5484   = 10;

const byte SER_Pin   = 10;   //pin 14 on the 74HC595   
const byte RCLK_Pin  = 11;   //pin 12 on the 74HC595   
const byte SRCLK_Pin = 12;   //pin 11 on the 74HC595

const byte LED_ESPEJO1 = 16; // 17 y 18 son los otros espejos
const byte LED_GANASTE = 21;

const byte LED_SENSOR0 = 22;
const byte LED_SENSOR1 = 23;
const byte ENABLE_LASER = 20;

const byte SENSOR0 = A2;
const byte SENSOR1 = A3;

const byte PIN_LUZCARGA0 = 2;
const byte PIN_LUZCARGA1 = 3;

const int ENERGIA_INICIAL = 10000;

Display16 display(DISPLAY_LEN, ENABLE_5484, CLOCK_5484, DATA_5484);
StepperTrain Stepper::train = StepperTrain();
Stepper espejo1(0, 1, 2, 3);
Stepper espejo2(4, 5, 6, 7);
Stepper espejo3(8, 9, 10, 11);  
Stepper carrito(15, 14, 13, 12);

typedef enum {ABRIENDO, CERRANDO, CERRADA} EstadosPuerta;
EstadosPuerta estado_puerta;
int espejo_activo;


class State {
public:
    static State* current_state;
    static void change_state(State* new_state);
    virtual const char* name() = 0;
    virtual void setup() = 0;
    virtual void loop() {}
};

void State::change_state (State* new_state) {
#ifdef DEBUG
    Serial.println(new_state->name());
    //while (digitalRead(DEBUG_PIN) == LOW) {
    //  delay(50);
    //}
#endif
    State::current_state = new_state;
    State::current_state->setup();
}

void steppers_go () {
    unsigned long now = millis();
    carrito.go(now);
    espejo1.go(now);
    espejo2.go(now);
    espejo3.go(now);
    Stepper::train.writeRegisters();
}

boolean inline fin_de_carrera_activado() {
    return digitalRead(FIN_CARRERA_PIN) == LOW;
}

class GameoverState : public State {
protected:
    unsigned long start_time;
    unsigned long last_change;
    int pos;
public:
    const char* name() {
      return "Game Over";
    }
    void apagar_todo();
    void setup();
    void loop();
} gameover_state;

class GanasteState : public GameoverState {
public:
    const char* name() {
      return "Ganaste";
    }
    void setup();
    void loop();
} ganaste_state;

class PlayState : public State {
    unsigned long last_change;
    int n;
    void verificar_sensor0();
    void verificar_sensor1();
    void mover_espejo(int espejo, int direccion);
    void procesar_botones_izquierda_derecha();
    void mostrar_espejo_activo();
    void procesar_boton_cambio_espejo();
    void procesar_botonera();
    void scrollear_texto();
    void mostrar_energia();
    unsigned long tiempo_inicial;
public:
    const char* name() {
      return "Running Game";
    }
    void setup();
    void loop();
} play_state;

class ResetState : public State {
    void ir_al_fin_carrera();
    void cerrar_puerta();
public:
    const char* name() {
      return "Initial Reset";
    }
    void setup();
    void loop();
} reset_state;

class AttractState : public State {
    unsigned long last_change;
    int n;
    void revisar_botones();
    void scrollear_texto();
public:
    const char* name() {
      return "Atract Mode";
    }
    void setup();
    void loop();
} attract_state;

class ThanksState : public State {
    unsigned long last_change;
    int n;
    void revisar_botones();
    void scrollear_texto();
    boolean algun_boton();
public:
    const char* name() {
      return "Atract Mode";
    }
    void setup();
    void loop();
} thanks_state;

class InputInitialsState : public State {
  unsigned long last_change_time;
  int cursor;
  byte initials[DISPLAY_LEN];
  char buffer[DISPLAY_LEN];
  byte prev_keystatus;
  boolean space_was_pressed;
  int display_delay;
  
  void convert_buffer();
  boolean is_blinking();
  void display_cursor();
  void update_display();
  void change_display(int d);
  inline byte getKeystatus();

public:
  const char* name() {
      return "InputInitialsState";
  }
  void setup();
  void loop();
} inputinitials_state;

State* State::current_state = &inputinitials_state;
LuzCarga carga0(PIN_LUZCARGA0, SENSOR0);
LuzCarga carga1(PIN_LUZCARGA1, SENSOR1);

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
#ifdef DEBUG
    Serial.println("hello board");
    //inputString.reserve(200);
    update_variables();
#endif
    
    Serial.println("-------------------------");
    stop_audio();

/*
    se hacen por 595
    pinMode(LED_ESPEJO1 + 0, OUTPUT);
    pinMode(LED_ESPEJO1 + 1, OUTPUT);
    pinMode(LED_ESPEJO1 + 2, OUTPUT);
    pinMode(LED_GANASTE, OUTPUT);
*/

    pinMode(FIN_CARRERA_PIN, INPUT_PULLUP);
    pinMode(BOTON_CAMBIO, INPUT_PULLUP);
    pinMode(BOTON_IZQUIERDO, INPUT_PULLUP);
    pinMode(BOTON_DERECHO, INPUT_PULLUP);
    
    Stepper::train.setup(SER_Pin, RCLK_Pin, SRCLK_Pin);
    
    //pinMode(DEBUG_PIN, OUTPUT);
    //digitalWrite(DEBUG_PIN, HIGH);

    Stepper::train.clearRegisters();
    Stepper::train.writeRegisters();

    carga0.setup();
    carga1.setup();
    State::change_state(&reset_state);
}

void loop() {
    State::current_state->loop();
    carga0.loop();
    carga1.loop();
    steppers_go();
#ifdef DEBUG
    if (DELAY_LOOP > 0) {
        delay(DELAY_LOOP);
    }
#endif
}



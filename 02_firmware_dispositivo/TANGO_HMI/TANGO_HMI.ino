/*
  TANGO HMI local - TFT 480 x 320 - ESP32 clasico - 20/09/2026
  ---------------------------------------------------------------------------
  UN SOLO SKETCH. Abrir como TANGO_HMI/TANGO_HMI.ino en Arduino IDE.
  Dependencias: TFT_eSPI + SPI (incluida en el core ESP32).
  Objetivo: ESP32 Dev Module (ESP32 clasico), Arduino ESP32 + TFT_eSPI.
  Esta revision se verifica en C++ host con fuentes reales de TFT_eSPI 2.5.44.
  No se realizo compilacion ESP32 ni carga/prueba fisica de esta revision.
  Conserva User_Setup.h, cableado y rotacion 1 del proyecto recibido.
  Conserva los pines TFT existentes. El backlight se controla por PWM en GPIO25.

  Prueba con el Python existente: Serial 115200, U/D/L/R/E.
  21 escenarios estaticos + 1 simulacion secuencial: MENU > SISTEMA >
  ESCENARIOS DEMO. U/D seleccionan una fila, L/R cambian de pagina y E carga.
  La simulacion secuencial recorre un procedimiento DEMO y cierra TNG-0013.
  Atajos conservados: 0 normal; 1 bateria baja; 2 perdida + baja; 9 terminado.
  ENABLE_DEMO_CONTROLS=false oculta el selector y deshabilita estos atajos.
  Los numeros cambian DATOS de demostracion, incluso estando bloqueado;
  NO son teclas de navegacion, NO accionan bomba/valvula.

  Detalles: L/R cambian de pagina mientras el foco esta en el selector superior.
  D baja al footer; U vuelve al selector. En el footer L/R mueven el foco.
  El candado retiene el foco: E bloquea, E desbloquea; ignora las flechas.

  Datos sinteticos. No hay algoritmo clinico, umbrales, lectura de sensores,
  escritura en SD implementada. Para la muestra, tres registros sinteticos viven
  en memoria y pueden consultarse por USB/Serial con @LIST y @GET:<ID>. Esto NO
  demuestra persistencia en microSD; esa integracion sigue pendiente.
  El brillo controla fisicamente el backlight por PWM; el audio sigue como hook.
  AUDIO_MUTE_MS es ilustrativo, no un parametro clinico validado.

  Fuentes FreeSansBold 7-bit: textos de pantalla sin tildes para evitar glifos
  ausentes. Se conservan las palabras (PRESION EJERCIDA, TIEMPO DE OCLUSION).
  Audio, candados y triangulo: SVG de Bootstrap Icons convertidos a 24x24,
  MSB primero (licencia mas abajo). Arrays generados, no escritos a mano.
  Verde: candado cerrado/audio activo. Naranja: abierto/audio silenciado.
  Rojo: alarma. Amarillo: aviso. El foco sigue siendo azul-gris, independiente.
  Los colores se crean con color565(); no se cambia el orden RGB del driver.
*/
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Usa tu Free_Fonts.h si ya esta en la carpeta. Los cuatro alias de respaldo
// permiten abrir este unico archivo sin tener que copiar un segundo .h.
#if __has_include("Free_Fonts.h")
  #include "Free_Fonts.h"
#endif
#ifndef LOAD_GFXFF
  #error "TFT_eSPI debe tener LOAD_GFXFF habilitado, como en el sketch original."
#endif
#ifndef FSS9
  #define FSS9   &FreeSans9pt7b
#endif
#ifndef FSSB9
  #define FSSB9  &FreeSansBold9pt7b
#endif
#ifndef FSSB12
  #define FSSB12 &FreeSansBold12pt7b
#endif
#ifndef FSSB18
  #define FSSB18 &FreeSansBold18pt7b
#endif
#ifndef FSSB24
  #define FSSB24 &FreeSansBold24pt7b
#endif

// ====================== 1. CONFIGURACION DE LA DEMO =========================
static constexpr uint32_t AUDIO_MUTE_MS = 30000UL;
static constexpr uint32_t ZERO_DISPLAY_MS = 1000UL;
// false = instantes fijos del PPT. true = avanza SOLO los relojes sinteticos.
static constexpr bool ADVANCE_DEMO_CLOCKS = false;
static constexpr bool ENABLE_DEMO_CONTROLS = true;
// Backlight del modulo ILI9488/MSP3520: mover el cable LED de 5V a GPIO25.
// El modulo incorpora su etapa de conmutacion; este pin recibe la senal PWM.
static constexpr uint8_t TFT_LED_PIN = 25;
static constexpr int16_t SCREEN_W = 480, SCREEN_H = 320;
static constexpr uint32_t TIME_UNAVAILABLE = UINT32_MAX;
static_assert(AUDIO_MUTE_MS > 0 && AUDIO_MUTE_MS < 3600000UL,
              "La demo de audio debe durar menos de una hora.");

// ====================== 2. MODELO DE ESTADO ================================
enum class NavEvent : uint8_t { Up, Down, Left, Right, Enter };
enum class Screen : uint8_t {
  Principal, Menu, Detalles, Alertas, Sistema, Ajustes, Informacion,
  Registros, Transferir, Comprobacion, Escenarios
};
enum class Operation : uint8_t {
  Preparado, Inflando, Buscando, OclusionAlcanzada, Reajustando,
  ControlEnPausa, Comprobando, Terminado, InicioBloqueado
};
enum class SensorStatus : uint8_t { OK, NoDisponible, Comprobando, SinVerificar };
enum class SelfTestStatus : uint8_t { EnCurso, Superada, Fallida };
enum class Severity : uint8_t { Aviso, Alarma };
enum class Condition : uint8_t {
  BateriaBaja, SenalNoConfiable, PresionNoSostenida, Sobrepresion,
  PresionNoAlcanzada, OclusionNoAlcanzada, BateriaCritica,
  PresionNoDisponible, PerdidaOclusion, AutocomprobacionFallida, Count
};
struct ConditionDefinition {
  const char *name;
  const char *description;   // Solo se muestra en ALERTAS ACTIVAS.
  Severity severity;
  uint8_t priority;          // Orden VISUAL ilustrativo, pendiente de validacion.
  bool audible;
};
static const ConditionDefinition CONDITIONS[] = {
  {"BATERIA BAJA", "Carga reducida; verificar autonomia disponible.", Severity::Aviso, 90, true},
  {"SENAL NO CONFIABLE", "Senal pulsatil insuficiente para evaluar oclusion.", Severity::Aviso, 70, true},
  {"PRESION NO SOSTENIDA", "La presion aplicada no se mantiene estable.", Severity::Aviso, 80, true},
  {"SOBREPRESION", "La presion supero el limite configurado.", Severity::Alarma, 10, true},
  {"PRESION NO ALCANZADA", "No se alcanzo la presion objetivo.", Severity::Alarma, 50, true},
  {"OCLUSION NO ALCANZADA", "No se alcanzo el criterio de oclusion.", Severity::Alarma, 40, true},
  {"BATERIA CRITICA", "Carga muy baja; autonomia limitada.", Severity::Alarma, 60, true},
  {"PRESION NO DISPONIBLE", "No hay una medicion valida de presion.", Severity::Alarma, 20, true},
  {"PERDIDA DE OCLUSION", "Se perdio el criterio de oclusion previo.", Severity::Alarma, 30, true},
  {"AUTOCOMPROBACION FALLIDA", "La comprobacion inicial no fue superada.", Severity::Alarma, 25, true}
};
// Perdida de oclusion presupone oclusion previa; no alcanzada = nunca lograda.
// Criterios de deteccion/recuperacion PENDIENTES, no se infiere flujo cero.

struct DeviceState {
  Operation operation = Operation::OclusionAlcanzada;
  int16_t pressure = 238;             // mmHg; -1 significa no disponible
  int16_t pressureAtOcclusion = 232;
  int16_t maximumPressure = 242;
  uint32_t occlusionSeconds = 420;
  uint32_t timeToOcclusionSeconds = 24;
  uint16_t readjustments = 1;
  uint32_t lastReadjustmentSeconds = 6;
  uint32_t ongoingReadjustmentSeconds = TIME_UNAVAILABLE;
  uint8_t batteryPercent = 76;
  SensorStatus pressureSensor = SensorStatus::OK;
  SelfTestStatus selfTest = SelfTestStatus::Superada;
  uint16_t activeConditions = 0;      // bit por Condition, NO prefijos en UI
  bool procedureActive = true;
  bool recordingActive = true;
  bool demoSdAvailable = true;        // dato de maqueta, NO resultado de SD.begin
  char recordId[16] = "TNG-0013";
  uint16_t savedRecords = 2;
};
// Un instante completo por escenario. Las vistas leen SIEMPRE el mismo estado.
// La severidad/orden visual son ilustrativos; no constituyen umbrales validados.
struct DemoScenario {
  const char *key;
  const char *label;
  Operation operation;
  int16_t pressure, firstPressure, maximumPressure;
  uint32_t occlusionSeconds, timeToOcclusionSeconds;
  uint16_t readjustments;
  uint32_t lastReadjustmentSeconds, ongoingReadjustmentSeconds;
  uint8_t battery;
  uint16_t conditions;
  SensorStatus sensor;
  SelfTestStatus selfTest;
  bool recording, initiallyMuted;
  uint16_t savedRecords;
};
static const DemoScenario DEMO_SCENARIOS[] = {
  {"checking", "Comprobacion inicial", Operation::Comprobando, -1, -1, -1, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 0, SensorStatus::Comprobando, SelfTestStatus::EnCurso, false, false, 2},
  {"prepared", "Preparado", Operation::Preparado, 0, -1, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 0, SensorStatus::OK, SelfTestStatus::Superada, false, false, 2},
  {"inflating", "Inflando torniquete", Operation::Inflando, 84, -1, 84, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 0, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"searching", "Buscando oclusion", Operation::Buscando, 186, -1, 186, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 0, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"normal", "Oclusion alcanzada", Operation::OclusionAlcanzada, 238, 232, 242, 420, 24, 1, 6, TIME_UNAVAILABLE, 76, 0, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"readjust", "Reajustando sin alarma", Operation::Reajustando, 224, 232, 242, 432, 24, 1, 6, 3, 76, 0, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"recovered", "Oclusion recuperada", Operation::OclusionAlcanzada, 238, 232, 242, 435, 24, 2, 6, TIME_UNAVAILABLE, 76, 0, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"low_battery", "Bateria baja", Operation::OclusionAlcanzada, 238, 232, 242, 491, 24, 1, 6, TIME_UNAVAILABLE, 12, 1, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"unreliable", "Senal no confiable", Operation::Buscando, 204, -1, 204, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 76, 2, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"unstable", "Presion no sostenida", Operation::Reajustando, 186, 232, 242, 438, 24, 1, 6, 5, 76, 4, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"overpressure", "Sobrepresion", Operation::ControlEnPausa, 318, 232, 318, 220, 24, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 76, 8, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"pressure_unreached", "Presion no alcanzada", Operation::ControlEnPausa, 96, -1, 96, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 76, 16, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"occlusion_unreached", "Oclusion no alcanzada", Operation::ControlEnPausa, 270, -1, 270, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 76, 32, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"critical_battery", "Bateria critica", Operation::OclusionAlcanzada, 238, 232, 242, 520, 24, 1, 6, TIME_UNAVAILABLE, 5, 64, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"sensor_failure", "Presion no disponible", Operation::ControlEnPausa, -1, -1, -1, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 76, 128, SensorStatus::NoDisponible, SelfTestStatus::Superada, true, false, 2},
  {"loss_only", "Perdida de oclusion", Operation::Reajustando, 204, 232, 242, 438, 24, 1, 6, 5, 76, 256, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"overpressure_low", "Sobrepresion + bateria baja", Operation::ControlEnPausa, 318, 232, 318, 491, 24, 1, 6, TIME_UNAVAILABLE, 12, 9, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"loss_active", "Perdida + baja: audio activo", Operation::Reajustando, 204, 232, 242, 438, 24, 1, 6, 5, 12, 257, SensorStatus::OK, SelfTestStatus::Superada, true, false, 2},
  {"loss", "Perdida + baja: silenciado", Operation::Reajustando, 204, 232, 242, 438, 24, 1, 6, 5, 12, 257, SensorStatus::OK, SelfTestStatus::Superada, true, true, 2},
  {"selftest_failure", "Autocomprobacion fallida", Operation::InicioBloqueado, -1, -1, -1, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 512, SensorStatus::SinVerificar, SelfTestStatus::Fallida, false, false, 2},
  {"idle", "Procedimiento terminado", Operation::Terminado, -1, -1, -1, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 0, TIME_UNAVAILABLE, TIME_UNAVAILABLE, 82, 0, SensorStatus::OK, SelfTestStatus::Superada, false, false, 3},
};
static constexpr uint8_t DEMO_SCENARIO_COUNT = sizeof(DEMO_SCENARIOS)/sizeof(DEMO_SCENARIOS[0]);

// ====================== REGISTROS DEMO EN MEMORIA ===========================
// Representan procedimientos completos, no pantallas individuales. La microSD
// real se integrara despues conservando este mismo contrato de datos.
struct ReadjustmentEvent {
  uint32_t tSeconds;
  uint32_t durationSeconds;
  int16_t fromPressure;
  int16_t toPressure;
};
struct AlertEvent {
  uint32_t tSeconds;
  Condition condition;
};
struct DemoRecord {
  const char *id;
  const char *label;
  uint32_t sessionDurationSeconds;
  uint32_t occlusionTotalSeconds;
  int16_t pressureAtOcclusion;
  int16_t maximumPressure;
  uint32_t timeToOcclusionSeconds;
  const ReadjustmentEvent *readjustments;
  uint8_t readjustmentCount;
  const AlertEvent *alerts;
  uint8_t alertCount;
};

static const ReadjustmentEvent RECORD_0012_READJUSTMENTS[] = {
  {180, 6, 224, 238}
};
static const ReadjustmentEvent RECORD_0013_READJUSTMENTS[] = {
  {180, 6, 224, 238},
  {430, 5, 204, 238}
};
static const AlertEvent RECORD_0013_ALERTS[] = {
  {430, Condition::PerdidaOclusion},
  {491, Condition::BateriaBaja}
};
static const DemoRecord DEMO_RECORDS[] = {
  {"TNG-0011", "Procedimiento normal", 452, 420, 228, 236, 22, nullptr, 0, nullptr, 0},
  {"TNG-0012", "Procedimiento con reajuste", 470, 435, 232, 242, 24,
   RECORD_0012_READJUSTMENTS, 1, nullptr, 0},
  {"TNG-0013", "Perdida de oclusion + bateria baja", 520, 496, 232, 242, 24,
   RECORD_0013_READJUSTMENTS, 2, RECORD_0013_ALERTS, 2}
};
static constexpr uint8_t DEMO_RECORD_COUNT = sizeof(DEMO_RECORDS)/sizeof(DEMO_RECORDS[0]);

// Tiempo real de la demostracion acelerada. Los valores mostrados/registrados
// son sinteticos y NO representan tiempos clinicos validados.
static const uint32_t SEQUENCE_AT_MS[] = {
  0, 2000, 4000, 7000, 10000, 16000, 19000, 25000, 28000, 31000, 34000, 37000
};
static constexpr uint8_t SEQUENCE_STEP_COUNT = sizeof(SEQUENCE_AT_MS)/sizeof(SEQUENCE_AT_MS[0]);

struct Rect { int16_t x, y, w, h; };
struct Palette {
  uint16_t bg, white, navy, text, muted, border, green, greenLight;
  uint16_t amber, amberBg, red, redBg, orange, focus, disabled, disabledText;
};

// 24 x 24, 1 bit/pixel, MSB primero: 3 bytes por fila, 72 por icono.
// Todos los iconos: Bootstrap Icons. Siluetas rasterizadas desde los SVG fuente.
// https://icons.getbootstrap.com/icons/volume-up-fill/
// https://icons.getbootstrap.com/icons/volume-mute-fill/
// https://icons.getbootstrap.com/icons/lock-fill/
// https://icons.getbootstrap.com/icons/unlock-fill/
// https://icons.getbootstrap.com/icons/exclamation-triangle-fill/
/*
The MIT License (MIT)

Copyright (c) 2019-2024 The Bootstrap Authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
static const uint8_t SOUND_BITMAP[72] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
  0x00, 0x00, 0x60, 0x00, 0x40, 0x60, 0x00, 0xE3, 0x30, 0x01, 0xE1, 0x90,
  0x3F, 0xEC, 0x90, 0x7F, 0xE6, 0xD8, 0x7F, 0xE6, 0xD8, 0x7F, 0xE6, 0xD8,
  0x7F, 0xE6, 0xD8, 0x7F, 0xE6, 0xD8, 0x7F, 0xE6, 0xD8, 0x3F, 0xEC, 0x90,
  0x01, 0xE1, 0x90, 0x00, 0xE3, 0x30, 0x00, 0x40, 0x60, 0x00, 0x00, 0x60,
  0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t MUTED_BITMAP[72] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xE0, 0x00, 0x01, 0xE0, 0x00,
  0x3F, 0xE2, 0x08, 0x7F, 0xE3, 0x18, 0x7F, 0xE1, 0xB0, 0x7F, 0xE0, 0xE0,
  0x7F, 0xE0, 0xE0, 0x7F, 0xE1, 0xB0, 0x7F, 0xE3, 0x18, 0x3F, 0xE2, 0x08,
  0x01, 0xE0, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t LOCKED_BITMAP[72] PROGMEM = {
  0x00, 0x3C, 0x00, 0x00, 0xFF, 0x00, 0x01, 0x81, 0x80, 0x01, 0x00, 0x80,
  0x03, 0x00, 0xC0, 0x03, 0x00, 0xC0, 0x03, 0x00, 0xC0, 0x03, 0x00, 0xC0,
  0x03, 0x00, 0xC0, 0x07, 0xFF, 0xE0, 0x0F, 0xFF, 0xF0, 0x1F, 0xFF, 0xF8,
  0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8,
  0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8,
  0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x0F, 0xFF, 0xF0, 0x07, 0xFF, 0xE0,
};
static const uint8_t UNLOCKED_BITMAP[72] PROGMEM = {
  0x00, 0x00, 0xF0, 0x00, 0x03, 0xFC, 0x00, 0x06, 0x06, 0x00, 0x04, 0x02,
  0x00, 0x0C, 0x03, 0x00, 0x0C, 0x03, 0x00, 0x0C, 0x03, 0x00, 0x0C, 0x03,
  0x00, 0x0C, 0x03, 0x3F, 0xFF, 0x01, 0x7F, 0xFF, 0x80, 0xFF, 0xFF, 0xC0,
  0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0,
  0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0,
  0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xC0, 0x7F, 0xFF, 0x80, 0x3F, 0xFF, 0x00,
};
static const uint8_t ALERT_BITMAP[72] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00,
  0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x80,
  0x01, 0xE7, 0x80, 0x03, 0xE7, 0xC0, 0x03, 0xE7, 0xC0, 0x07, 0xE7, 0xE0,
  0x07, 0xE7, 0xE0, 0x0F, 0xE7, 0xF0, 0x1F, 0xE7, 0xF8, 0x1F, 0xFF, 0xF8,
  0x3F, 0xFF, 0xFC, 0x3F, 0xE7, 0xFC, 0x7F, 0xE7, 0xFE, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
};

// ====================== 3. HMI / NAVEGACION ================================
class TangoHMI {
public:
  TFT_eSPI tft;
  TFT_eSprite patch;
  DeviceState state;
  bool audioMuted = false;
  bool screenLocked = false;
  bool audioRequested = false;        // solicitud logica hacia futuro buzzer
  uint8_t brightnessPercent = 60;

  TangoHMI() : patch(&tft) {}

  void begin() {
    Serial.begin(115200);
    pinMode(TFT_LED_PIN, OUTPUT);
    applyPhysicalBacklight(brightnessPercent);
    tft.init();
    tft.setRotation(1);               // IGUAL al sketch original
    initPalette();
    // Un parche de 444 x 56 x 2 = 49.728 bytes; NO framebuffer 480 x 320.
    // Se reserva UNA vez, no hay asignaciones al navegar ni al contar segundos.
    patch.setColorDepth(16);
    patchReady = patch.createSprite(444, 56) != nullptr;
    if (!patchReady) Serial.println("Sin sprite: actualizacion parcial directa.");
    currentScreen = Screen::Principal;
    buildNavigation();
    focus = C_MENU;
    needsScreenEntry = true;
    lastSimulationTick = millis();
    updateAudioOutput();
    refreshDirtyWidgets();
    Serial.println("TANGO HMI: U D L R E. Escenarios demo + simulacion secuencial (S).");
    Serial.println("Web local: @LIST y @GET:TNG-0011. microSD fisica aun pendiente.");
  }

  void tick() {
    readNavigation();
    updateTimers(millis());
    updateSimulation(millis());
    refreshDirtyWidgets();
  }

  // Un futuro lector de pulsadores llama exactamente a esta funcion.
  void dispatchNavigation(NavEvent event) {
    if (screenLocked) {
      if (event == NavEvent::Enter && focus == C_LOCK) {
        screenLocked = false;
        markControl(C_LOCK);
      }
      return;
    }
    // Selector de paginas: foco sobre flechas/cabecera del contenido.
    if ((currentScreen == Screen::Detalles || currentScreen == Screen::Alertas || currentScreen == Screen::Escenarios)
        && (focus == C_PREV || focus == C_NEXT)
        && (event == NavEvent::Left || event == NavEvent::Right)) {
      changePage(event == NavEvent::Left ? -1 : 1);
      return;
    }
    // En el catalogo, L/R tambien cambian pagina con el foco en una fila.
    if(currentScreen==Screen::Escenarios && focus>=C_CASE0 && focus<=C_CASE2 &&
       (event==NavEvent::Left || event==NavEvent::Right)) {
      changePage(event==NavEvent::Left ? -1 : 1); return;
    }
    if (event == NavEvent::Enter) { activateFocused(); return; }
    int direction = static_cast<int>(event); // Up/Down/Left/Right = 0..3
    int8_t next = nodes[focus].neighbor[direction];
    // Solo salta controles deshabilitados en la MISMA direccion, nunca circula.
    for (uint8_t guard = 0; guard < CONTROL_COUNT && next >= 0; ++guard) {
      if (nodes[next].visible && nodes[next].enabled) { setFocus(next); return; }
      next = nodes[next].neighbor[direction];
    }
  }

  // Integracion futura: actualizar el MODELO, nunca llamar drawScreen().
  // Llamar desde loop/tarea propietaria de la HMI, no desde ISR.
  void setDeviceState(const DeviceState &value) {
    state = value;
    if (state.batteryPercent > 100) state.batteryPercent = 100;
    state.activeConditions &= (1U << static_cast<uint8_t>(Condition::Count)) - 1U;
    state.recordId[sizeof(state.recordId)-1] = '\0';
    // Una condicion NUEVA audible interrumpe el silencio de esta demostracion.
    // No confundir esta politica ilustrativa con una regla clinica validada.
    if (state.activeConditions & ~lastAudioConditions) cancelMute();
    lastAudioConditions = state.activeConditions;
    if (!hasAudibleCondition()) cancelMute();
    updateAudioOutput();
    if (currentScreen == Screen::Transferir && !canTransfer()) {
      // Los datos pueden cambiar con pantalla bloqueada; no forzar navegacion.
      // La vista de transferencia muestra entonces "No disponible".
      contentDirty = true;
    }
    rebuildKeepingFocus();
  }

  void setBrightnessValue(int value) {
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    if (brightnessPercent == value) return;
    brightnessPercent = static_cast<uint8_t>(value);
    brightnessDirty = true;
    applyPhysicalBacklight(brightnessPercent);
    rebuildKeepingFocus();
  }

  // ====================== 4. TEMPORIZADORES =================================
  void updateTimers(uint32_t now) {
    if (audioMuted) {
      uint32_t elapsed = now - muteStarted; // resta unsigned: tolera rollover
      if (elapsed >= AUDIO_MUTE_MS) {
        audioMuted = false;
        muteSeconds = 0;
        showZero = true;
        zeroStarted = now;
        audioIconDirty = true;
        audioCounterDirty = true;
        updateAudioOutput();
      } else {
        uint16_t remaining = (AUDIO_MUTE_MS - elapsed + 999UL) / 1000UL;
        if (remaining != muteSeconds) {
          muteSeconds = remaining;
          audioCounterDirty = true;  // SOLO el rectangulo del contador
        }
      }
    }
    // 00:00 permanece un segundo, pero el audio ya se reanudo al cumplir 30 s.
    if (showZero && (uint32_t)(now - zeroStarted) >= ZERO_DISPLAY_MS) {
      showZero = false;
      audioCounterDirty = true;
    }
  }

  void updateSimulation(uint32_t now) {
    if (sequenceRunning) updateSequentialDemo(now);
    if (!ADVANCE_DEMO_CLOCKS || sequenceRunning) return;
    uint32_t elapsed = now - lastSimulationTick;
    uint32_t seconds = elapsed / 1000UL;
    if (!seconds) return;
    lastSimulationTick += seconds * 1000UL;
    if (state.procedureActive && state.occlusionSeconds != TIME_UNAVAILABLE)
      state.occlusionSeconds += seconds;
    if (state.ongoingReadjustmentSeconds != TIME_UNAVAILABLE)
      state.ongoingReadjustmentSeconds += seconds;
  }

  void startSequentialDemo() {
    if (!ENABLE_DEMO_CONTROLS) return;
    sequenceRunning = true;
    sequenceStep = 0;
    sequenceStarted = millis();
    record3Ready = false;
    selectedDemo = DEMO_SCENARIO_COUNT; // item extra del catalogo
    cancelMute();
    applySequenceStep(0);
    Serial.println("DEMO SECUENCIAL: inicio TNG-0013");
  }

  void updateSequentialDemo(uint32_t now) {
    uint32_t elapsed = now - sequenceStarted;
    uint8_t target = sequenceStep;
    while (target + 1 < SEQUENCE_STEP_COUNT && elapsed >= SEQUENCE_AT_MS[target + 1]) ++target;
    if (target != sequenceStep) {
      sequenceStep = target;
      applySequenceStep(sequenceStep);
    }
  }

  void applySequenceStep(uint8_t step) {
    DeviceState d;
    snprintf(d.recordId, sizeof(d.recordId), "TNG-0013");
    d.savedRecords = record3Ready ? 3 : 2;
    d.demoSdAvailable = true;
    d.batteryPercent = 82;
    d.pressureSensor = SensorStatus::OK;
    d.selfTest = SelfTestStatus::Superada;
    d.activeConditions = 0;
    d.pressureAtOcclusion = -1;
    d.maximumPressure = 0;
    d.occlusionSeconds = TIME_UNAVAILABLE;
    d.timeToOcclusionSeconds = TIME_UNAVAILABLE;
    d.readjustments = 0;
    d.lastReadjustmentSeconds = TIME_UNAVAILABLE;
    d.ongoingReadjustmentSeconds = TIME_UNAVAILABLE;
    d.procedureActive = false;
    d.recordingActive = false;

    switch (step) {
      case 0: // comprobacion
        d.operation=Operation::Comprobando; d.pressure=-1;
        d.pressureSensor=SensorStatus::Comprobando; d.selfTest=SelfTestStatus::EnCurso;
        break;
      case 1: // preparado
        d.operation=Operation::Preparado; d.pressure=0;
        break;
      case 2: // inflando
        d.operation=Operation::Inflando; d.pressure=84; d.maximumPressure=84;
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 3: // buscando oclusion
        d.operation=Operation::Buscando; d.pressure=186; d.maximumPressure=186;
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 4: // primera oclusion
        d.operation=Operation::OclusionAlcanzada; d.pressure=232; d.maximumPressure=232;
        d.pressureAtOcclusion=232; d.occlusionSeconds=0; d.timeToOcclusionSeconds=24;
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 5: // reajuste sin alarma
        d.operation=Operation::Reajustando; d.pressure=224; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=180; d.timeToOcclusionSeconds=24;
        d.ongoingReadjustmentSeconds=3; d.procedureActive=true; d.recordingActive=true;
        break;
      case 6: // recuperada
        d.operation=Operation::OclusionAlcanzada; d.pressure=238; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=186; d.timeToOcclusionSeconds=24;
        d.readjustments=1; d.lastReadjustmentSeconds=6;
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 7: // perdida de oclusion
        d.operation=Operation::Reajustando; d.pressure=204; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=430; d.timeToOcclusionSeconds=24;
        d.readjustments=1; d.lastReadjustmentSeconds=6; d.ongoingReadjustmentSeconds=1;
        d.activeConditions=conditionBit(Condition::PerdidaOclusion);
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 8: // segundo reajuste
        d.operation=Operation::Reajustando; d.pressure=220; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=432; d.timeToOcclusionSeconds=24;
        d.readjustments=1; d.lastReadjustmentSeconds=6; d.ongoingReadjustmentSeconds=3;
        d.activeConditions=conditionBit(Condition::PerdidaOclusion);
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 9: // oclusion recuperada
        d.operation=Operation::OclusionAlcanzada; d.pressure=238; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=435; d.timeToOcclusionSeconds=24;
        d.readjustments=2; d.lastReadjustmentSeconds=5;
        d.procedureActive=true; d.recordingActive=true;
        break;
      case 10: // bateria baja
        d.operation=Operation::OclusionAlcanzada; d.pressure=238; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=491; d.timeToOcclusionSeconds=24;
        d.readjustments=2; d.lastReadjustmentSeconds=5; d.batteryPercent=12;
        d.activeConditions=conditionBit(Condition::BateriaBaja);
        d.procedureActive=true; d.recordingActive=true;
        break;
      default: // cierre del procedimiento
        d.operation=Operation::Terminado; d.pressure=-1; d.maximumPressure=242;
        d.pressureAtOcclusion=232; d.occlusionSeconds=496; d.timeToOcclusionSeconds=24;
        d.readjustments=2; d.lastReadjustmentSeconds=5; d.batteryPercent=12;
        record3Ready=true; d.savedRecords=3;
        sequenceRunning=false;
        break;
    }
    setDeviceState(d);
    if (!sequenceRunning && step == SEQUENCE_STEP_COUNT-1 && currentScreen==Screen::Principal)
      enterScreen(Screen::Registros);
  }

  uint8_t availableRecordCount() const { return record3Ready ? 3 : 2; }

  const DemoRecord *findRecord(const char *id) const {
    for (uint8_t i=0; i<availableRecordCount(); ++i)
      if (strcmp(DEMO_RECORDS[i].id,id)==0) return &DEMO_RECORDS[i];
    return nullptr;
  }

  void sendRecordList() {
    Serial.println("###TANGO_LIST_BEGIN###");
    Serial.print("{\"schema_version\":\"1.0\",\"records\":[");
    uint8_t n=availableRecordCount();
    for(uint8_t i=0;i<n;++i) {
      if(i) Serial.print(',');
      Serial.print("{\"record_id\":\""); Serial.print(DEMO_RECORDS[i].id);
      Serial.print("\",\"label\":\""); Serial.print(DEMO_RECORDS[i].label); Serial.print("\"}");
    }
    Serial.println("]}");
    Serial.println("###TANGO_LIST_END###");
  }

  const char *severityName(Severity s) const { return s==Severity::Alarma ? "alarma" : "aviso"; }

  void sendRecord(const DemoRecord &r) {
    Serial.println("###TANGO_RECORD_BEGIN###");
    Serial.print("{\"schema_version\":\"1.0\",\"demo\":true,\"record_id\":\"");
    Serial.print(r.id); Serial.print("\",\"sesion\":{\"duracion_s\":"); Serial.print(r.sessionDurationSeconds);
    Serial.print("},\"metricas\":{\"tiempo_oclusion_total_s\":"); Serial.print(r.occlusionTotalSeconds);
    Serial.print(",\"presion_alcanzar_oclusion_mmHg\":"); Serial.print(r.pressureAtOcclusion);
    Serial.print(",\"presion_maxima_mmHg\":"); Serial.print(r.maximumPressure);
    Serial.print(",\"tiempo_hasta_oclusion_inicial_s\":"); Serial.print(r.timeToOcclusionSeconds);
    Serial.print(",\"reajustes_automaticos\":"); Serial.print(r.readjustmentCount);
    Serial.print("},\"reajustes\":[");
    for(uint8_t i=0;i<r.readjustmentCount;++i) {
      if(i) Serial.print(',');
      const ReadjustmentEvent &e=r.readjustments[i];
      Serial.print("{\"t_s\":"); Serial.print(e.tSeconds);
      Serial.print(",\"duracion_s\":"); Serial.print(e.durationSeconds);
      Serial.print(",\"de_mmHg\":"); Serial.print(e.fromPressure);
      Serial.print(",\"a_mmHg\":"); Serial.print(e.toPressure); Serial.print('}');
    }
    Serial.print("],\"alertas\":[");
    for(uint8_t i=0;i<r.alertCount;++i) {
      if(i) Serial.print(',');
      const AlertEvent &e=r.alerts[i];
      uint8_t ci=static_cast<uint8_t>(e.condition);
      Serial.print("{\"t_s\":"); Serial.print(e.tSeconds);
      Serial.print(",\"tipo\":\""); Serial.print(severityName(CONDITIONS[ci].severity));
      Serial.print("\",\"codigo\":\""); Serial.print(CONDITIONS[ci].name);
      Serial.print("\",\"texto\":\""); Serial.print(CONDITIONS[ci].name); Serial.print("\"}");
    }
    Serial.println("]}");
    Serial.println("###TANGO_RECORD_END###");
  }

  void sendProtocolError(const char *message) {
    Serial.print("###TANGO_ERROR###{\"message\":\""); Serial.print(message); Serial.println("\"}");
  }

  void handleSerialCommand() {
    serialCommand[serialCommandLen]='\0';
    if(strcmp(serialCommand,"LIST")==0) { sendRecordList(); return; }
    if(strncmp(serialCommand,"GET:",4)==0) {
      const DemoRecord *record=findRecord(serialCommand+4);
      if(record) sendRecord(*record); else sendProtocolError("Registro no disponible");
      return;
    }
    sendProtocolError("Comando no reconocido");
  }

  // ====================== 5. ESCENARIOS DE PRUEBA =============================
  void loadDemoScenario(uint8_t index) {
    if (!ENABLE_DEMO_CONTROLS || index >= DEMO_SCENARIO_COUNT) return;
    sequenceRunning=false;
    const DemoScenario &d=DEMO_SCENARIOS[index];
    DeviceState demo;
    demo.operation=d.operation; demo.pressure=d.pressure;
    demo.pressureAtOcclusion=d.firstPressure; demo.maximumPressure=d.maximumPressure;
    demo.occlusionSeconds=d.occlusionSeconds;
    demo.timeToOcclusionSeconds=d.timeToOcclusionSeconds;
    demo.readjustments=d.readjustments;
    demo.lastReadjustmentSeconds=d.lastReadjustmentSeconds;
    demo.ongoingReadjustmentSeconds=d.ongoingReadjustmentSeconds;
    demo.batteryPercent=d.battery; demo.activeConditions=d.conditions;
    demo.pressureSensor=d.sensor; demo.selfTest=d.selfTest;
    demo.procedureActive=d.recording; demo.recordingActive=d.recording;
    demo.savedRecords=d.savedRecords;
    if(d.operation==Operation::Terminado) { record3Ready=true; demo.savedRecords=3; }
    // Los 21 instantes del PPT comparten el ID de demostracion TNG-0013.
    selectedDemo=index;
    cancelMute(); // cargar OTRO instante reinicia la demo; navegar NO lo hace.
    setDeviceState(demo);
    if(d.initiallyMuted) startMute();
    lastSimulationTick=millis();
    Serial.print("ESCENARIO: "); Serial.println(d.label);
  }
  void loadScenario(char key) {
    if (!ENABLE_DEMO_CONTROLS) return;
    switch(key) {
      case '0': loadDemoScenario(4); break;
      case '1': loadDemoScenario(7); break;
      case '2': loadDemoScenario(17); break;
      case '9': loadDemoScenario(20); break;
      case 'S': startSequentialDemo(); break;
      default: break;
    }
  }

private:
  enum Control : int8_t {
    C_AUDIO = 0, C_LOCK, C_ALERTS, C_MENU,
    C_TILE0, C_TILE1, C_TILE2, C_TILE3,
    C_PREV, C_NEXT, C_MINUS, C_PLUS, C_INFO, C_TRANSFER, C_BACK,
    C_DEMOS, C_CASE0, C_CASE1, C_CASE2, CONTROL_COUNT
  };
  struct FocusNode {
    bool visible = false, enabled = false;
    int8_t neighbor[4] = {-1,-1,-1,-1};
  };
  FocusNode nodes[CONTROL_COUNT];
  Palette c;
  Screen currentScreen = Screen::Principal;
  int8_t focus = C_MENU;
  uint8_t detailsPage = 0, alertsPage = 0, demoPage = 1, selectedDemo = 4;
  uint32_t controlsDirty = 0;
  bool needsScreenEntry = true, contentDirty = false, brightnessDirty = false;
  bool audioIconDirty = false, audioCounterDirty = false;
  bool patchReady = false, drawingPatch = false;
  Rect activePatch = {0,0,0,0};
  DeviceState painted;
  bool paintedMuted = false, paintedZero = false;
  uint16_t muteSeconds = 0, lastAudioConditions = 0;
  uint32_t muteStarted = 0, zeroStarted = 0, lastSimulationTick = 0;
  bool showZero = false;
  bool sequenceRunning = false, record3Ready = false;
  uint8_t sequenceStep = 0;
  uint32_t sequenceStarted = 0;
  bool serialCommandMode = false;
  char serialCommand[48] = {0};
  uint8_t serialCommandLen = 0;

  static uint16_t conditionBit(Condition condition) {
    return 1U << static_cast<uint8_t>(condition);
  }
  uint8_t activeCount() const {
    uint8_t n = 0;
    for (uint8_t i=0; i<static_cast<uint8_t>(Condition::Count); ++i)
      if (state.activeConditions & (1U << i)) ++n;
    return n;
  }
  int8_t sortedCondition(uint8_t rank) const {
    int16_t previous = -1;
    int8_t result = -1;
    for (uint8_t position=0; position<=rank; ++position) {
      int16_t candidatePriority = 1000;
      result = -1;
      for (uint8_t i=0; i<static_cast<uint8_t>(Condition::Count); ++i) {
        if ((state.activeConditions & (1U << i)) &&
            CONDITIONS[i].priority > previous &&
            CONDITIONS[i].priority < candidatePriority) {
          candidatePriority = CONDITIONS[i].priority;
          result = i;
        }
      }
      if (result < 0) return -1;
      previous = candidatePriority;
    }
    return result;
  }
  bool hasAudibleCondition() const {
    for (uint8_t i=0; i<static_cast<uint8_t>(Condition::Count); ++i)
      if ((state.activeConditions & (1U << i)) && CONDITIONS[i].audible) return true;
    return false;
  }
  bool canTransfer() const {
    return !state.procedureActive && !state.recordingActive &&
           state.demoSdAvailable && state.savedRecords > 0;
  }
  uint8_t demoItemCount() const { return DEMO_SCENARIO_COUNT + 1; }
  uint8_t demoPages() const { return (demoItemCount()+2)/3; }
  uint8_t demoRows() const {
    uint8_t left=demoItemCount()-demoPage*3;
    return left<3 ? left : 3;
  }
  const char *sensorName() const {
    switch(state.pressureSensor) {
      case SensorStatus::OK: return "OK";
      case SensorStatus::NoDisponible: return "No disponible";
      case SensorStatus::Comprobando: return "Comprobando";
      case SensorStatus::SinVerificar: return "Sin verificar";
    }
    return "Sin verificar";
  }
  uint8_t alertPages() const { return activeCount() ? (activeCount()+1)/2 : 1; }
  const char *operationName() const {
    switch (state.operation) {
      case Operation::Preparado: return "PREPARADO";
      case Operation::Inflando: return "INFLANDO TORNIQUETE";
      case Operation::Buscando: return "BUSCANDO OCLUSION";
      case Operation::OclusionAlcanzada: return "OCLUSION ALCANZADA";
      case Operation::Reajustando: return "REAJUSTANDO";
      case Operation::ControlEnPausa: return "CONTROL EN PAUSA";
      case Operation::Comprobando: return "COMPROBANDO SISTEMA";
      case Operation::Terminado: return "SIN PROCEDIMIENTO ACTIVO";
      case Operation::InicioBloqueado: return "INICIO BLOQUEADO";
    }
    return "ESTADO NO DISPONIBLE";
  }
  const char *screenTitle() const {
    switch (currentScreen) {
      case Screen::Principal: return "PRINCIPAL";
      case Screen::Menu: return "MENU";
      case Screen::Detalles: return "DETALLES";
      case Screen::Alertas: return "ALERTAS ACTIVAS";
      case Screen::Sistema: return "SISTEMA";
      case Screen::Ajustes: return "AJUSTES";
      case Screen::Informacion: return "INFORMACION";
      case Screen::Registros: return "REGISTROS";
      case Screen::Transferir: return "TRANSFERIR A PC";
      case Screen::Comprobacion: return "COMPROBACION";
      case Screen::Escenarios: return "ESCENARIOS DEMO";
    }
    return "TANGO";
  }

  void readNavigation() {
    // U/D/L/R/E siguen siendo bytes unicos para el mando. Los comandos de la
    // interfaz web comienzan con @ y terminan en CR/LF: @LIST, @GET:TNG-0012.
    for (uint8_t i=0; i<64 && Serial.available(); ++i) {
      char key = static_cast<char>(Serial.read());
      if(serialCommandMode) {
        if(key=='\n' || key=='\r') {
          if(serialCommandLen) handleSerialCommand();
          serialCommandMode=false; serialCommandLen=0;
        } else if(serialCommandLen < sizeof(serialCommand)-1) {
          serialCommand[serialCommandLen++]=key;
        } else {
          serialCommandMode=false; serialCommandLen=0;
          sendProtocolError("Comando demasiado largo");
        }
        continue;
      }
      if(key=='@') { serialCommandMode=true; serialCommandLen=0; continue; }
      switch (key) {
        case 'U': dispatchNavigation(NavEvent::Up); break;
        case 'D': dispatchNavigation(NavEvent::Down); break;
        case 'L': dispatchNavigation(NavEvent::Left); break;
        case 'R': dispatchNavigation(NavEvent::Right); break;
        case 'E': dispatchNavigation(NavEvent::Enter); break;
        case '0': case '1': case '2': case '9': case 'S': loadScenario(key); break;
        default: break;
      }
      refreshDirtyWidgets();
    }
  }
  void markControl(int8_t id) { if (id>=0) controlsDirty |= 1UL << id; }
  void setFocus(int8_t id) {
    if (id < 0 || id == focus || !nodes[id].visible || !nodes[id].enabled) return;
    markControl(focus);
    focus = id;
    markControl(focus);
  }
  void node(int8_t id, bool enabled, int8_t up, int8_t down, int8_t left, int8_t right) {
    nodes[id].visible = true;
    nodes[id].enabled = enabled;
    nodes[id].neighbor[0]=up; nodes[id].neighbor[1]=down;
    nodes[id].neighbor[2]=left; nodes[id].neighbor[3]=right;
  }
  void buildNavigation() {
    for (auto &n : nodes) n = FocusNode();
    node(C_AUDIO, hasAudibleCondition(), -1,-1,-1,C_LOCK);
    node(C_LOCK, true, -1,-1,C_AUDIO,C_ALERTS);
    node(C_ALERTS, activeCount()>0, -1,-1,C_LOCK,C_MENU);
    node(C_MENU, true, -1,-1,C_ALERTS,-1);
    switch (currentScreen) {
      case Screen::Menu:
        node(C_TILE0,true,-1,C_TILE2,-1,C_TILE1);
        node(C_TILE1,true,-1,C_TILE3,C_TILE0,-1);
        node(C_TILE2,true,C_TILE0,C_LOCK,-1,C_TILE3);
        node(C_TILE3,true,C_TILE1,C_MENU,C_TILE2,-1);
        footerUp(C_TILE2,C_TILE2,C_TILE2,C_TILE3);
        break;
      case Screen::Sistema:
        node(C_TILE0,true,-1,ENABLE_DEMO_CONTROLS?C_DEMOS:C_LOCK,-1,C_TILE1);
        node(C_TILE1,true,-1,ENABLE_DEMO_CONTROLS?C_DEMOS:C_MENU,C_TILE0,-1);
        if(ENABLE_DEMO_CONTROLS) {
          node(C_DEMOS,true,C_TILE0,C_MENU,-1,-1);
          footerUp(C_DEMOS,C_DEMOS,C_DEMOS,C_DEMOS);
        } else footerUp(C_TILE0,C_TILE0,C_TILE0,C_TILE1);
        break;
      case Screen::Escenarios:
        node(C_PREV,demoPage>0,-1,C_CASE0,-1,C_NEXT);
        node(C_NEXT,demoPage+1<demoPages(),-1,C_CASE0,C_PREV,-1);
        for(uint8_t i=0;i<demoRows();++i)
          node(C_CASE0+i,true,i?C_CASE0+i-1:(demoPage?C_PREV:C_NEXT),
               i+1<demoRows()?C_CASE0+i+1:C_MENU,-1,-1);
        footerUp(C_CASE0+demoRows()-1,C_CASE0+demoRows()-1,
                 C_CASE0+demoRows()-1,C_CASE0+demoRows()-1);
        break;
      case Screen::Detalles:
        // En extremos la flecha no disponible permanece visible y atenuada.
        node(C_PREV,detailsPage>0,-1,C_LOCK,-1,C_NEXT);
        node(C_NEXT,detailsPage<2,-1,C_MENU,C_PREV,-1);
        footerUp(detailsPage>0?C_PREV:C_NEXT,detailsPage>0?C_PREV:C_NEXT,
                 detailsPage>0?C_PREV:C_NEXT,detailsPage<2?C_NEXT:C_PREV);
        break;
      case Screen::Alertas:
        if (alertPages()>1) {
          node(C_PREV,alertsPage>0,-1,C_LOCK,-1,C_NEXT);
          node(C_NEXT,alertsPage+1<alertPages(),-1,C_MENU,C_PREV,-1);
          footerUp(alertsPage>0?C_PREV:C_NEXT,alertsPage>0?C_PREV:C_NEXT,
                   alertsPage>0?C_PREV:C_NEXT,alertsPage+1<alertPages()?C_NEXT:C_PREV);
        }
        break;
      case Screen::Ajustes:
        node(C_MINUS,brightnessPercent>10,-1,C_INFO,-1,C_PLUS);
        node(C_PLUS,brightnessPercent<100,-1,C_INFO,C_MINUS,-1);
        node(C_INFO,true,brightnessPercent>10?C_MINUS:C_PLUS,C_LOCK,-1,-1);
        footerUp(C_INFO,C_INFO,C_INFO,C_INFO);
        break;
      case Screen::Registros:
        node(C_TRANSFER,canTransfer(),-1,C_MENU,-1,-1);
        footerUp(C_TRANSFER,C_TRANSFER,C_TRANSFER,C_TRANSFER);
        break;
      case Screen::Transferir:
        node(C_BACK,true,-1,C_MENU,-1,-1);
        footerUp(C_BACK,C_BACK,C_BACK,C_BACK);
        break;
      default: break;
    }
  }
  void footerUp(int8_t audio, int8_t lock, int8_t alerts, int8_t menu) {
    nodes[C_AUDIO].neighbor[0]=audio; nodes[C_LOCK].neighbor[0]=lock;
    nodes[C_ALERTS].neighbor[0]=alerts; nodes[C_MENU].neighbor[0]=menu;
  }
  void rebuildKeepingFocus() {
    FocusNode previous[CONTROL_COUNT];
    memcpy(previous,nodes,sizeof(nodes));
    if(alertsPage>=alertPages()) alertsPage=alertPages()-1;
    buildNavigation();
    for (int8_t i=0;i<CONTROL_COUNT;++i)
      if (previous[i].enabled!=nodes[i].enabled || previous[i].visible!=nodes[i].visible)
        markControl(i);
    if (screenLocked) { if (focus!=C_LOCK) setFocus(C_LOCK); return; }
    if (!nodes[focus].enabled || !nodes[focus].visible) {
      int8_t preferred=C_MENU;
      if (currentScreen==Screen::Ajustes && (focus==C_MINUS || focus==C_PLUS))
        preferred=(focus==C_MINUS ? C_PLUS : C_MINUS);
      if ((currentScreen==Screen::Alertas || currentScreen==Screen::Detalles || currentScreen==Screen::Escenarios) &&
          (focus==C_PREV || focus==C_NEXT))
        preferred=nodes[C_NEXT].enabled?C_NEXT:nodes[C_PREV].enabled?C_PREV:C_MENU;
      setFocus(preferred);
    }
  }
  void enterScreen(Screen next) {
    if (next == currentScreen) return;
    currentScreen = next;
    if (next==Screen::Alertas) alertsPage=0;
    if (next==Screen::Escenarios) demoPage=selectedDemo/3;
    buildNavigation();
    switch (next) {
      case Screen::Menu: case Screen::Sistema: focus=C_TILE0; break;
      case Screen::Escenarios: focus=C_CASE0+(selectedDemo%3); break;
      case Screen::Detalles: focus=(detailsPage<2 ? C_NEXT : C_PREV); break;
      case Screen::Ajustes: focus=nodes[C_MINUS].enabled ? C_MINUS : C_PLUS; break;
      case Screen::Registros: focus=canTransfer() ? C_TRANSFER : C_MENU; break;
      case Screen::Transferir: focus=C_BACK; break;
      case Screen::Alertas: focus=alertPages()>1 ? C_NEXT : C_MENU; break;
      default: focus=C_MENU; break;
    }
    needsScreenEntry = true;
  }
  void changePage(int8_t direction) {
    uint8_t *page = currentScreen==Screen::Detalles ? &detailsPage :
                    currentScreen==Screen::Escenarios ? &demoPage : &alertsPage;
    uint8_t maxPages = currentScreen==Screen::Detalles ? 3 :
                       currentScreen==Screen::Escenarios ? demoPages() : alertPages();
    int8_t previousFocus=focus;
    int16_t candidate=*page+direction;
    if (candidate<0 || candidate>=maxPages) return;
    *page=static_cast<uint8_t>(candidate);
    contentDirty=true; // SOLO contenido de Detalles/Alertas, no cabecera/footer
    // Mantener foco en la flecha que permite seguir o regresar, sin callejon.
    focus=(*page==maxPages-1 ? C_PREV : C_NEXT);
    if(currentScreen==Screen::Escenarios && previousFocus>=C_CASE0 && previousFocus<=C_CASE2)
      focus=C_CASE0+((previousFocus-C_CASE0)<demoRows()?previousFocus-C_CASE0:demoRows()-1);
    buildNavigation();
  }
  void activateFocused() {
    if (!nodes[focus].enabled) return;
    switch (focus) {
      case C_AUDIO: startMute(); return;
      case C_LOCK: screenLocked=true; markControl(C_LOCK); return;
      case C_ALERTS: enterScreen(Screen::Alertas); return;
      case C_MENU: enterScreen(Screen::Menu); return;
      case C_PREV: changePage(-1); return;
      case C_NEXT: changePage(1); return;
      case C_MINUS: setBrightnessValue(brightnessPercent-10); return;
      case C_PLUS: setBrightnessValue(brightnessPercent+10); return;
      case C_INFO: enterScreen(Screen::Informacion); return;
      case C_DEMOS: enterScreen(Screen::Escenarios); return;
      case C_CASE0: case C_CASE1: case C_CASE2: {
        uint8_t index=demoPage*3+focus-C_CASE0;
        if(index==DEMO_SCENARIO_COUNT) { startSequentialDemo(); enterScreen(Screen::Principal); return; }
        loadDemoScenario(index);
        Screen destination=(state.operation==Operation::Comprobando || state.operation==Operation::InicioBloqueado)
          ? Screen::Comprobacion : state.operation==Operation::Terminado ? Screen::Registros : Screen::Principal;
        enterScreen(destination); return;
      }
      case C_TRANSFER: if(canTransfer()) enterScreen(Screen::Transferir); return;
      case C_BACK: enterScreen(Screen::Registros); return;
      default: break;
    }
    if (currentScreen==Screen::Menu) {
      const Screen destination[]={Screen::Principal,Screen::Detalles,Screen::Alertas,Screen::Sistema};
      if(focus>=C_TILE0 && focus<=C_TILE3) enterScreen(destination[focus-C_TILE0]);
    } else if (currentScreen==Screen::Sistema) {
      if(focus==C_TILE0) enterScreen(Screen::Ajustes);
      else if(focus==C_TILE1) enterScreen(Screen::Registros);
    }
  }
  void startMute() {
    if (!hasAudibleCondition() || audioMuted) return;
    audioMuted=true; showZero=false; muteStarted=millis();
    muteSeconds=(AUDIO_MUTE_MS+999UL)/1000UL;
    audioIconDirty=audioCounterDirty=true;
    updateAudioOutput();
  }
  void cancelMute() {
    if(audioMuted || showZero) {
      audioMuted=false; showZero=false; muteSeconds=0;
      audioIconDirty=audioCounterDirty=true;
    }
  }
  void updateAudioOutput() {
    bool requested=hasAudibleCondition() && !audioMuted;
    if(requested!=audioRequested) {
      audioRequested=requested;
      applyAudioOutput(requested);
    }
  }
  void applyAudioOutput(bool enabled) {
    // HOOK: futuro generador de sonido NO bloqueante. Ningun GPIO asumido.
    // Hoy la solicitud se observa por Serial; no se afirma sonido fisico.
    Serial.println(enabled ? "AUDIO solicitado" : "AUDIO detenido");
  }
  void applyPhysicalBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    // PWM 8-bit: 0 = apagado, 255 = brillo maximo.
    // La HMI limita el ajuste visible a 10..100 %.
    const uint8_t duty = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255U) / 100U);
    analogWrite(TFT_LED_PIN, duty);
  }

  // ====================== 6. WIDGETS / PARCHE PARCIAL =========================
  void initPalette() {
    c.bg=tft.color565(244,247,249); c.white=TFT_WHITE;
    c.navy=tft.color565(31,54,78); c.text=tft.color565(24,34,44);
    c.muted=tft.color565(88,100,112); c.border=tft.color565(200,209,217);
    c.green=tft.color565(36,145,92); c.greenLight=tft.color565(203,236,219);
    c.amber=tft.color565(180,105,0); c.amberBg=tft.color565(255,194,51);
    c.red=tft.color565(155,28,28); c.redBg=tft.color565(211,56,56);
    c.orange=tft.color565(232,135,36);
    c.focus=tft.color565(222,232,241); c.disabled=tft.color565(228,232,235);
    c.disabledText=tft.color565(140,148,155);
  }
  TFT_eSPI &beginPatch(Rect r, uint16_t background) {
    activePatch=r;
    drawingPatch=patchReady && r.w<=444 && r.h<=56;
    TFT_eSPI &g=drawingPatch ? static_cast<TFT_eSPI&>(patch) : tft;
    if(drawingPatch) { patch.resetViewport(); patch.setViewport(0,0,r.w,r.h); }
    else tft.setViewport(r.x,r.y,r.w,r.h,true);
    g.fillRect(0,0,r.w,r.h,background);
    return g;
  }
  void endPatch() {
    if(drawingPatch) {
      patch.resetViewport();
      patch.pushSprite(activePatch.x,activePatch.y,0,0,activePatch.w,activePatch.h);
    } else tft.resetViewport();
  }
  const GFXfont *fitFont(TFT_eSPI &g,const char *text,int16_t width,const GFXfont *preferred) {
    const GFXfont *choices[]={FSSB24,FSSB18,FSSB12,FSSB9};
    bool use=false;
    for(const GFXfont *font:choices) {
      if(font==preferred) use=true;
      if(!use) continue;
      g.setFreeFont(font);
      if(g.textWidth(text)<=width) return font;
    }
    return FSSB9; // Cadenas/valores se verifican; no truncar palabras.
  }
  void text(TFT_eSPI &g,const char *value,int16_t x,int16_t y,int16_t width,
            const GFXfont *font,uint16_t fg,uint16_t bg,uint8_t datum=MC_DATUM) {
    g.setTextSize(1);
    const GFXfont *chosen=fitFont(g,value,width,font);
    g.setFreeFont(chosen);
    // FreeFont centra su ascendente, no el contorno real de la cadena.
    // Compensar evita cortar la 'g' de mmHg o Disponible en filas compactas.
    int16_t ascent=0, top=127, bottom=-127;
    for(uint16_t i=0;i<=chosen->last-chosen->first;++i)
      if(-chosen->glyph[i].yOffset>ascent) ascent=-chosen->glyph[i].yOffset;
    for(const char *p=value;*p;++p) {
      uint8_t ch=static_cast<uint8_t>(*p);
      if(ch<chosen->first || ch>chosen->last) continue;
      const GFXglyph &glyph=chosen->glyph[ch-chosen->first];
      if(!glyph.height) continue;
      if(glyph.yOffset<top) top=glyph.yOffset;
      int16_t end=glyph.yOffset+glyph.height-1;
      if(end>bottom) bottom=end;
    }
    if(top<=bottom && (datum==MC_DATUM || datum==ML_DATUM || datum==MR_DATUM))
      y-=ascent-ascent/2+(top+bottom)/2;
    g.setTextDatum(datum);
    g.setTextColor(fg,bg);
    g.drawString(value,x,y,1);
    g.setTextDatum(TL_DATUM);
  }
  void outline(TFT_eSPI &g,Rect r,uint16_t fill,uint16_t border,bool selected=false) {
    g.fillRoundRect(0,0,r.w,r.h,6,fill);
    g.drawRoundRect(0,0,r.w,r.h,6,border);
    if(selected) g.drawRoundRect(1,1,r.w-2,r.h-2,5,border);
  }
  void formatTime(uint32_t seconds,char *out,size_t size) const {
    if(seconds==TIME_UNAVAILABLE) { snprintf(out,size,"--:--:--"); return; }
    if(seconds>359999UL) { snprintf(out,size,">99 h"); return; }
    snprintf(out,size,"%02lu:%02lu:%02lu",(unsigned long)(seconds/3600UL),
             (unsigned long)((seconds/60UL)%60UL),(unsigned long)(seconds%60UL));
  }
  void formatPressure(int16_t pressure,char *out,size_t size,bool units=true) const {
    if(pressure<0) snprintf(out,size,units ? "--- mmHg" : "---");
    else snprintf(out,size,units ? "%d mmHg" : "%d",pressure);
  }
  void drawHeader() {
    tft.fillRect(0,0,480,42,c.white);
    tft.drawFastHLine(0,41,480,c.border);
    text(tft,screenTitle(),18,21,307,FSSB12,c.navy,c.white,ML_DATUM);
    updateBattery();
  }
  void updateBattery() {
    Rect r={334,4,128,34};
    TFT_eSPI &g=beginPatch(r,c.white);
    char b[8]; snprintf(b,sizeof(b),"%u%%",state.batteryPercent);
    text(g,b,70,17,70,FSSB12,c.navy,c.white,MR_DATUM);
    uint16_t ink=c.green;
    if(state.activeConditions & conditionBit(Condition::BateriaCritica)) ink=c.red;
    else if(state.activeConditions & conditionBit(Condition::BateriaBaja)) ink=c.amber;
    g.drawRoundRect(80,6,42,22,3,c.navy); g.fillRect(122,12,4,10,c.navy);
    int16_t fill=(36*state.batteryPercent)/100;
    if(fill>0) g.fillRect(83,9,fill,16,ink);
    endPatch();
  }
  void updateState() {
    Rect r={18,49,444,43};
    uint16_t background=c.greenLight, border=c.green;
    // La operacion NO cambia de color por una alarma independiente.
    if(state.operation==Operation::Reajustando || state.operation==Operation::Inflando ||
       state.operation==Operation::Buscando || state.operation==Operation::Comprobando) {
      background=c.focus; border=c.navy;
    } else if(state.operation==Operation::ControlEnPausa || state.operation==Operation::Terminado ||
              state.operation==Operation::Preparado || state.operation==Operation::InicioBloqueado) { background=c.disabled; border=c.muted; }
    TFT_eSPI &g=beginPatch(r,c.bg);
    outline(g,r,background,border);
    g.fillRoundRect(0,0,6,r.h,3,border);
    text(g,operationName(),r.w/2,22,r.w-30,FSSB12,c.navy,background);
    endPatch();
  }
  Rect footerRect(int8_t id) const {
    switch(id) {
      case C_AUDIO: return {18,274,92,40};
      case C_LOCK: return {118,274,46,40};
      case C_ALERTS: return {172,274,74,40};
      default: return {358,274,104,40};
    }
  }
  uint16_t controlFill(int8_t id) const {
    if(!nodes[id].enabled) return c.disabled;
    return id==focus ? c.focus : c.white;
  }
  uint16_t controlInk(int8_t id) const { return nodes[id].enabled ? c.navy : c.disabledText; }
  void speaker(TFT_eSPI &g,int16_t x,int16_t y) {
    uint16_t bg=hasAudibleCondition() ? (audioMuted?c.orange:c.green) : c.disabled;
    uint16_t ink=hasAudibleCondition() ? c.white : c.disabledText;
    g.fillRoundRect(x,y,30,28,5,bg);
    g.drawBitmap(x+3,y+2,audioMuted?MUTED_BITMAP:SOUND_BITMAP,24,24,ink);
  }
  void padlock(TFT_eSPI &g) {
    uint16_t bg=screenLocked ? c.green : c.orange;
    g.fillRoundRect(6,4,34,32,5,bg);
    g.drawBitmap(11,8,screenLocked?LOCKED_BITMAP:UNLOCKED_BITMAP,24,24,c.white);
    // Bocallave dibujada como geometria simple, como en el sketch original.
    int16_t keyX=screenLocked?23:20;
    g.fillCircle(keyX,24,2,bg); g.fillRect(keyX-1,24,3,5,bg);
  }
  void drawFooterItem(int8_t id) {
    Rect r=footerRect(id);
    TFT_eSPI &g=beginPatch(r,c.white);
    // El acceso de alertas conserva su hueco aun con contador cero.
    if(id==C_ALERTS && !activeCount()) { endPatch(); return; }
    uint16_t bg=controlFill(id),ink=controlInk(id);
    outline(g,r,bg,id==focus ? c.navy : c.border,id==focus);
    if(id==C_AUDIO) {
      speaker(g,4,6);
      audioCounter(g,36,20,50,bg,ink);
    } else if(id==C_LOCK) padlock(g);
    else if(id==C_ALERTS) {
      int8_t condition=sortedCondition(0);
      bool alarm=condition>=0 && CONDITIONS[condition].severity==Severity::Alarma;
      uint16_t tile=alarm ? c.redBg : c.amberBg;
      g.fillRoundRect(4,5,32,30,5,tile);
      g.drawBitmap(8,8,ALERT_BITMAP,24,24,alarm?c.white:c.navy);
      char n[5]; snprintf(n,sizeof(n),"%u",activeCount());
      text(g,n,53,21,26,FSSB12,ink,bg);
    } else text(g,"MENU",52,21,84,FSSB12,ink,bg);
    endPatch();
  }
  void audioCounter(TFT_eSPI &g,int16_t x,int16_t y,int16_t width,uint16_t bg,uint16_t ink) {
    char label[8];
    if(audioMuted || showZero) snprintf(label,sizeof(label),"%02u:%02u",muteSeconds/60,muteSeconds%60);
    else snprintf(label,sizeof(label),hasAudibleCondition() ? "ON" : "--");
    text(g,label,x+width/2,y,width,FSSB9,ink,bg);
  }
  void updateAudioCounter() {
    uint16_t bg=controlFill(C_AUDIO);
    TFT_eSPI &g=beginPatch({54,281,51,26},bg);
    audioCounter(g,0,13,51,bg,controlInk(C_AUDIO));
    endPatch();
  }
  void updateAudioIcon() {
    uint16_t bg=controlFill(C_AUDIO);
    TFT_eSPI &g=beginPatch({22,280,30,28},bg);
    speaker(g,0,0);
    endPatch();
  }
  void drawFooter() {
    tft.fillRect(0,267,480,53,c.white);
    tft.drawFastHLine(0,267,480,c.border);
    for(int8_t i=C_AUDIO;i<=C_MENU;++i) drawFooterItem(i);
  }
  void drawAlertBand(Rect r,int8_t condition) {
    TFT_eSPI &g=beginPatch(r,c.bg);
    if(condition>=0) {
      bool alarm=CONDITIONS[condition].severity==Severity::Alarma;
      uint16_t bg=alarm?c.redBg:c.amberBg;
      uint16_t ink=alarm?c.white:c.navy;
      outline(g,r,bg,alarm?c.red:c.amber);
      g.drawBitmap(10,(r.h-24)/2,ALERT_BITMAP,24,24,ink);
      text(g,CONDITIONS[condition].name,44,(r.h+1)/2,r.w-55,FSSB12,ink,bg,ML_DATUM);
    }
    endPatch();
  }
  void drawAlertDetailed(Rect r,int8_t condition) {
    TFT_eSPI &g=beginPatch(r,c.bg);
    if(condition>=0) {
      bool alarm=CONDITIONS[condition].severity==Severity::Alarma;
      uint16_t bg=alarm?c.redBg:c.amberBg;
      uint16_t ink=alarm?c.white:c.navy;
      outline(g,r,bg,alarm?c.red:c.amber);
      g.drawBitmap(9,8,ALERT_BITMAP,24,24,ink);
      text(g,CONDITIONS[condition].name,43,17,r.w-54,FSSB9,ink,bg,ML_DATUM);
      g.setTextSize(1); g.setFreeFont(FSS9); g.setTextDatum(ML_DATUM);
      g.setTextColor(ink,bg);
      g.drawString(CONDITIONS[condition].description,43,r.h-16,1);
      g.setTextDatum(TL_DATUM);
    }
    endPatch();
  }

  void updateAlertArea() {
    if(currentScreen==Screen::Alertas) return;
    drawAlertBand({18,230,444,30},sortedCondition(0));
  }
  void rowLabel(const char *label,Rect row) {
    TFT_eSPI &g=beginPatch(row,c.bg);
    outline(g,row,c.white,c.border);
    text(g,label,12,(row.h+1)/2,265,FSSB9,c.navy,c.white,ML_DATUM);
    endPatch();
  }
  void rowValue(const char *value,Rect row,const GFXfont *font=FSSB12) {
    Rect r={static_cast<int16_t>(row.x+282),static_cast<int16_t>(row.y+3),
            static_cast<int16_t>(row.w-294),static_cast<int16_t>(row.h-6)};
    TFT_eSPI &g=beginPatch(r,c.white);
    text(g,value,r.w-1,r.h/2,r.w-2,font,c.text,c.white,MR_DATUM);
    endPatch();
  }
  void centeredLine(const char *value,Rect r,const GFXfont *font,uint16_t ink) {
    TFT_eSPI &g=beginPatch(r,c.bg);
    text(g,value,r.w/2,r.h/2,r.w-12,font,ink,c.bg);
    endPatch();
  }

  // ====================== 7. PANTALLAS =======================================
  void drawPrincipalStatic() {
    const Rect cards[]={{18,100,217,122},{245,100,217,122}};
    const char *labels[]={"PRESION EJERCIDA","TIEMPO DE OCLUSION"};
    for(uint8_t i=0;i<2;++i) {
      tft.fillRoundRect(cards[i].x,cards[i].y,cards[i].w,cards[i].h,7,c.white);
      tft.drawRoundRect(cards[i].x,cards[i].y,cards[i].w,cards[i].h,7,c.border);
      text(tft,labels[i],cards[i].x+cards[i].w/2,121,201,FSSB9,c.navy,c.white);
    }
    text(tft,"mmHg",185,172,78,FSSB12,c.muted,c.white);
    updatePressure(); updateOcclusionTime();
  }
  void updatePressure() {
    char b[16]; formatPressure(state.pressure,b,sizeof(b),false);
    TFT_eSPI &g=beginPatch({26,143,116,55},c.white);
    text(g,b,58,29,112,FSSB24,c.text,c.white);
    endPatch();
  }
  void updateOcclusionTime() {
    char b[16]; formatTime(state.occlusionSeconds,b,sizeof(b));
    TFT_eSPI &g=beginPatch({251,143,205,55},c.white);
    text(g,b,102,29,199,FSSB24,c.text,c.white);
    endPatch();
  }
  Rect tileRect(int8_t id) const {
    uint8_t n=id-C_TILE0;
    if(currentScreen==Screen::Sistema) return {static_cast<int16_t>(n?245:18),124,217,56};
    return {static_cast<int16_t>((n%2)?245:18),static_cast<int16_t>(100+(n/2)*64),217,56};
  }
  void drawTile(int8_t id) {
    Rect r=tileRect(id); uint16_t bg=controlFill(id);
    const char *menuLabels[]={"PRINCIPAL","DETALLES","ALERTAS","SISTEMA"};
    const char *systemLabels[]={"AJUSTES","REGISTROS"};
    const char *label=currentScreen==Screen::Sistema ? systemLabels[id-C_TILE0] : menuLabels[id-C_TILE0];
    TFT_eSPI &g=beginPatch(r,c.bg);
    outline(g,r,bg,id==focus?c.navy:c.border,id==focus);
    text(g,label,r.w/2,r.h/2,r.w-24,FSSB12,c.navy,bg);
    endPatch();
  }
  void drawArrow(int8_t id) {
    bool details=currentScreen==Screen::Detalles;
    uint8_t page=details?detailsPage:currentScreen==Screen::Escenarios?demoPage:alertsPage;
    uint8_t total=details?3:currentScreen==Screen::Escenarios?demoPages():alertPages();
    Rect r={static_cast<int16_t>(id==C_PREV?18:420),100,42,28};
    TFT_eSPI &g=beginPatch(r,c.bg);
    bool usable=id==C_PREV?page>0:page+1<total;
    uint16_t bg=id==focus?c.focus:c.white;
    outline(g,r,bg,id==focus?c.navy:c.border,id==focus);
    uint16_t ink=usable?c.navy:c.disabledText;
    if(id==C_PREV) g.fillTriangle(12,14,27,6,27,22,ink);
    else g.fillTriangle(29,14,14,6,14,22,ink);
    endPatch();
  }
  Rect detailRow(uint8_t i) const { return {18,static_cast<int16_t>(133+i*31),444,30}; }
  uint8_t detailsRows() const { return detailsPage==1?2:3; }
  const char *detailLabel(uint8_t i) const {
    static const char *labels[3][3]={
      {"Presion actual","Presion al alcanzar oclusion","Presion maxima registrada"},
      {"Tiempo de oclusion","Tiempo hasta oclusion",""},
      {"Reajustes automaticos","Duracion del ultimo reajuste","Reajuste en curso"}
    };
    return labels[detailsPage][i];
  }
  void detailValue(uint8_t i,char *b,size_t size) const {
    if(detailsPage==0) {
      const int16_t values[]={state.pressure,state.pressureAtOcclusion,state.maximumPressure};
      formatPressure(values[i],b,size);
    } else if(detailsPage==1)
      formatTime(i==0?state.occlusionSeconds:state.timeToOcclusionSeconds,b,size);
    else {
      if(i==0) snprintf(b,size,"%u",state.readjustments);
      if(i==1) formatTime(state.lastReadjustmentSeconds,b,size);
      if(i==2) formatTime(state.ongoingReadjustmentSeconds,b,size);
    }
  }
  void updateDetailValue(uint8_t i) {
    if(detailsPage==2 && i==2 && state.ongoingReadjustmentSeconds==TIME_UNAVAILABLE) return;
    char b[24]; detailValue(i,b,sizeof(b)); rowValue(b,detailRow(i));
  }
  void drawDetailsContent() {
    tft.fillRect(18,98,444,126,c.bg);
    const char *titles[]={"PRESIONES","TIEMPOS","REAJUSTES"};
    centeredLine(titles[detailsPage],{68,100,344,28},FSSB9,c.navy);
    drawArrow(C_PREV); drawArrow(C_NEXT);
    for(uint8_t i=0;i<detailsRows();++i) {
      if(detailsPage==2 && i==2 && state.ongoingReadjustmentSeconds==TIME_UNAVAILABLE) continue;
      rowLabel(detailLabel(i),detailRow(i)); updateDetailValue(i);
    }
  }
  void drawAlertsContent() {
    tft.fillRect(18,98,444,166,c.bg);
    uint8_t count=activeCount();
    if(!count) {
      centeredLine("SIN ALERTAS ACTIVAS",{18,130,444,50},FSSB12,c.muted);
      return;
    }
    if(alertsPage>=alertPages()) alertsPage=alertPages()-1;
    uint8_t start=alertsPage*2;
    bool paged=count>2;
    int16_t y=paged?132:105;
    int16_t h=paged?56:66;
    int16_t gap=paged?6:8;
    if(paged) {
      char range[40];
      uint8_t end=start+2<count?start+2:count;
      snprintf(range,sizeof(range),"%u - %u de %u",start+1,end,count);
      centeredLine(range,{68,100,344,28},FSSB9,c.muted);
      drawArrow(C_PREV); drawArrow(C_NEXT);
    }
    for(uint8_t i=0;i<2 && start+i<count;++i)
      drawAlertDetailed({18,static_cast<int16_t>(y+i*(h+gap)),444,h},sortedCondition(start+i));
  }
  void drawBrightnessValue() {
    char b[12]; snprintf(b,sizeof(b),"%u %%",brightnessPercent);
    TFT_eSPI &g=beginPatch({159,134,162,42},c.bg);
    text(g,b,81,21,150,FSSB18,c.navy,c.bg); endPatch();
  }
  void drawSettings() {
    centeredLine("BRILLO",{18,98,444,28},FSSB9,c.navy);
    drawControl(C_MINUS); drawBrightnessValue(); drawControl(C_PLUS); drawControl(C_INFO);
  }
  Rect informationRow(uint8_t i) const { return {18,static_cast<int16_t>(100+i*42),444,37}; }
  void informationValue(uint8_t i) {
    char b[32];
    if(i==0) snprintf(b,sizeof(b),"%s",sensorName());
    else {
      const char *status=(state.activeConditions&conditionBit(Condition::BateriaCritica))?"Critica":
        (state.activeConditions&conditionBit(Condition::BateriaBaja))?"Baja":"Normal";
      snprintf(b,sizeof(b),"%u%% %s",state.batteryPercent,status);
    }
    rowValue(b,informationRow(i),FSSB9);
  }
  void drawInformation() {
    const char *labels[]={"Sensor de presion","Bateria"};
    for(uint8_t i=0;i<2;++i) { rowLabel(labels[i],informationRow(i)); informationValue(i); }
  }
  Rect recordsRow(uint8_t i) const { return {18,static_cast<int16_t>(100+i*27),444,25}; }
  void recordsValue(uint8_t i) {
    char b[32];
    if(i==0) snprintf(b,sizeof(b),state.demoSdAvailable?"Disponible":"No disponible");
    if(i==1) snprintf(b,sizeof(b),"%s",state.recordId);
    if(i==2) snprintf(b,sizeof(b),"%u",state.savedRecords);
    rowValue(b,recordsRow(i),FSSB9);
  }
  void drawRecords() {
    rowLabel("Tarjeta microSD",recordsRow(0));
    rowLabel(state.procedureActive?"Registro actual":state.operation==Operation::Terminado?"Ultimo registro":"Proximo registro",recordsRow(1));
    rowLabel("Registros guardados",recordsRow(2));
    for(uint8_t i=0;i<3;++i) recordsValue(i);
    const char *status=state.recordingActive?"Guardando datos":state.operation==Operation::Terminado?"Registro cerrado":"Sin registro abierto";
    centeredLine(status,{18,181,188,43},FSSB9,c.muted);
    drawControl(C_TRANSFER);
  }
  void drawTransfer() {
    centeredLine(canTransfer()?"Esperando a la computadora":"Transferencia no disponible",
                 {18,98,444,30},FSSB12,c.navy);
    centeredLine("Conecta el cable USB y abri el visor.",{18,128,444,26},FSSB9,c.navy);
    centeredLine("Elegi los registros desde la computadora.",{18,154,444,26},FSSB9,c.navy);
    char b[48];
    if(canTransfer()) snprintf(b,sizeof(b),"%u registros disponibles",state.savedRecords);
    else snprintf(b,sizeof(b),"Finaliza el procedimiento para transferir");
    centeredLine(b,{18,180,264,42},FSSB9,c.muted);
    drawControl(C_BACK);
  }
  Rect controlRect(int8_t id) const {
    switch(id) {
      case C_MINUS: return {91,134,52,42};
      case C_PLUS: return {337,134,52,42};
      case C_INFO: return {18,185,444,37};
      case C_TRANSFER: return {214,186,248,36};
      case C_BACK: return {306,186,156,36};
      case C_DEMOS: return {18,190,444,32};
      default: return {0,0,1,1};
    }
  }
  void drawControl(int8_t id) {
    if(id<=C_MENU) { drawFooterItem(id); return; }
    if(id>=C_CASE0 && id<=C_CASE2) {
      if(nodes[id].visible) drawDemoRow(id); return;
    }
    if(id>=C_TILE0 && id<=C_TILE3) {
      if(nodes[id].visible) drawTile(id);
      return;
    }
    if(id==C_PREV || id==C_NEXT) {
      if(nodes[id].visible) drawArrow(id);
      return;
    }
    if(!nodes[id].visible) return;
    Rect r=controlRect(id);
    TFT_eSPI &g=beginPatch(r,c.bg);
    uint16_t bg=controlFill(id), ink=controlInk(id);
    outline(g,r,bg,id==focus?c.navy:c.border,id==focus);
    if(id==C_MINUS || id==C_PLUS) {
      g.fillRect(r.w/2-9,r.h/2-1,18,3,ink);
      if(id==C_PLUS) g.fillRect(r.w/2-1,r.h/2-9,3,18,ink);
    } else {
      const char *label=id==C_INFO?"INFORMACION DEL EQUIPO":id==C_TRANSFER?"TRANSFERIR A PC":id==C_DEMOS?"ESCENARIOS DEMO":"VOLVER";
      text(g,label,r.w/2,r.h/2,r.w-22,FSSB9,ink,bg);
    }
    endPatch();
  }
  void drawDemoRow(int8_t id) {
    uint8_t row=id-C_CASE0, index=demoPage*3+row;
    if(index>=demoItemCount()) return;
    Rect r={18,static_cast<int16_t>(133+31*row),444,30};
    uint16_t bg=controlFill(id);
    TFT_eSPI &g=beginPatch(r,c.bg);
    outline(g,r,bg,id==focus?c.navy:c.border,id==focus);
    char label[72];
    if(index==DEMO_SCENARIO_COUNT) snprintf(label,sizeof(label),"%02u  SIMULACION DE PROCEDIMIENTO",index+1);
    else snprintf(label,sizeof(label),"%02u  %s",index+1,DEMO_SCENARIOS[index].label);
    text(g,label,12,15,r.w-24,FSSB9,c.navy,bg,ML_DATUM);
    endPatch();
  }
  void drawDemoContent() {
    tft.fillRect(18,98,444,126,c.bg);
    char title[32]; snprintf(title,sizeof(title),"%u / %u",demoPage+1,demoPages());
    centeredLine(title,{68,100,344,28},FSSB9,c.navy);
    drawArrow(C_PREV); drawArrow(C_NEXT);
    for(uint8_t i=0;i<demoRows();++i) drawDemoRow(C_CASE0+i);
  }
  void drawCheck() {
    const char *labels[]={"Autocomprobacion","Sensor de presion","Inicio"};
    const char *status=state.selfTest==SelfTestStatus::EnCurso?"En curso":
                      state.selfTest==SelfTestStatus::Fallida?"Fallida":"Superada";
    const char *start=state.selfTest==SelfTestStatus::EnCurso?"En espera":
                     state.selfTest==SelfTestStatus::Fallida?"Bloqueado":"Disponible";
    const char *values[]={status,sensorName(),start};
    for(uint8_t i=0;i<3;++i) {
      rowLabel(labels[i],informationRow(i)); rowValue(values[i],informationRow(i),FSSB9);
    }
  }
  void drawScreenOnEntry() {
    // UNICA llamada a fillScreen de todo el programa. SOLO entrada de pantalla.
    tft.fillScreen(c.bg);
    drawHeader(); updateState();
    switch(currentScreen) {
      case Screen::Principal: drawPrincipalStatic(); break;
      case Screen::Menu: for(int8_t i=C_TILE0;i<=C_TILE3;++i) drawTile(i); break;
      case Screen::Detalles: drawDetailsContent(); break;
      case Screen::Alertas: drawAlertsContent(); break;
      case Screen::Sistema: drawTile(C_TILE0); drawTile(C_TILE1); drawControl(C_DEMOS); break;
      case Screen::Escenarios: drawDemoContent(); break;
      case Screen::Comprobacion: drawCheck(); break;
      case Screen::Ajustes: drawSettings(); break;
      case Screen::Informacion: drawInformation(); break;
      case Screen::Registros: drawRecords(); break;
      case Screen::Transferir: drawTransfer(); break;
    }
    updateAlertArea(); drawFooter();
  }

  // ====================== 8. INVALIDACION / ACTUALIZACION PARCIAL =============
public:
  void refreshDirtyWidgets() {
    if(needsScreenEntry) {
      drawScreenOnEntry();
      needsScreenEntry=false; contentDirty=false; controlsDirty=0;
      brightnessDirty=false; audioIconDirty=false; audioCounterDirty=false;
      painted=state; paintedMuted=audioMuted; paintedZero=showZero;
      return;
    }
    bool alertsChanged=state.activeConditions!=painted.activeConditions;
    const uint16_t batteryFlags=conditionBit(Condition::BateriaBaja)|conditionBit(Condition::BateriaCritica);
    bool batteryChanged=state.batteryPercent!=painted.batteryPercent ||
                       ((state.activeConditions^painted.activeConditions)&batteryFlags);
    if(batteryChanged) updateBattery();
    if(state.operation!=painted.operation) updateState();
    if(alertsChanged) {
      markControl(C_ALERTS);
      if(currentScreen==Screen::Alertas) contentDirty=true;
      else updateAlertArea();
    }
    switch(currentScreen) {
      case Screen::Principal:
        if(state.pressure!=painted.pressure) updatePressure();
        if(state.occlusionSeconds!=painted.occlusionSeconds) updateOcclusionTime();
        break;
      case Screen::Detalles:
        if(detailsPage==2 && ((state.ongoingReadjustmentSeconds==TIME_UNAVAILABLE)!=
                             (painted.ongoingReadjustmentSeconds==TIME_UNAVAILABLE))) contentDirty=true;
        if(!contentDirty) {
          if(detailsPage==0) {
            if(state.pressure!=painted.pressure) updateDetailValue(0);
            if(state.pressureAtOcclusion!=painted.pressureAtOcclusion) updateDetailValue(1);
            if(state.maximumPressure!=painted.maximumPressure) updateDetailValue(2);
          } else if(detailsPage==1) {
            if(state.occlusionSeconds!=painted.occlusionSeconds) updateDetailValue(0);
            if(state.timeToOcclusionSeconds!=painted.timeToOcclusionSeconds) updateDetailValue(1);
          } else {
            if(state.readjustments!=painted.readjustments) updateDetailValue(0);
            if(state.lastReadjustmentSeconds!=painted.lastReadjustmentSeconds) updateDetailValue(1);
            if(state.ongoingReadjustmentSeconds!=painted.ongoingReadjustmentSeconds) updateDetailValue(2);
          }
        }
        if(contentDirty) drawDetailsContent();
        break;
      case Screen::Alertas:
        if(contentDirty) drawAlertsContent();
        break;
      case Screen::Escenarios:
        if(contentDirty) drawDemoContent();
        break;
      case Screen::Comprobacion:
        if(state.selfTest!=painted.selfTest || state.pressureSensor!=painted.pressureSensor) drawCheck();
        break;
      case Screen::Ajustes:
        if(brightnessDirty) drawBrightnessValue();
        break;
      case Screen::Informacion:
        if(state.pressureSensor!=painted.pressureSensor) informationValue(0);
        if(batteryChanged) informationValue(1);
        break;
      case Screen::Registros:
        if(state.procedureActive!=painted.procedureActive || state.recordingActive!=painted.recordingActive ||
           state.operation!=painted.operation) drawRecords();
        else {
          if(state.demoSdAvailable!=painted.demoSdAvailable) recordsValue(0);
          if(strcmp(state.recordId,painted.recordId)) recordsValue(1);
          if(state.savedRecords!=painted.savedRecords) recordsValue(2);
        }
        break;
      case Screen::Transferir:
        if(contentDirty || state.savedRecords!=painted.savedRecords ||
           state.procedureActive!=painted.procedureActive || state.recordingActive!=painted.recordingActive ||
           state.demoSdAvailable!=painted.demoSdAvailable) drawTransfer();
        break;
      default: break;
    }
    // Foco: solamente dos bits marcados por setFocus().
    // Otros bits pueden responder a cambios reales de disponibilidad/datos.
    uint32_t redraw=controlsDirty;
    for(int8_t i=0;i<CONTROL_COUNT;++i)
      if(redraw & (1UL<<i)) drawControl(i);
    if(!(redraw & (1UL<<C_AUDIO))) {
      if(audioIconDirty || paintedMuted!=audioMuted) updateAudioIcon();
      if(audioCounterDirty || paintedZero!=showZero) updateAudioCounter();
    }
    controlsDirty=0; contentDirty=false; brightnessDirty=false;
    audioIconDirty=false; audioCounterDirty=false;
    painted=state; paintedMuted=audioMuted; paintedZero=showZero;
  }
};

TangoHMI hmi;
void setup() { hmi.begin(); }
void loop() { hmi.tick(); }
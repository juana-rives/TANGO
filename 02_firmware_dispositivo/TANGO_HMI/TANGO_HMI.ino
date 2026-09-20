#include <TFT_eSPI.h>
#include <SPI.h>
#include "Free_Fonts.h"

TFT_eSPI tft = TFT_eSPI();


// =====================================================
// PANTALLAS
// =====================================================

enum Screen {
  SCREEN_PRINCIPAL,
  SCREEN_MENU,
  SCREEN_DETALLES,
  SCREEN_ALERTAS,
  SCREEN_SISTEMA,
  SCREEN_REGISTROS,
  SCREEN_TRANSFERIR
};

Screen currentScreen = SCREEN_PRINCIPAL;


// =====================================================
// EVENTOS DE NAVEGACION
// =====================================================

enum NavEvent {
  NAV_NONE,
  NAV_UP,
  NAV_DOWN,
  NAV_LEFT,
  NAV_RIGHT,
  NAV_ENTER
};


// =====================================================
// DATOS DEL ESCENARIO
// =====================================================

int batteryPercent = 76;

int pressureValue = 238;
int firstOcclusionPressure = 232;
int maxPressure = 242;

const char* occlusionTime = "00:07:00";
const char* timeToOcclusion = "00:00:24";
const char* lastReadjustment = "00:00:06";
const char* currentReadjustment = "--:--:--";

int readjustmentCount = 1;

const char* operationalState = "OCLUSION ALCANZADA";

bool audioMuted = false;
bool screenLocked = true;

bool conditionLoss = false;
bool conditionBatteryLow = false;

bool procedureActive = true;
bool recordingActive = true;

const char* recordId = "TNG-0013";
int storedRecords = 12;

int brightnessPercent = 60;


// =====================================================
// NAVEGACION / FOCOS
// =====================================================

// Footer:
// 0 = Audio
// 1 = Candado
// 2 = Alertas
// 3 = Menu

int footerFocus = 0;

// Menu 2 x 2:
// 0 Principal
// 1 Detalles
// 2 Alertas
// 3 Sistema

int menuFocus = 0;

// Detalles: 0 o 1
int detailsPage = 0;

// Sistema:
// 0 brillo -
// 1 brillo +
// 2 registros
// 3 audio
// 4 candado
// 5 alertas
// 6 menu

int systemFocus = 0;

// Registros:
// 0 transferir
// 1 audio
// 2 candado
// 3 alertas
// 4 menu

int recordsFocus = 0;


// =====================================================
// COLORES
// =====================================================

uint16_t BG;
uint16_t WHITE;
uint16_t NAVY;
uint16_t TEXT;
uint16_t MUTED;
uint16_t BORDER;

uint16_t GREEN;
uint16_t GREEN_LIGHT;

uint16_t AMBER;
uint16_t AMBER_BG;

uint16_t RED;
uint16_t RED_BG;

uint16_t FOCUS_BG;
uint16_t DISABLED_BG;
uint16_t DISABLED_TEXT;


// =====================================================
// ICONO SONIDO 32 x 32
// =====================================================

const uint8_t ICON_SOUND[] PROGMEM = {

  0x00,0x0F,0xF0,0x00,
  0x00,0x7F,0xFE,0x00,
  0x01,0xF0,0x0F,0x80,
  0x03,0xC0,0x03,0xC0,
  0x07,0x00,0x00,0xE0,
  0x0E,0x00,0x00,0x70,
  0x1C,0x00,0x00,0x38,
  0x38,0x00,0x00,0x1C,

  0x30,0x01,0x8C,0x0C,
  0x70,0x03,0x8E,0x0E,
  0x60,0x07,0x86,0x06,
  0x60,0x0F,0x37,0x06,
  0xC0,0xFF,0xB3,0x03,
  0xC0,0xFF,0xBB,0x03,
  0xC0,0xFF,0x9B,0x03,
  0xC0,0xFF,0x9B,0x83,

  0xC0,0xFF,0x9B,0x03,
  0xC0,0xFF,0x9B,0x03,
  0xC0,0xFF,0xBB,0x03,
  0xC0,0xFF,0x33,0x03,
  0x60,0x0F,0x67,0x06,
  0x60,0x07,0x8E,0x06,
  0x70,0x03,0x8C,0x0E,
  0x30,0x01,0x88,0x0C,

  0x18,0x00,0x00,0x1C,
  0x1C,0x00,0x00,0x38,
  0x0E,0x00,0x00,0x70,
  0x07,0x00,0x00,0xE0,
  0x03,0xC0,0x03,0xC0,
  0x01,0xF0,0x0F,0x80,
  0x00,0x7F,0xFE,0x00,
  0x00,0x0F,0xF0,0x00
};


// =====================================================
// ICONO ALERTA
// Convertido de TU imagen.
// 28 x 28
// =====================================================

const uint8_t ICON_ALERT[] PROGMEM = {

  0x00,0x00,0x00,0x00,
  0x00,0x0F,0x00,0x00,
  0x00,0x1F,0x80,0x00,
  0x00,0x3F,0xC0,0x00,

  0x00,0x3F,0xC0,0x00,
  0x00,0x6F,0x60,0x00,
  0x00,0x7F,0xE0,0x00,
  0x00,0xDF,0xB0,0x00,

  0x01,0xBF,0xF0,0x00,
  0x01,0xFF,0xF8,0x00,
  0x03,0x79,0xEC,0x00,
  0x03,0xF9,0xFC,0x00,

  0x06,0xF9,0xF6,0x00,
  0x07,0xF9,0xFE,0x00,
  0x0D,0xF9,0xFB,0x00,
  0x1B,0xF9,0xFD,0x80,

  0x1F,0xFB,0xFF,0x80,
  0x37,0xFF,0xFE,0xC0,
  0x3F,0xFF,0xFF,0xC0,
  0x6F,0xF9,0xFF,0x60,

  0x7F,0xFF,0xFF,0xE0,
  0xFF,0xFF,0xFF,0xF0,
  0xFF,0xFF,0xFF,0xF0,
  0xDF,0xFF,0xFF,0xB0,

  0xFF,0xFF,0xFF,0xF0,
  0x7F,0xFF,0xFF,0xE0,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00
};


// =====================================================
// FUNCIONES GENERALES
// =====================================================

void centerFreeText(
  const char* text,
  int x,
  int y,
  const GFXfont* font,
  uint16_t color,
  uint16_t background
) {

  tft.setFreeFont(font);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, background);

  tft.drawString(
    text,
    x,
    y,
    GFXFF
  );

  tft.setTextDatum(TL_DATUM);
}


// =====================================================
// CANTIDAD DE CONDICIONES
// =====================================================

int activeConditionCount() {

  int count = 0;

  if (conditionLoss) {
    count++;
  }

  if (conditionBatteryLow) {
    count++;
  }

  return count;
}


// =====================================================
// BATERIA
// =====================================================

void drawBattery(
  int x,
  int y,
  int percent
) {

  uint16_t batteryColor = GREEN;

  if (percent <= 20) {
    batteryColor = RED;
  }
  else if (percent <= 40) {
    batteryColor = AMBER;
  }


  tft.setFreeFont(FSSB12);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(NAVY, WHITE);

  tft.drawString(
    String(percent) + "%",
    x - 10,
    y + 11,
    GFXFF
  );


  tft.drawRoundRect(
    x,
    y,
    40,
    22,
    3,
    NAVY
  );


  tft.fillRect(
    x + 40,
    y + 6,
    4,
    10,
    NAVY
  );


  int fillWidth =
    map(percent, 0, 100, 0, 33);


  if (fillWidth > 0) {

    tft.fillRect(
      x + 3,
      y + 3,
      fillWidth,
      16,
      batteryColor
    );
  }


  tft.setTextDatum(TL_DATUM);
}


// =====================================================
// CABECERA
// =====================================================

void drawHeader(
  const char* title
) {

  tft.fillRect(
    0,
    0,
    480,
    42,
    WHITE
  );


  tft.drawFastHLine(
    0,
    41,
    480,
    BORDER
  );


  tft.setFreeFont(FSSB12);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(NAVY, WHITE);

  tft.drawString(
    title,
    18,
    21,
    GFXFF
  );


  drawBattery(
    418,
    10,
    batteryPercent
  );


  tft.setTextDatum(TL_DATUM);
}


// =====================================================
// ESTADO OPERATIVO
// =====================================================

void drawOperationalState() {

  uint16_t stateBG = GREEN_LIGHT;
  uint16_t stateBorder = GREEN;


  if (conditionLoss) {

    stateBG =
      tft.color565(
        255,
        225,
        205
      );

    stateBorder = AMBER;
  }


  if (!procedureActive) {

    stateBG =
      tft.color565(
        232,
        236,
        240
      );

    stateBorder = MUTED;
  }


  tft.fillRoundRect(
    18,
    49,
    444,
    43,
    7,
    stateBG
  );


  tft.drawRoundRect(
    18,
    49,
    444,
    43,
    7,
    stateBorder
  );


  tft.fillRoundRect(
    18,
    49,
    7,
    43,
    3,
    stateBorder
  );


  centerFreeText(
    operationalState,
    240,
    71,
    FSSB12,
    NAVY,
    stateBG
  );
}


// =====================================================
// AUDIO
// =====================================================

void drawAudioIcon(
  int x,
  int y,
  bool focused
) {

  if (focused) {

    tft.fillRoundRect(
      x - 7,
      y - 7,
      46,
      46,
      8,
      FOCUS_BG
    );


    tft.drawRoundRect(
      x - 7,
      y - 7,
      46,
      46,
      8,
      NAVY
    );
  }


  uint16_t iconColor =
    audioMuted ? AMBER : NAVY;


  tft.drawBitmap(
    x,
    y,
    ICON_SOUND,
    32,
    32,
    iconColor
  );


  if (audioMuted) {

    tft.drawLine(
      x + 3,
      y + 3,
      x + 29,
      y + 29,
      AMBER
    );

    tft.drawLine(
      x + 4,
      y + 3,
      x + 30,
      y + 29,
      AMBER
    );
  }
}


// =====================================================
// CANDADO
// =====================================================

void drawLockIcon(
  int x,
  int y,
  bool focused
) {

  uint16_t lockColor =
    screenLocked ? GREEN : AMBER;


  if (focused) {

    tft.fillRoundRect(
      x - 5,
      y - 5,
      46,
      46,
      8,
      FOCUS_BG
    );


    tft.drawRoundRect(
      x - 5,
      y - 5,
      46,
      46,
      8,
      NAVY
    );
  }


  tft.fillRoundRect(
    x,
    y,
    36,
    36,
    7,
    lockColor
  );


  tft.drawRoundRect(
    x + 10,
    y + 5,
    16,
    16,
    7,
    WHITE
  );


  tft.fillRoundRect(
    x + 7,
    y + 17,
    22,
    14,
    3,
    WHITE
  );


  tft.fillCircle(
    x + 18,
    y + 23,
    2,
    lockColor
  );


  tft.fillRect(
    x + 17,
    y + 23,
    3,
    5,
    lockColor
  );
}


// =====================================================
// ACCESO DIRECTO A ALERTAS
// Aparece con contador si hay 2 o mas condiciones.
// =====================================================

void drawAlertAccess(
  int x,
  int y,
  bool focused
) {

  int count =
    activeConditionCount();


  if (count < 2) {
    return;
  }


  if (focused) {

    tft.fillRoundRect(
      x - 6,
      y - 6,
      52,
      42,
      8,
      FOCUS_BG
    );

    tft.drawRoundRect(
      x - 6,
      y - 6,
      52,
      42,
      8,
      NAVY
    );
  }


  tft.drawBitmap(
    x,
    y,
    ICON_ALERT,
    28,
    28,
    RED
  );


  // contador
  tft.fillCircle(
    x + 31,
    y + 4,
    10,
    RED
  );


  String countText =
    String(count);


  centerFreeText(
    countText.c_str(),
    x + 31,
    y + 4,
    FSSB9,
    WHITE,
    RED
  );
}


// =====================================================
// BOTON MENU
// =====================================================

void drawMenuButton(
  bool focused
) {

  uint16_t background =
    focused ? FOCUS_BG : WHITE;


  uint16_t outline =
    focused ? NAVY : BORDER;


  tft.fillRoundRect(
    342,
    274,
    120,
    38,
    7,
    background
  );


  tft.drawRoundRect(
    342,
    274,
    120,
    38,
    7,
    outline
  );


  if (focused) {

    tft.drawRoundRect(
      340,
      272,
      124,
      42,
      8,
      NAVY
    );
  }


  centerFreeText(
    "MENU",
    402,
    293,
    FSSB12,
    NAVY,
    background
  );
}


// =====================================================
// FOOTER GENERAL
// =====================================================

void drawFooter(
  int focus
) {

  tft.fillRect(
    0,
    267,
    480,
    53,
    WHITE
  );


  tft.drawFastHLine(
    0,
    267,
    480,
    BORDER
  );


  drawAudioIcon(
    27,
    279,
    focus == 0
  );


  drawLockIcon(
    82,
    277,
    focus == 1
  );


  drawAlertAccess(
    145,
    280,
    focus == 2
  );


  if (audioMuted) {

    centerFreeText(
      "00:30",
      231,
      293,
      FSSB9,
      MUTED,
      WHITE
    );
  }


  drawMenuButton(
    focus == 3
  );
}


// =====================================================
// FRANJA DE AVISO / ALARMA
// =====================================================

void drawCurrentAlertStrip() {

  if (conditionLoss) {

    tft.fillRoundRect(
      18,
      218,
      444,
      42,
      7,
      RED_BG
    );


    tft.drawRoundRect(
      18,
      218,
      444,
      42,
      7,
      RED
    );


    centerFreeText(
      "PERDIDA DE OCLUSION",
      240,
      239,
      FSSB12,
      WHITE,
      RED_BG
    );

    return;
  }


  if (conditionBatteryLow) {

    tft.fillRoundRect(
      18,
      218,
      444,
      42,
      7,
      AMBER_BG
    );


    tft.drawRoundRect(
      18,
      218,
      444,
      42,
      7,
      AMBER
    );


    centerFreeText(
      "BATERIA BAJA",
      240,
      239,
      FSSB12,
      NAVY,
      AMBER_BG
    );
  }
}


// =====================================================
// PRINCIPAL
// =====================================================

void drawPrincipal() {

  tft.fillScreen(BG);

  drawHeader(
    "PRINCIPAL"
  );


  drawOperationalState();


  // ---------------------------------------------------
  // Dos tarjetas iguales
  // ---------------------------------------------------

  const int CARD_W = 217;
  const int CARD_H = 110;
  const int CARD_Y = 100;

  const int PRESS_X = 18;
  const int TIME_X = 245;


  // ===================================================
  // PRESION EJERCIDA
  // ===================================================

  tft.fillRoundRect(
    PRESS_X,
    CARD_Y,
    CARD_W,
    CARD_H,
    7,
    WHITE
  );


  tft.drawRoundRect(
    PRESS_X,
    CARD_Y,
    CARD_W,
    CARD_H,
    7,
    BORDER
  );


  centerFreeText(
    "PRESION EJERCIDA",
    PRESS_X + CARD_W / 2,
    120,
    FSSB9,
    NAVY,
    WHITE
  );


  tft.setFreeFont(FSSB24);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, WHITE);


  String pressureText;

  if (pressureValue >= 0) {
    pressureText = String(pressureValue);
  }
  else {
    pressureText = "---";
  }


  tft.drawString(
    pressureText,
    83,
    169,
    GFXFF
  );


  tft.setFreeFont(FSSB12);
  tft.setTextColor(MUTED, WHITE);


  tft.drawString(
    "mmHg",
    185,
    170,
    GFXFF
  );


  tft.setTextDatum(TL_DATUM);


  // ===================================================
  // TIEMPO
  // ===================================================

  tft.fillRoundRect(
    TIME_X,
    CARD_Y,
    CARD_W,
    CARD_H,
    7,
    WHITE
  );


  tft.drawRoundRect(
    TIME_X,
    CARD_Y,
    CARD_W,
    CARD_H,
    7,
    BORDER
  );


  centerFreeText(
    "TIEMPO DE OCLUSION",
    TIME_X + CARD_W / 2,
    120,
    FSSB9,
    NAVY,
    WHITE
  );


  centerFreeText(
    occlusionTime,
    TIME_X + CARD_W / 2,
    169,
    FSSB24,
    TEXT,
    WHITE
  );


  drawCurrentAlertStrip();

  drawFooter(
    footerFocus
  );
}


// =====================================================
// MENU 2 x 2
// =====================================================

void drawMenuOption(
  int x,
  int y,
  int w,
  int h,
  const char* text,
  bool focused
) {

  // Nunca negro:
  // blanco normal / azul claro con foco

  uint16_t background =
    focused ? FOCUS_BG : WHITE;


  uint16_t outline =
    focused ? NAVY : BORDER;


  tft.fillRoundRect(
    x,
    y,
    w,
    h,
    8,
    background
  );


  tft.drawRoundRect(
    x,
    y,
    w,
    h,
    8,
    outline
  );


  if (focused) {

    tft.drawRoundRect(
      x - 2,
      y - 2,
      w + 4,
      h + 4,
      9,
      NAVY
    );
  }


  centerFreeText(
    text,
    x + w / 2,
    y + h / 2,
    FSSB12,
    NAVY,
    background
  );
}


void drawMenu() {

  tft.fillScreen(BG);

  drawHeader(
    "MENU"
  );


  drawMenuOption(
    18,
    60,
    214,
    80,
    "PRINCIPAL",
    menuFocus == 0
  );


  drawMenuOption(
    248,
    60,
    214,
    80,
    "DETALLES",
    menuFocus == 1
  );


  drawMenuOption(
    18,
    154,
    214,
    80,
    "ALERTAS",
    menuFocus == 2
  );


  drawMenuOption(
    248,
    154,
    214,
    80,
    "SISTEMA",
    menuFocus == 3
  );


  // Estado inferior, sin foco.
  tft.fillRect(
    0,
    252,
    480,
    68,
    WHITE
  );


  drawAudioIcon(
    27,
    273,
    false
  );


  drawLockIcon(
    82,
    271,
    false
  );


  drawAlertAccess(
    145,
    274,
    false
  );


  if (audioMuted) {

    centerFreeText(
      "00:30",
      231,
      289,
      FSSB9,
      MUTED,
      WHITE
    );
  }
}


// =====================================================
// FILA DE DETALLE
// =====================================================

void drawDetailRow(
  int y,
  const char* label,
  const char* value
) {

  tft.fillRoundRect(
    18,
    y,
    444,
    20,
    4,
    WHITE
  );


  tft.setFreeFont(FSSB9);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(NAVY, WHITE);


  tft.drawString(
    label,
    28,
    y + 10,
    GFXFF
  );


  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(TEXT, WHITE);


  tft.drawString(
    value,
    451,
    y + 10,
    GFXFF
  );


  tft.setTextDatum(TL_DATUM);
}


// =====================================================
// DETALLES
// =====================================================

void drawDetalles() {

  tft.fillScreen(BG);

  drawHeader(
    "DETALLES"
  );


  drawOperationalState();


  if (detailsPage == 0) {

    String p1 =
      String(pressureValue) + " mmHg";

    String p2 =
      String(firstOcclusionPressure) + " mmHg";

    String p3 =
      String(maxPressure) + " mmHg";

    String r =
      String(readjustmentCount);


    drawDetailRow(
      99,
      "PRESION ACTUAL",
      p1.c_str()
    );


    drawDetailRow(
      122,
      "PRESION AL ALCANZAR OCLUSION",
      p2.c_str()
    );


    drawDetailRow(
      145,
      "PRESION MAXIMA REGISTRADA",
      p3.c_str()
    );


    drawDetailRow(
      168,
      "TIEMPO DE OCLUSION",
      occlusionTime
    );


    drawDetailRow(
      191,
      "REAJUSTES AUTOMATICOS",
      r.c_str()
    );
  }

  else {

    drawDetailRow(
      108,
      "TIEMPO HASTA OCLUSION",
      timeToOcclusion
    );


    drawDetailRow(
      144,
      "DURACION ULTIMO REAJUSTE",
      lastReadjustment
    );


    drawDetailRow(
      180,
      "REAJUSTE EN CURSO",
      currentReadjustment
    );
  }


  String pageText =
    String(detailsPage + 1)
    + "/2";


  centerFreeText(
    pageText.c_str(),
    430,
    207,
    FSSB9,
    MUTED,
    BG
  );


  drawCurrentAlertStrip();

  drawFooter(
    footerFocus
  );
}


// =====================================================
// ALERTAS ACTIVAS
// =====================================================

void drawAlertas() {

  tft.fillScreen(BG);

  drawHeader(
    "ALERTAS ACTIVAS"
  );


  drawOperationalState();


  if (
    !conditionLoss
    &&
    !conditionBatteryLow
  ) {

    centerFreeText(
      "NO HAY ALERTAS ACTIVAS",
      240,
      155,
      FSSB12,
      MUTED,
      BG
    );
  }


  else {

    int y = 106;


    if (conditionLoss) {

      tft.fillRoundRect(
        18,
        y,
        444,
        48,
        7,
        RED_BG
      );


      tft.drawBitmap(
        31,
        y + 10,
        ICON_ALERT,
        28,
        28,
        WHITE
      );


      centerFreeText(
        "ALARMA - PERDIDA DE OCLUSION",
        265,
        y + 24,
        FSSB12,
        WHITE,
        RED_BG
      );


      y += 58;
    }


    if (conditionBatteryLow) {

      tft.fillRoundRect(
        18,
        y,
        444,
        48,
        7,
        AMBER_BG
      );


      tft.drawBitmap(
        31,
        y + 10,
        ICON_ALERT,
        28,
        28,
        AMBER
      );


      centerFreeText(
        "AVISO - BATERIA BAJA",
        255,
        y + 24,
        FSSB12,
        NAVY,
        AMBER_BG
      );
    }
  }


  drawFooter(
    footerFocus
  );
}


// =====================================================
// BOTON PEQUENO
// =====================================================

void drawSmallButton(
  int x,
  int y,
  int w,
  int h,
  const char* text,
  bool focused,
  bool enabled = true
) {

  uint16_t background;
  uint16_t foreground;
  uint16_t outline;


  if (!enabled) {

    background =
      DISABLED_BG;

    foreground =
      DISABLED_TEXT;

    outline =
      BORDER;
  }

  else if (focused) {

    background =
      FOCUS_BG;

    foreground =
      NAVY;

    outline =
      NAVY;
  }

  else {

    background =
      WHITE;

    foreground =
      NAVY;

    outline =
      BORDER;
  }


  tft.fillRoundRect(
    x,
    y,
    w,
    h,
    5,
    background
  );


  tft.drawRoundRect(
    x,
    y,
    w,
    h,
    5,
    outline
  );


  centerFreeText(
    text,
    x + w / 2,
    y + h / 2,
    FSSB9,
    foreground,
    background
  );
}


// =====================================================
// SISTEMA
// =====================================================

void drawSistema() {

  tft.fillScreen(BG);

  drawHeader(
    "SISTEMA"
  );


  drawOperationalState();


  // ===================================================
  // BRILLO
  // ===================================================

  tft.fillRoundRect(
    18,
    100,
    444,
    34,
    6,
    WHITE
  );


  tft.setFreeFont(FSSB9);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(NAVY, WHITE);

  tft.drawString(
    "BRILLO",
    30,
    117,
    GFXFF
  );


  drawSmallButton(
    245,
    103,
    42,
    28,
    "-",
    systemFocus == 0
  );


  String bright =
    String(brightnessPercent)
    + "%";


  centerFreeText(
    bright.c_str(),
    330,
    117,
    FSSB9,
    TEXT,
    WHITE
  );


  drawSmallButton(
    382,
    103,
    42,
    28,
    "+",
    systemFocus == 1
  );


  // ===================================================
  // SENSOR / BATERIA
  // ===================================================

  tft.fillRoundRect(
    18,
    141,
    217,
    34,
    6,
    WHITE
  );


  centerFreeText(
    "SENSOR PRESION   OK",
    126,
    158,
    FSSB9,
    NAVY,
    WHITE
  );


  tft.fillRoundRect(
    245,
    141,
    217,
    34,
    6,
    WHITE
  );


  String battText =
    String("BATERIA   ")
    + String(batteryPercent)
    + "%";


  centerFreeText(
    battText.c_str(),
    353,
    158,
    FSSB9,
    NAVY,
    WHITE
  );


  // ===================================================
  // FIRMWARE / REGISTROS
  // ===================================================

  tft.fillRoundRect(
    18,
    182,
    217,
    29,
    6,
    WHITE
  );


  centerFreeText(
    "FIRMWARE   vX.X",
    126,
    196,
    FSSB9,
    NAVY,
    WHITE
  );


  drawSmallButton(
    245,
    182,
    217,
    29,
    "REGISTROS",
    systemFocus == 2
  );


  drawCurrentAlertStrip();


  int footerVisualFocus = -1;

  if (systemFocus >= 3) {

    footerVisualFocus =
      systemFocus - 3;
  }


  drawFooter(
    footerVisualFocus
  );
}


// =====================================================
// REGISTROS
// =====================================================

void drawRecordsRow(
  int x,
  int y,
  int w,
  const char* label,
  const char* value
) {

  tft.fillRoundRect(
    x,
    y,
    w,
    38,
    6,
    WHITE
  );


  tft.setFreeFont(FSSB9);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(MUTED, WHITE);


  tft.drawString(
    label,
    x + 10,
    y + 11,
    GFXFF
  );


  tft.setTextColor(NAVY, WHITE);


  tft.drawString(
    value,
    x + 10,
    y + 27,
    GFXFF
  );


  tft.setTextDatum(TL_DATUM);
}


void drawRegistros() {

  tft.fillScreen(BG);

  drawHeader(
    "REGISTROS"
  );


  drawOperationalState();


  drawRecordsRow(
    18,
    101,
    217,
    "TARJETA microSD",
    "DISPONIBLE"
  );


  drawRecordsRow(
    245,
    101,
    217,
    procedureActive
      ? "REGISTRO ACTUAL"
      : "ULTIMO REGISTRO",
    recordId
  );


  String stored =
    String(storedRecords);


  drawRecordsRow(
    18,
    146,
    217,
    "REGISTROS GUARDADOS",
    stored.c_str()
  );


  drawRecordsRow(
    245,
    146,
    217,
    "ESTADO",
    recordingActive
      ? "GUARDANDO DATOS"
      : "REGISTRO CERRADO"
  );


  drawSmallButton(
    18,
    191,
    444,
    27,
    "TRANSFERIR A PC",
    recordsFocus == 0,
    !procedureActive
  );


  drawCurrentAlertStrip();


  int footerVisualFocus = -1;

  if (recordsFocus >= 1) {

    footerVisualFocus =
      recordsFocus - 1;
  }


  drawFooter(
    footerVisualFocus
  );
}


// =====================================================
// TRANSFERIR A PC
// =====================================================

void drawTransferir() {

  tft.fillScreen(BG);

  drawHeader(
    "TRANSFERIR A PC"
  );


  centerFreeText(
    "ESPERANDO A LA COMPUTADORA",
    240,
    92,
    FSSB12,
    NAVY,
    BG
  );


  centerFreeText(
    "CONECTA EL CABLE USB",
    240,
    135,
    FSSB9,
    TEXT,
    BG
  );


  centerFreeText(
    "Y ABRI EL VISOR EN LA PC",
    240,
    159,
    FSSB9,
    TEXT,
    BG
  );


  centerFreeText(
    "ELEGI LOS REGISTROS DESDE LA COMPUTADORA",
    240,
    193,
    FSSB9,
    MUTED,
    BG
  );


  String available =
    String(storedRecords)
    + " REGISTROS DISPONIBLES";


  centerFreeText(
    available.c_str(),
    240,
    225,
    FSSB12,
    NAVY,
    BG
  );


  drawSmallButton(
    170,
    270,
    140,
    38,
    "VOLVER",
    true
  );
}


// =====================================================
// REDIBUJAR PANTALLA
// =====================================================

void redrawScreen() {

  if (
    currentScreen
    == SCREEN_PRINCIPAL
  ) {

    drawPrincipal();
  }


  else if (
    currentScreen
    == SCREEN_MENU
  ) {

    drawMenu();
  }


  else if (
    currentScreen
    == SCREEN_DETALLES
  ) {

    drawDetalles();
  }


  else if (
    currentScreen
    == SCREEN_ALERTAS
  ) {

    drawAlertas();
  }


  else if (
    currentScreen
    == SCREEN_SISTEMA
  ) {

    drawSistema();
  }


  else if (
    currentScreen
    == SCREEN_REGISTROS
  ) {

    drawRegistros();
  }


  else if (
    currentScreen
    == SCREEN_TRANSFERIR
  ) {

    drawTransferir();
  }
}


// =====================================================
// NAVEGACION FOOTER
// =====================================================

bool footerItemAvailable(
  int item
) {

  if (item == 2) {

    return
      activeConditionCount()
      >= 2;
  }


  return true;
}


void moveFooter(
  int direction
) {

  int candidate =
    footerFocus;


  while (true) {

    candidate +=
      direction;


    if (
      candidate < 0
      ||
      candidate > 3
    ) {

      return;
    }


    if (
      footerItemAvailable(
        candidate
      )
    ) {

      footerFocus =
        candidate;

      return;
    }
  }
}


void activateFooter() {

  // Audio
  if (footerFocus == 0) {

    audioMuted =
      !audioMuted;

    redrawScreen();

    return;
  }


  // Candado
  if (footerFocus == 1) {

    screenLocked =
      !screenLocked;

    redrawScreen();

    return;
  }


  // Alertas
  if (
    footerFocus == 2
    &&
    activeConditionCount() >= 2
  ) {

    currentScreen =
      SCREEN_ALERTAS;

    footerFocus =
      0;

    redrawScreen();

    return;
  }


  // Menu
  if (footerFocus == 3) {

    currentScreen =
      SCREEN_MENU;

    menuFocus =
      0;

    redrawScreen();
  }
}


// =====================================================
// NAVEGACION PRINCIPAL
// =====================================================

void navigatePrincipal(
  NavEvent event
) {

  if (event == NAV_LEFT) {

    moveFooter(-1);

    drawPrincipal();
  }


  else if (event == NAV_RIGHT) {

    moveFooter(1);

    drawPrincipal();
  }


  else if (event == NAV_ENTER) {

    activateFooter();
  }
}


// =====================================================
// NAVEGACION MENU
// =====================================================

void navigateMenu(
  NavEvent event
) {

  if (event == NAV_LEFT) {

    if (menuFocus == 1) {
      menuFocus = 0;
    }

    else if (menuFocus == 3) {
      menuFocus = 2;
    }
  }


  else if (event == NAV_RIGHT) {

    if (menuFocus == 0) {
      menuFocus = 1;
    }

    else if (menuFocus == 2) {
      menuFocus = 3;
    }
  }


  else if (event == NAV_UP) {

    if (menuFocus == 2) {
      menuFocus = 0;
    }

    else if (menuFocus == 3) {
      menuFocus = 1;
    }
  }


  else if (event == NAV_DOWN) {

    if (menuFocus == 0) {
      menuFocus = 2;
    }

    else if (menuFocus == 1) {
      menuFocus = 3;
    }
  }


  else if (event == NAV_ENTER) {

    if (menuFocus == 0) {

      currentScreen =
        SCREEN_PRINCIPAL;

      footerFocus =
        0;
    }


    else if (menuFocus == 1) {

      currentScreen =
        SCREEN_DETALLES;

      detailsPage =
        0;

      footerFocus =
        0;
    }


    else if (menuFocus == 2) {

      currentScreen =
        SCREEN_ALERTAS;

      footerFocus =
        0;
    }


    else if (menuFocus == 3) {

      currentScreen =
        SCREEN_SISTEMA;

      systemFocus =
        0;
    }


    redrawScreen();

    return;
  }


  drawMenu();
}


// =====================================================
// NAVEGACION DETALLES
// =====================================================

void navigateDetalles(
  NavEvent event
) {

  // Arriba / abajo cambia página
  if (event == NAV_UP) {

    detailsPage =
      0;

    drawDetalles();
  }


  else if (event == NAV_DOWN) {

    detailsPage =
      1;

    drawDetalles();
  }


  // Izquierda / derecha recorre footer
  else if (event == NAV_LEFT) {

    moveFooter(-1);

    drawDetalles();
  }


  else if (event == NAV_RIGHT) {

    moveFooter(1);

    drawDetalles();
  }


  else if (event == NAV_ENTER) {

    activateFooter();
  }
}


// =====================================================
// NAVEGACION ALERTAS
// =====================================================

void navigateAlertas(
  NavEvent event
) {

  if (event == NAV_LEFT) {

    moveFooter(-1);

    drawAlertas();
  }


  else if (event == NAV_RIGHT) {

    moveFooter(1);

    drawAlertas();
  }


  else if (event == NAV_ENTER) {

    activateFooter();
  }
}


// =====================================================
// SISTEMA - DISPONIBILIDAD DE FOCO
// =====================================================

bool systemFocusAvailable(
  int item
) {

  if (
    item == 5
    &&
    activeConditionCount() < 2
  ) {

    return false;
  }


  return true;
}


void moveSystemFocus(
  int direction
) {

  int candidate =
    systemFocus;


  while (true) {

    candidate +=
      direction;


    if (
      candidate < 0
      ||
      candidate > 6
    ) {

      return;
    }


    if (
      systemFocusAvailable(
        candidate
      )
    ) {

      systemFocus =
        candidate;

      return;
    }
  }
}


// =====================================================
// NAVEGACION SISTEMA
// =====================================================

void navigateSistema(
  NavEvent event
) {

  if (event == NAV_LEFT) {

    moveSystemFocus(-1);

    drawSistema();

    return;
  }


  if (event == NAV_RIGHT) {

    moveSystemFocus(1);

    drawSistema();

    return;
  }


  if (event != NAV_ENTER) {

    return;
  }


  // Brillo -
  if (systemFocus == 0) {

    brightnessPercent -= 10;

    if (brightnessPercent < 10) {
      brightnessPercent = 10;
    }

    drawSistema();

    return;
  }


  // Brillo +
  if (systemFocus == 1) {

    brightnessPercent += 10;

    if (brightnessPercent > 100) {
      brightnessPercent = 100;
    }

    drawSistema();

    return;
  }


  // Registros
  if (systemFocus == 2) {

    currentScreen =
      SCREEN_REGISTROS;


    if (procedureActive) {

      recordsFocus =
        1;
    }

    else {

      recordsFocus =
        0;
    }


    drawRegistros();

    return;
  }


  // Audio
  if (systemFocus == 3) {

    audioMuted =
      !audioMuted;

    drawSistema();

    return;
  }


  // Candado
  if (systemFocus == 4) {

    screenLocked =
      !screenLocked;

    drawSistema();

    return;
  }


  // Alertas
  if (
    systemFocus == 5
    &&
    activeConditionCount() >= 2
  ) {

    currentScreen =
      SCREEN_ALERTAS;

    footerFocus =
      0;

    drawAlertas();

    return;
  }


  // Menu
  if (systemFocus == 6) {

    currentScreen =
      SCREEN_MENU;

    menuFocus =
      0;

    drawMenu();
  }
}


// =====================================================
// REGISTROS
// =====================================================

bool recordFocusAvailable(
  int item
) {

  // Transferir deshabilitado mientras
  // hay procedimiento activo.

  if (
    item == 0
    &&
    procedureActive
  ) {

    return false;
  }


  // Alertas sólo si hay 2 o más.
  if (
    item == 3
    &&
    activeConditionCount() < 2
  ) {

    return false;
  }


  return true;
}


void moveRecordsFocus(
  int direction
) {

  int candidate =
    recordsFocus;


  while (true) {

    candidate +=
      direction;


    if (
      candidate < 0
      ||
      candidate > 4
    ) {

      return;
    }


    if (
      recordFocusAvailable(
        candidate
      )
    ) {

      recordsFocus =
        candidate;

      return;
    }
  }
}


void navigateRegistros(
  NavEvent event
) {

  if (event == NAV_LEFT) {

    moveRecordsFocus(-1);

    drawRegistros();

    return;
  }


  if (event == NAV_RIGHT) {

    moveRecordsFocus(1);

    drawRegistros();

    return;
  }


  if (event != NAV_ENTER) {

    return;
  }


  // Transferir
  if (
    recordsFocus == 0
    &&
    !procedureActive
  ) {

    currentScreen =
      SCREEN_TRANSFERIR;

    drawTransferir();

    return;
  }


  // Audio
  if (recordsFocus == 1) {

    audioMuted =
      !audioMuted;

    drawRegistros();

    return;
  }


  // Candado
  if (recordsFocus == 2) {

    screenLocked =
      !screenLocked;

    drawRegistros();

    return;
  }


  // Alertas
  if (
    recordsFocus == 3
    &&
    activeConditionCount() >= 2
  ) {

    currentScreen =
      SCREEN_ALERTAS;

    footerFocus =
      0;

    drawAlertas();

    return;
  }


  // Menu
  if (recordsFocus == 4) {

    currentScreen =
      SCREEN_MENU;

    menuFocus =
      0;

    drawMenu();
  }
}


// =====================================================
// CONTROL GENERAL DE NAVEGACION
// =====================================================

void handleNavigation(
  NavEvent event
) {

  if (
    currentScreen
    == SCREEN_PRINCIPAL
  ) {

    navigatePrincipal(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_MENU
  ) {

    navigateMenu(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_DETALLES
  ) {

    navigateDetalles(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_ALERTAS
  ) {

    navigateAlertas(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_SISTEMA
  ) {

    navigateSistema(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_REGISTROS
  ) {

    navigateRegistros(
      event
    );

    return;
  }


  if (
    currentScreen
    == SCREEN_TRANSFERIR
  ) {

    if (event == NAV_ENTER) {

      currentScreen =
        SCREEN_REGISTROS;

      recordsFocus =
        0;

      drawRegistros();
    }
  }
}


// =====================================================
// ESCENARIO 0
// NORMAL
// =====================================================

void setNormalScenario() {

  batteryPercent =
    76;

  pressureValue =
    238;

  firstOcclusionPressure =
    232;

  maxPressure =
    242;

  occlusionTime =
    "00:07:00";

  timeToOcclusion =
    "00:00:24";

  lastReadjustment =
    "00:00:06";

  currentReadjustment =
    "--:--:--";

  readjustmentCount =
    1;

  operationalState =
    "OCLUSION ALCANZADA";

  conditionLoss =
    false;

  conditionBatteryLow =
    false;

  procedureActive =
    true;

  recordingActive =
    true;

  storedRecords =
    12;

  redrawScreen();
}


// =====================================================
// ESCENARIO 1
// AVISO BATERIA BAJA
// =====================================================

void setBatteryWarningScenario() {

  batteryPercent =
    12;

  pressureValue =
    238;

  occlusionTime =
    "00:08:11";

  operationalState =
    "OCLUSION ALCANZADA";

  conditionLoss =
    false;

  conditionBatteryLow =
    true;

  procedureActive =
    true;

  recordingActive =
    true;

  redrawScreen();
}


// =====================================================
// ESCENARIO 2
// PERDIDA OCLUSION + BATERIA BAJA
// =====================================================

void setMultiAlertScenario() {

  batteryPercent =
    12;

  pressureValue =
    204;

  firstOcclusionPressure =
    232;

  maxPressure =
    242;

  occlusionTime =
    "00:07:18";

  timeToOcclusion =
    "00:00:24";

  lastReadjustment =
    "00:00:06";

  currentReadjustment =
    "00:00:05";

  readjustmentCount =
    1;

  operationalState =
    "REAJUSTANDO";

  conditionLoss =
    true;

  conditionBatteryLow =
    true;

  procedureActive =
    true;

  recordingActive =
    true;

  storedRecords =
    12;

  redrawScreen();
}


// =====================================================
// ESCENARIO 9
// PROCEDIMIENTO TERMINADO
// =====================================================

void setClosedProcedureScenario() {

  batteryPercent =
    82;

  pressureValue =
    -1;

  occlusionTime =
    "--:--:--";

  operationalState =
    "SIN PROCEDIMIENTO ACTIVO";

  conditionLoss =
    false;

  conditionBatteryLow =
    false;

  procedureActive =
    false;

  recordingActive =
    false;

  storedRecords =
    13;

  redrawScreen();
}


// =====================================================
// SERIAL
//
// Navegacion:
// U arriba
// D abajo
// L izquierda
// R derecha
// E Enter
//
// Escenarios:
// 0 normal
// 1 bateria baja
// 2 perdida + bateria baja
// 9 procedimiento terminado
// =====================================================

void readSerialNavigation() {

  while (
    Serial.available()
  ) {

    char c =
      Serial.read();


    if (c == 'U') {

      handleNavigation(
        NAV_UP
      );
    }


    else if (c == 'D') {

      handleNavigation(
        NAV_DOWN
      );
    }


    else if (c == 'L') {

      handleNavigation(
        NAV_LEFT
      );
    }


    else if (c == 'R') {

      handleNavigation(
        NAV_RIGHT
      );
    }


    else if (c == 'E') {

      handleNavigation(
        NAV_ENTER
      );
    }


    else if (c == '0') {

      setNormalScenario();
    }


    else if (c == '1') {

      setBatteryWarningScenario();
    }


    else if (c == '2') {

      setMultiAlertScenario();
    }


    else if (c == '9') {

      setClosedProcedureScenario();
    }
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );


  tft.init();

  tft.setRotation(
    1
  );


  // ===================================================
  // PALETA
  // ===================================================

  BG =
    tft.color565(
      244,
      247,
      249
    );


  WHITE =
    TFT_WHITE;


  NAVY =
    tft.color565(
      31,
      54,
      78
    );


  TEXT =
    tft.color565(
      24,
      34,
      44
    );


  MUTED =
    tft.color565(
      88,
      100,
      112
    );


  BORDER =
    tft.color565(
      200,
      209,
      217
    );


  GREEN =
    tft.color565(
      36,
      145,
      92
    );


  GREEN_LIGHT =
    tft.color565(
      203,
      236,
      219
    );


  AMBER =
    tft.color565(
      180,
      105,
      0
    );


  AMBER_BG =
    tft.color565(
      255,
      194,
      51
    );


  RED =
    tft.color565(
      155,
      28,
      28
    );


  RED_BG =
    tft.color565(
      211,
      56,
      56
    );


  FOCUS_BG =
    tft.color565(
      222,
      232,
      241
    );


  DISABLED_BG =
    tft.color565(
      228,
      232,
      235
    );


  DISABLED_TEXT =
    tft.color565(
      140,
      148,
      155
    );


  drawPrincipal();


  Serial.println();
  Serial.println("TANGO listo");
  Serial.println();
  Serial.println("U D L R E = navegacion");
  Serial.println("0 = normal");
  Serial.println("1 = bateria baja");
  Serial.println("2 = perdida + bateria baja");
  Serial.println("9 = procedimiento terminado");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  readSerialNavigation();
}
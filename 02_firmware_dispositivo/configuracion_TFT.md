# Configuración de la TFT de TANGO

## Hardware actual

- Microcontrolador: ESP32 clásico / ESP-WROOM-32 / NodeMCU-32S
- Pantalla: TFT SPI 3.5"
- Resolución: 480 × 320 px
- Controlador utilizado: ILI9488
- Librería: `TFT_eSPI`
- Orientación de trabajo: horizontal (`setRotation(1)`)
- Touch: no utilizado actualmente

## Cableado actualmente funcional

| Señal TFT | GPIO ESP32 |
|---|---:|
| MOSI / SDI | 23 |
| SCK | 18 |
| CS | 17 |
| DC / RS | 16 |
| RESET | 4 |
| MISO / SDO | 19 en configuración de librería; no conectado físicamente en la configuración actual |
| GND | GND |
| LED | 25 |

## Configuración de `TFT_eSPI`

La instalación local de `TFT_eSPI` debe tener seleccionada una configuración equivalente a:

```cpp
#define ILI9488_DRIVER

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   17
#define TFT_DC   16
#define TFT_RST   4
```

El proyecto actual utiliza también `Free_Fonts.h` con fuentes FreeSansBold.

## Importante

- No modificar los pines sin verificar primero el cableado físico.
- La librería `TFT_eSPI` no se copia completa al repositorio.
- Si otra computadora clona el proyecto, debe instalar `TFT_eSPI` y reproducir esta configuración antes de compilar.

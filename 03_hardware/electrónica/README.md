# Hardware

Esta carpeta reúne todo lo relacionado con el hardware del proyecto: esquemáticos, diseños de las placas, modelos 3D y la lista de materiales.

## Estructura

- `schematics/`: esquemáticos del circuito
- `pcb/`: diseños de las 6 placas del proyecto
- `3d/`: modelos 3D de conectores y carcasa
- `materiales.md`: lista de materiales necesarios

## Placas

El proyecto se compone de 6 placas:

| Placa | Descripción |
|-------|-------------|
| `TANGO_Alarmas` | LEDs y Buzzer para aviso de situaciones críticas |
| `TANGO_Alimentación` | Reguladores de tensión para disminuir de 12V a 6V y 5V |
| `TANGO_Amplificación` | Filtros y capacitores de desacople para la señal del sensor de presión |
| `TANGO_Botonera` | Botones de control del Display |
| `TANGO_ESP` | ESP con conectores |
| `TANGO_Transistores` | Transistores para el control de las válvulas y bombas |

## Requisitos

- [KiCad 10](https://www.kicad.org/): todos los diseños fueron creados con esta versión. Con versiones anteriores es probable que los archivos no abran correctamente.

## Uso

1. Clonar el repositorio y abrir el archivo `.kicad_pro` de la placa que te interese.
2. Revisar el esquemático y el diseño de PCB desde KiCad.
3. Consultar `materiales.md` antes de comprar componentes.

## Especificaciones

- Tensión de operación: 12V

## Más información

Para las instrucciones de ensamblaje y la descripción general del proyecto, ver el [README principal](../README.md).

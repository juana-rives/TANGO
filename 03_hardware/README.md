# Hardware

Esta carpeta reúne todo lo relacionado con el hardware del proyecto, dividido en tres áreas: 
1. Electrónica
2. Neumática
3. Impresión 3D.

## Estructura

```
hardware/
├── electronica/    Esquemáticos, PCB y materiales de las 6 placas
├── neumatica/      Circuito neumático, componentes y materiales
└── impresion-3d/   Modelos 3D, piezas y parámetros de impresión
```

## Electrónica

Carpeta: [`03_hardware/electrónica/`](03_hardware/electrónica/) 

El proyecto se compone de 6 placas diseñadas en KiCad:

| Placa | Descripción |
|-------|-------------|
| `TANGO_Alarmas` | LEDs y Buzzer para aviso de situaciones críticas |
| `TANGO_Alimentación` | Reguladores de tensión para disminuir de 12V a 6V y 5V |
| `TANGO_Amplificación` | Filtros y capacitores de desacople para la señal del sensor de presión |
| `TANGO_Botonera` | Botones de control del Display |
| `TANGO_ESP` | ESP con conectores |
| `TANGO_Transistores` | Transistores para el control de las válvulas y bombas |

**Contenido:**
- `schematics/`: esquemáticos del circuito
- `pcb/`: diseños de las placas
- `materiales/`: lista de materiales de cada placa en CSV
  - [Placa 1](electronica/materiales/placa-1.csv)
  - [Placa 2](electronica/materiales/placa-2.csv)
  - [Placa 3](electronica/materiales/placa-3.csv)
  - [Placa 4](electronica/materiales/placa-4.csv)
  - [Placa 5](electronica/materiales/placa-5.csv)
  - [Placa 6](electronica/materiales/placa-6.csv)

**Requisitos:** [KiCad 10](https://www.kicad.org/). Con versiones anteriores es probable que los archivos no abran correctamente.

**Uso:**
1. Abrir el archivo `.kicad_pro` de la placa que te interese.
2. Revisar el esquemático y el diseño de PCB desde KiCad.
3. Consultar la lista de materiales antes de comprar componentes.

**Especificaciones:**
- Tensión de operación: 12V

## Neumática

Carpeta: [`neumatica/`](neumatica/)

Contiene todo lo relacionado con el sistema neumático del proyecto.

**Contenido:**
- `esquemas/`: diagrama del circuito neumático
- `materiales.csv`: lista de componentes (cilindros, válvulas, mangueras, fittings, etc.)

**Especificaciones:**
- Presión de trabajo: 0 mmHg - 300 mmHg
- Tipo de válvulas: Selenoidales de 6V y 12V

## Impresión 3D

Carpeta: [`impresion-3d/`](impresion-3d/)

Contiene las piezas que se fabrican por impresión 3D.

**Contenido:**
- `modelos/`: archivos fuente (STEP, etc.)
- `stl/`: archivos listos para imprimir
- `materiales.csv`: lista de piezas con cantidad y material

**Parámetros recomendados:**
- Material: *completar (PLA, PETG, etc.)*
- Altura de capa: *completar*
- Relleno: *completar*
- Soportes: *completar*

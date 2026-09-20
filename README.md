# TANGO

Proyecto final de Instrumentación Biomédica II.

TANGO es un prototipo de torniquete neumático automatizado para hemorragias externas graves de extremidades. El repositorio reúne la interfaz para el personal de salud, el firmware del dispositivo y la documentación de hardware.

## Estructura del repositorio

```text
TANGO/
├── 01_interfaz_medico/
├── 02_firmware_dispositivo/
├── 03_hardware/
│   ├── electronica/
│   └── neumatica/
├── README.md
└── .gitignore
```

### 01_interfaz_medico

Contiene la aplicación desarrollada en Streamlit para visualizar e integrar los registros generados por TANGO.

Archivos previstos:
- `TANGO_Interfaz.py`: aplicación principal.
- `logo.png`: logo utilizado por la interfaz.
- `requirements.txt`: dependencias de Python.

El entorno virtual `.venv` y la carpeta `.vscode` son locales y no se versionan.

### 02_firmware_dispositivo

Contiene el firmware del ESP32 y la HMI de la pantalla TFT.

Estructura sugerida:

```text
02_firmware_dispositivo/
├── TANGO_HMI/
│   ├── TANGO_HMI.ino
│   ├── Free_Fonts.h
│   └── icons.h
└── configuracion_TFT.md
```

`icons.h` sólo es necesario si los bitmaps se separan del archivo `.ino`.

La librería `TFT_eSPI` no se copia dentro del repositorio: se instala como librería de Arduino y su configuración específica se documenta en `configuracion_TFT.md`.

### 03_hardware

Contiene la documentación y los archivos editables del hardware.

```text
03_hardware/
├── electronica/
│   ├── kicad/
│   └── esquemas_exportados/
└── neumatica/
```

En `electronica/kicad/` se guardan los archivos editables de KiCad (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`).

En `esquemas_exportados/` pueden guardarse PDFs o imágenes para consulta rápida.

En `neumatica/` se guardan los diagramas del circuito neumático y, posteriormente, documentación de componentes.

## Ejecutar la interfaz médica

Desde `01_interfaz_medico`:

```powershell
uv venv
uv pip install -r requirements.txt
.\.venv\Scripts\streamlit.exe run TANGO_Interfaz.py
```

## Nota

Los archivos del repositorio representan el estado del prototipo y no implican validación clínica del dispositivo.

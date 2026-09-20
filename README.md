# TANGO

**TANGO** es un prototipo universitario de torniquete neumático automatizado para el control de hemorragias externas graves de extremidades.

El sistema está pensado para funcionar de forma autónoma en el dispositivo. La interfaz web es complementaria y se utiliza para consultar, asociar y visualizar registros de procedimientos.

> Estado actual: prototipo en desarrollo y demo funcional. Los datos de los registros incluidos en esta versión son sintéticos. No se implementó todavía persistencia real en microSD ni validación clínica.

---

## Estructura del repositorio

```text
TANGO/
│
├── 01_interfaz_medico/
│   ├── TANGO_Interfaz.py
│   ├── logo.png
│   └── requirements.txt
│
├── 02_firmware_dispositivo/
│   └── TANGO_HMI/
│       ├── TANGO_HMI.ino
│       └── Free_Fonts.h
│
├── 03_hardware/
│   ├── electronica/
│   └── neumatica/
│
├── herramientas_demo/
│   └── TANGO_Control.py
│
├── README.md
└── .gitignore
```

---

## 1. Firmware del dispositivo

El firmware principal se encuentra en:

```text
02_firmware_dispositivo/TANGO_HMI/TANGO_HMI.ino
```

La HMI utiliza una pantalla TFT de 480 × 320 px con `TFT_eSPI`.

El archivo:

```text
Free_Fonts.h
```

debe mantenerse en la misma carpeta del sketch.

### Funcionalidades actuales de la HMI

- Pantalla principal con presión ejercida y tiempo de oclusión.
- Menú de navegación.
- Pantallas de:
  - Principal.
  - Detalles.
  - Alertas.
  - Sistema.
  - Ajustes.
  - Información.
  - Registros.
  - Transferencia a PC.
- Bloqueo de pantalla.
- Silenciado temporal de alertas.
- Escenarios de demostración.
- Registro sintético de procedimientos.
- Comunicación USB serie con la interfaz médica.

### Escenarios demo

En:

```text
MENÚ → SISTEMA → ESCENARIOS DEMO
```

se encuentran escenarios estáticos para probar distintos estados de la HMI, incluyendo:

- comprobación inicial;
- preparado;
- inflado;
- búsqueda de oclusión;
- oclusión alcanzada;
- reajuste;
- batería baja/crítica;
- señal no confiable;
- presión no sostenida;
- sobrepresión;
- presión no alcanzada;
- oclusión no alcanzada;
- falla de sensor;
- pérdida de oclusión;
- autocomprobación fallida;
- procedimiento terminado.

También se incluye una **simulación secuencial de procedimiento** que recorre varios estados de forma acelerada y finaliza generando un registro de demostración.

Los escenarios son exclusivamente de prueba de interfaz y no representan un algoritmo clínico validado.

---

## 2. Registros demo

La versión actual incluye registros sintéticos en memoria del firmware:

```text
TNG-0011
TNG-0012
TNG-0013
```

Ejemplo de uso:

- `TNG-0011`: procedimiento normal.
- `TNG-0012`: procedimiento con reajuste.
- `TNG-0013`: procedimiento con pérdida de oclusión y batería baja.

Cada registro puede contener:

- ID del registro;
- tiempo total de oclusión;
- presión al alcanzar oclusión;
- presión máxima;
- tiempo hasta la oclusión inicial;
- cantidad de reajustes;
- historial de reajustes;
- historial de alertas.

Los tiempos se almacenan como segundos relativos al procedimiento y la interfaz los convierte a formato `HH:MM:SS`.

---

## 3. Comunicación USB

La comunicación entre el ESP32 y la PC se realiza por puerto serie a:

```text
115200 baudios
```

La interfaz médica puede consultar los registros sin necesidad de cargar archivos manualmente.

### Listar registros

La PC envía:

```text
@LIST
```

El ESP32 responde con la lista de registros disponibles.

### Obtener un registro

La PC envía, por ejemplo:

```text
@GET:TNG-0012
```

El ESP32 devuelve el registro correspondiente en formato JSON.

La respuesta se delimita con marcadores para facilitar su lectura:

```text
###TANGO_RECORD_BEGIN###
{ ... JSON ... }
###TANGO_RECORD_END###
```

En esta etapa los registros están almacenados en memoria del firmware. Más adelante, la misma lógica podrá utilizarse para leer registros persistidos en microSD.

---

## 4. Interfaz médica local

La interfaz está desarrollada con **Streamlit** y se encuentra en:

```text
01_interfaz_medico/TANGO_Interfaz.py
```

La aplicación se ejecuta de forma local en la computadora y se abre en el navegador.

### Flujo de uso

```text
1. Conectar TANGO por USB.
2. Abrir la interfaz Streamlit.
3. Seleccionar el puerto COM.
4. Conectar con TANGO.
5. Consultar los registros disponibles.
6. Seleccionar un registro.
7. Cargar el registro.
8. Completar los datos del paciente.
9. Visualizar el informe del procedimiento.
```

Los datos del paciente no se almacenan actualmente en el ESP32. Se asocian manualmente al registro desde la interfaz médica.

Si se cambia a un registro distinto, los datos del paciente ingresados para el registro anterior se limpian automáticamente.

### Información mostrada

La interfaz puede mostrar:

- presión al alcanzar oclusión;
- presión máxima;
- tiempo hasta la oclusión inicial;
- tiempo total de oclusión;
- cantidad de reajustes automáticos;
- historial de reajustes;
- historial de alertas;
- datos clínicos agregados manualmente;
- notas del profesional.

---

## 5. Instalación de la interfaz

Desde la carpeta:

```text
01_interfaz_medico
```

crear un entorno virtual:

```powershell
uv venv
```

Instalar dependencias:

```powershell
uv pip install --link-mode=copy -r requirements.txt
```

> Se utiliza `--link-mode=copy` porque el proyecto puede estar ubicado dentro de OneDrive y los hardlinks pueden generar errores en Windows.

Ejecutar la interfaz:

```powershell
.\.venv\Scripts\python.exe -m streamlit run TANGO_Interfaz.py
```

La aplicación se abrirá normalmente en:

```text
http://localhost:8501
```

### requirements.txt

```text
streamlit
pyserial
```

---

## 6. Herramienta de control de la TFT

Para las pruebas de navegación se incluye:

```text
herramientas_demo/TANGO_Control.py
```

Permite controlar la HMI desde la PC mediante:

```text
↑ ↓ ← → Enter
```

y algunos atajos de demostración.

### Importante

El puerto serie no puede estar abierto simultáneamente por dos programas.

Por lo tanto, no deben usarse al mismo tiempo:

```text
TANGO_Control.py
Streamlit
Arduino Serial Monitor
Arduino Serial Plotter
```

Antes de conectar la interfaz médica a TANGO, cerrar cualquier otro programa que esté usando el mismo puerto COM.

---

## 7. Alertas

En la pantalla principal se muestra únicamente el nombre de la alerta activa.

En la pestaña:

```text
ALERTAS ACTIVAS
```

cada alerta incluye además una descripción breve de la condición detectada.

Estas descripciones informan sobre el estado del sistema y no constituyen instrucciones clínicas.

---

## 8. Estado actual y pendientes

Actualmente implementado:

- HMI navegable.
- Escenarios demo.
- Simulación secuencial.
- Registros sintéticos.
- Selección de registros desde Streamlit.
- Transferencia USB serie.
- Asociación manual de datos del paciente.
- Visualización de métricas, reajustes y alertas.

Pendiente de desarrollo e integración:

- sensor de presión real;
- adquisición y procesamiento de señal;
- algoritmo closed-loop;
- bomba y válvulas;
- persistencia real en microSD;
- validación en fantoma;
- almacenamiento definitivo de series temporales;
- definición final de criterios y umbrales;
- validación de seguridad y funcionamiento.

---

## 9. Alcance

TANGO se limita al estudio de un torniquete neumático automatizado para hemorragias externas graves de extremidades.

El prototipo no debe presentarse como solución para:

- hemorragias internas;
- hemorragias de tronco;
- hemorragias de unión;
- situaciones clínicas no validadas experimentalmente.

La desaparición de oscilaciones en el manguito se considera un indicador indirecto y deberá validarse mediante una referencia independiente de flujo y/o presión en el banco de ensayo.

---

## 10. Validación prevista

La validación se realizará sin pacientes, mediante un fantoma con:

- tejido simulado;
- conducto flexible tipo arteria;
- circulación pulsátil;
- bomba externa;
- referencia independiente de flujo y/o presión.

La lógica de validación será:

```text
entrada conocida
→ sensado
→ procesamiento/decisión
→ actuación
→ resultado físico
→ comparación con referencia independiente
```

Se evaluarán, entre otros:

- presión mínima de oclusión;
- tiempo hasta oclusión;
- overshoot;
- estabilidad;
- reapertura;
- fugas;
- repetibilidad;
- falsos positivos/negativos;
- efecto del ruido de bomba y válvulas.

---

## 11. Nota de desarrollo

La versión actual de software tiene fines de integración y demostración del flujo completo:

```text
ESP32
→ USB serie
→ Streamlit local
→ selección de registro
→ asociación de datos del paciente
→ informe del procedimiento
```

No constituye todavía un dispositivo médico validado ni un sistema clínico listo para uso asistencial.

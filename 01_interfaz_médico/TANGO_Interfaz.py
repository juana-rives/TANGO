import streamlit as st
import base64
import csv
import html
import io
import json
import re
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

# ============================================================
# CONFIGURACIÓN DE PÁGINA
# ============================================================
st.set_page_config(
    page_title="TANGO — Informe Clínico",
    page_icon="🩸",
    layout="centered"
)

# ============================================================
# FUNCIONES DE VALIDACIÓN Y UTILIDADES
# ============================================================
BAUD_TANGO = 115200
BOOT_WAIT_SECONDS = 2.0
SERIAL_TIMEOUT_SECONDS = 4.0

ALERTAS_INFO = {
    "BATERIA BAJA": (
        "BATERÍA BAJA",
        "Nivel de batería reducido."
    ),
    "SENAL NO CONFIABLE": (
        "SEÑAL NO CONFIABLE",
        "La señal pulsátil no permite una evaluación confiable."
    ),
    "PRESION NO SOSTENIDA": (
        "PRESIÓN NO SOSTENIDA",
        "No se logra mantener estable la presión aplicada."
    ),
    "SOBREPRESION": (
        "SOBREPRESIÓN",
        "La presión aplicada superó el límite configurado."
    ),
    "PRESION NO ALCANZADA": (
        "PRESIÓN NO ALCANZADA",
        "No se logró alcanzar la presión objetivo."
    ),
    "OCLUSION NO ALCANZADA": (
        "OCLUSIÓN NO ALCANZADA",
        "No se alcanzó el criterio de oclusión esperado."
    ),
    "BATERIA CRITICA": (
        "BATERÍA CRÍTICA",
        "Nivel de batería críticamente bajo."
    ),
    "PRESION NO DISPONIBLE": (
        "PRESIÓN NO DISPONIBLE",
        "No se dispone de una medición válida de presión."
    ),
    "PERDIDA DE OCLUSION": (
        "PÉRDIDA DE OCLUSIÓN",
        "Se perdió el criterio de oclusión previamente alcanzado."
    ),
    "AUTOCOMPROBACION FALLIDA": (
        "AUTOCOMPROBACIÓN FALLIDA",
        "La comprobación inicial del sistema no fue superada."
    ),
}


def get_image_base64(image_path):
    """Convierte una imagen local en cadena Base64."""
    try:
        with open(image_path, "rb") as img_file:
            return base64.b64encode(img_file.read()).decode("utf-8")
    except Exception:
        return None


def validar_solo_letras(texto):
    """Valida que el texto solo contenga letras, espacios y caracteres acentuados."""
    if not texto or not texto.strip():
        return False
    patron = r"^[a-zA-ZáéíóúÁÉÍÓÚñÑ\s]+$"
    return bool(re.match(patron, texto.strip()))


def validar_formato_hora(hora_str):
    """Valida formato de hora HH:MM (24 horas). Opcional si está vacío."""
    if not hora_str or not hora_str.strip():
        return True
    patron = r"^(?:[01]\d|2[0-3]):[0-5]\d$"
    return bool(re.match(patron, hora_str.strip()))


def formatear_tiempo(segundos):
    """Convierte segundos de tiempo relativo a HH:MM:SS."""
    try:
        total = max(0, int(segundos))
    except (TypeError, ValueError):
        return "—"
    horas = total // 3600
    minutos = (total % 3600) // 60
    seg = total % 60
    return f"{horas:02d}:{minutos:02d}:{seg:02d}"


def obtener_clase_alerta(tipo_alerta):
    """Usa la severidad explícita enviada por TANGO."""
    return "alarm-critical" if str(tipo_alerta).lower() == "alarma" else "alarm-warning"


def metadata_alerta(codigo_o_texto):
    clave = str(codigo_o_texto or "").strip().upper()
    return ALERTAS_INFO.get(clave, (clave or "ALERTA DEL SISTEMA", ""))


def adaptar_registro_tango(raw):
    """Adapta el JSON del ESP32 al formato de visualización del informe."""
    metricas_raw = raw.get("metricas", {})
    sesion_raw = raw.get("sesion", {})

    reajustes = []
    for evento in raw.get("reajustes", []):
        reajustes.append({
            "momento": formatear_tiempo(evento.get("t_s")),
            "duracion": formatear_tiempo(evento.get("duracion_s")),
            "de_mmHg": evento.get("de_mmHg"),
            "a_mmHg": evento.get("a_mmHg"),
        })

    alertas = []
    for evento in raw.get("alertas", []):
        codigo = evento.get("codigo") or evento.get("texto", "")
        nombre, descripcion = metadata_alerta(codigo)
        alertas.append({
            "tipo": evento.get("tipo", "aviso"),
            "texto": nombre,
            "momento": formatear_tiempo(evento.get("t_s")),
            "detalle": descripcion,
        })

    return {
        "record_id": raw.get("record_id", "—"),
        "sesion": {
            "duracion_s": sesion_raw.get("duracion_s"),
            "duracion": formatear_tiempo(sesion_raw.get("duracion_s")),
        },
        "metricas": {
            "tiempo_oclusion_total": formatear_tiempo(
                metricas_raw.get("tiempo_oclusion_total_s")
            ),
            "presion_alcanzar_oclusion_mmHg": metricas_raw.get(
                "presion_alcanzar_oclusion_mmHg"
            ),
            "presion_maxima_mmHg": metricas_raw.get("presion_maxima_mmHg"),
            "tiempo_hasta_oclusion_inicial": formatear_tiempo(
                metricas_raw.get("tiempo_hasta_oclusion_inicial_s")
            ),
            "reajustes_automaticos": metricas_raw.get("reajustes_automaticos", 0),
        },
        "reajustes": reajustes,
        "alertas": alertas,
        "_raw": raw,
    }


def puerto_abierto():
    conexion = st.session_state.get("tango_serial")
    return conexion is not None and getattr(conexion, "is_open", False)


def cerrar_tango():
    conexion = st.session_state.get("tango_serial")
    if conexion is not None:
        try:
            conexion.close()
        except Exception:
            pass
    st.session_state.tango_serial = None
    st.session_state.tango_port = None
    st.session_state.registros_disponibles = []


def abrir_tango(puerto):
    if serial is None:
        raise RuntimeError("Falta pyserial. Instalalo con: uv pip install pyserial")

    cerrar_tango()

    conexion = serial.Serial(
        port=None,
        baudrate=BAUD_TANGO,
        timeout=0.15,
        write_timeout=1.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )
    conexion.dtr = False
    conexion.rts = False
    conexion.port = puerto
    conexion.open()

    # Algunos adaptadores USB-UART reinician el ESP32 al abrir el puerto.
    time.sleep(BOOT_WAIT_SECONDS)
    try:
        conexion.reset_input_buffer()
        conexion.reset_output_buffer()
    except Exception:
        pass

    st.session_state.tango_serial = conexion
    st.session_state.tango_port = puerto


def leer_json_enmarcado(inicio, fin, timeout_s=SERIAL_TIMEOUT_SECONDS):
    conexion = st.session_state.get("tango_serial")
    if conexion is None or not getattr(conexion, "is_open", False):
        raise RuntimeError("TANGO no está conectado.")

    limite = time.monotonic() + timeout_s
    capturando = False
    lineas = []

    while time.monotonic() < limite:
        bruto = conexion.readline()
        if not bruto:
            time.sleep(0.02)
            continue

        linea = bruto.decode("utf-8", errors="replace").strip()
        if not linea:
            continue

        if linea.startswith("###TANGO_ERROR###"):
            detalle = linea.replace("###TANGO_ERROR###", "", 1)
            try:
                mensaje = json.loads(detalle).get("message", "Error informado por TANGO.")
            except Exception:
                mensaje = detalle or "Error informado por TANGO."
            raise RuntimeError(mensaje)

        if linea == inicio:
            capturando = True
            lineas = []
            continue

        if linea == fin and capturando:
            payload = "".join(lineas)
            return json.loads(payload)

        if capturando:
            lineas.append(linea)

    raise TimeoutError("TANGO no respondió dentro del tiempo esperado.")


def enviar_comando_tango(comando, inicio, fin):
    conexion = st.session_state.get("tango_serial")
    if conexion is None or not getattr(conexion, "is_open", False):
        raise RuntimeError("TANGO no está conectado.")

    try:
        conexion.reset_input_buffer()
    except Exception:
        pass

    conexion.write((comando + "\n").encode("ascii"))
    conexion.flush()
    return leer_json_enmarcado(inicio, fin)


def solicitar_lista_registros():
    respuesta = enviar_comando_tango(
        "@LIST",
        "###TANGO_LIST_BEGIN###",
        "###TANGO_LIST_END###",
    )
    registros = respuesta.get("records", [])
    if not isinstance(registros, list):
        raise ValueError("La lista de registros recibida no tiene el formato esperado.")
    return registros


def solicitar_registro(record_id):
    return enviar_comando_tango(
        f"@GET:{record_id}",
        "###TANGO_RECORD_BEGIN###",
        "###TANGO_RECORD_END###",
    )



def listar_puertos():
    if list_ports is None:
        return []
    try:
        return sorted(list_ports.comports(), key=lambda p: p.device)
    except Exception:
        return []


# ============================================================
# ESTADO DE LA SESIÓN (PERSISTENCIA DE DATOS)
# ============================================================
if "paso" not in st.session_state:
    st.session_state.paso = "registro"

if "datos_clinicos" not in st.session_state:
    st.session_state.datos_clinicos = {
        "nombre_paciente": "",
        "hc_paciente": "",
        "extremidad": "",
        "hora_retiro": "",
        "notas_medico": ""
    }

if "tango_serial" not in st.session_state:
    st.session_state.tango_serial = None

if "tango_port" not in st.session_state:
    st.session_state.tango_port = None

if "registros_disponibles" not in st.session_state:
    st.session_state.registros_disponibles = []

if "registro_tango" not in st.session_state:
    st.session_state.registro_tango = None

if "registro_tango_raw" not in st.session_state:
    st.session_state.registro_tango_raw = None

if "estado_tango" not in st.session_state:
    st.session_state.estado_tango = "Desconectado."


# ============================================================
# ESTILOS CSS PERSONALIZADOS
# ============================================================
st.markdown("""
<style>
    /* FONDO DE LA APLICACIÓN */
    .stApp {
        background-color: #ECEFF1 !important;
        color: #102A43 !important;
    }
    
    /* CONTENEDOR DEL FORMULARIO */
    div[data-testid="stForm"] {
        background-color: #FFFFFF !important;
        border: 1px solid #CFD8DC !important;
        border-radius: 8px !important;
        padding: 24px !important;
        margin-bottom: 20px !important;
        box-shadow: 0 1px 3px rgba(0,0,0,0.05) !important;
    }

    /* ESTILOS PARA INPUTS Y TEXTAREA (MÁXIMA LEGIBILIDAD) */
    .stTextInput input, .stTextArea textarea {
        background-color: #FFFFFF !important;
        color: #0E2439 !important;
        border: 1px solid #B0BEC5 !important;
        border-radius: 6px !important;
        font-size: 14px !important;
    }

    .stTextArea textarea {
        color: #0E2439 !important;
        font-family: inherit !important;
        line-height: 1.5 !important;
    }

    .stTextInput input:focus, .stTextArea textarea:focus {
        border-color: #0E2439 !important;
        box-shadow: 0 0 0 1px #0E2439 !important;
    }

    /* TARJETAS DEL INFORME */
    .report-card {
        background-color: #FFFFFF !important;
        border: 1px solid #CFD8DC !important;
        border-radius: 8px !important;
        padding: 20px !important;
        margin-bottom: 20px !important;
        box-shadow: 0 1px 3px rgba(0,0,0,0.05) !important;
    }

    .card-title {
        margin-top: 0 !important;
        margin-bottom: 18px !important;
        color: #0E2439 !important;
        font-size: 18px !important;
        font-weight: 700 !important;
        border-bottom: 2px solid #E2E8F0 !important;
        padding-bottom: 8px !important;
    }

    /* ETIQUETAS DE FORMULARIO */
    div[data-testid="stForm"] label, div[data-testid="stWidgetLabel"] label {
        color: #0E2439 !important;
        font-weight: 700 !important;
        font-size: 14px !important;
    }

    /* ENCABEZADO TANGO */
    .header-box {
        background-color: #0E2439 !important;
        color: #FFFFFF !important;
        padding: 16px 20px !important;
        border-radius: 8px !important;
        margin-bottom: 20px !important;
        display: flex !important;
        align-items: center !important;
        justify-content: space-between !important;
    }
    
    .header-brand-container {
        display: flex !important;
        align-items: center !important;
        gap: 16px !important;
    }

    .header-logo-img {
        height: 50px !important;
        width: auto !important;
        object-fit: contain !important;
    }

    .brand-title {
        font-size: 24px !important;
        font-weight: 800 !important;
        color: #FFFFFF !important;
        letter-spacing: 1px !important;
        margin: 0 !important;
        line-height: 1.1 !important;
    }

    .brand-sub {
        font-size: 13px !important;
        color: #B0BEC5 !important;
        margin-top: 2px !important;
    }

    .header-meta {
        text-align: right !important;
        font-size: 13px !important;
        color: #ECEFF1 !important;
        line-height: 1.4 !important;
    }

    /* TARJETAS DE MÉTRICAS */
    .metric-card {
        background-color: #FFFFFF !important;
        border: 1px solid #CFD8DC !important;
        border-radius: 8px !important;
        padding: 18px !important;
        text-align: center !important;
        box-shadow: 0 1px 3px rgba(0,0,0,0.05) !important;
    }

    .metric-label {
        font-size: 12.5px !important;
        font-weight: 700 !important;
        color: #475569 !important;
        text-transform: uppercase !important;
        letter-spacing: 0.5px !important;
    }

    .metric-value {
        font-size: 34px !important;
        font-weight: 800 !important;
        color: #0E2439 !important;
        font-family: monospace !important;
        margin-top: 4px !important;
    }

    /* FILAS DE INFORMACIÓN */
    .data-row {
        display: flex !important;
        justify-content: space-between !important;
        padding: 10px 0 !important;
        border-bottom: 1px solid #F1F5F9 !important;
        font-size: 14px !important;
    }

    .data-row:last-child {
        border-bottom: none !important;
    }

    .data-key {
        color: #334155 !important;
        font-weight: 600 !important;
    }

    .data-val {
        color: #0F172A !important;
        font-weight: 700 !important;
        font-family: monospace !important;
    }

    /* CONTENEDOR DE NOTAS DEL MÉDICO (INFORME) */
    .notes-box {
        background-color: #F8FAFC !important;
        border: 1px solid #CBD5E1 !important;
        border-radius: 6px !important;
        padding: 12px 16px !important;
        color: #0F172A !important;
        font-size: 14px !important;
        line-height: 1.5 !important;
        white-space: pre-wrap !important;
        word-wrap: break-word !important;
        margin-top: 6px !important;
    }

    /* HISTORIAL DE LOGS Y ALERTAS MULTICOLOR */
    .log-box {
        background-color: #F8FAFC !important;
        border: 1px solid #E2E8F0 !important;
        border-left: 5px solid #475569 !important;
        padding: 12px 16px !important;
        border-radius: 4px !important;
        margin-bottom: 10px !important;
    }

    .log-box:last-child {
        margin-bottom: 0 !important;
    }

    /* ALERTA ROJA (CRÍTICA / PÉRDIDA DE OCLUSIÓN) */
    .log-box.alarm-critical {
        background-color: #FEF2F2 !important;
        border-color: #FCA5A5 !important;
        border-left: 5px solid #DC2626 !important;
    }
    .log-box.alarm-critical .log-title { color: #991B1B !important; }
    .log-box.alarm-critical .log-time { color: #7F1D1D !important; }
    .log-box.alarm-critical .log-detail { color: #991B1B !important; }

    /* ALERTA ÁMBAR (ADVERTENCIA / PRESIÓN FUERA DE RANGO) */
    .log-box.alarm-warning {
        background-color: #FFFBEB !important;
        border-color: #FDE68A !important;
        border-left: 5px solid #D97706 !important;
    }
    .log-box.alarm-warning .log-title { color: #92400E !important; }
    .log-box.alarm-warning .log-time { color: #78350F !important; }
    .log-box.alarm-warning .log-detail { color: #92400E !important; }

    /* ALERTA AZUL (INFORMATIVA / TIEMPO EXCEDIDO) */
    .log-box.alarm-info {
        background-color: #EFF6FF !important;
        border-color: #BFDBFE !important;
        border-left: 5px solid #2563EB !important;
    }
    .log-box.alarm-info .log-title { color: #1E40AF !important; }
    .log-box.alarm-info .log-time { color: #1E3A8A !important; }
    .log-box.alarm-info .log-detail { color: #1E40AF !important; }

    /* ALERTA NEUTRA / DEFAULT */
    .log-box.alarm-default {
        background-color: #F5F3FF !important;
        border-color: #DDD6FE !important;
        border-left: 5px solid #7C3AED !important;
    }
    .log-box.alarm-default .log-title { color: #5B21B6 !important; }
    .log-box.alarm-default .log-time { color: #4C1D95 !important; }
    .log-box.alarm-default .log-detail { color: #5B21B6 !important; }

    .log-time {
        font-family: monospace !important;
        font-weight: 700 !important;
    }

    .log-title {
        font-weight: 700 !important;
        font-size: 14px !important;
    }

    .log-detail {
        font-size: 13px !important;
        margin-top: 2px !important;
    }

    /* BOTONES */
    .stButton > button, div[data-testid="stForm"] button, .stDownloadButton > button {
        background-color: #0E2439 !important;
        color: #FFFFFF !important;
        font-weight: 700 !important;
        border: none !important;
        border-radius: 6px !important;
        padding: 10px 20px !important;
        width: 100% !important;
        transition: all 0.2s ease !important;
        margin-bottom: 8px !important;
    }

    .stButton > button:hover, div[data-testid="stForm"] button:hover, .stDownloadButton > button:hover {
        background-color: #1E3A8A !important;
        color: #FFFFFF !important;
    }

    @media print {
        header, footer, .stButton, .stDownloadButton, [data-testid="stSidebar"] {
            display: none !important;
        }
        .stApp {
            background-color: #FFFFFF !important;
        }
    }
</style>
""", unsafe_allow_html=True)



# ============================================================
# LOGO
# ============================================================
LOGO_B64 = get_image_base64("logo.png")
img_tag = f'<img src="data:image/png;base64,{LOGO_B64}" class="header-logo-img" />' if LOGO_B64 else ''

# ============================================================
# VENTANA 1: CONECTAR TANGO Y SELECCIONAR REGISTRO
# ============================================================
if st.session_state.paso == "registro":

    header_html = (
        f'<div class="header-box"><div class="header-brand-container">{img_tag}<div>'
        f'<div class="brand-title">TANGO</div>'
        f'<div class="brand-sub">Torniquete Automático Neumático</div>'
        f'</div></div></div>'
    )
    st.markdown(header_html, unsafe_allow_html=True)

    st.markdown(
        '<div class="report-card">'
        '<div class="card-title">Cargar registro del dispositivo</div>'
        '<div style="color:#475569; font-size:14px; line-height:1.5;">'
        'Conectá TANGO por USB, seleccioná el puerto del dispositivo y elegí el registro que querés consultar.'
        '</div></div>',
        unsafe_allow_html=True,
    )

    if serial is None:
        st.error("No está instalado pyserial. Ejecutá `uv pip install pyserial` y reiniciá Streamlit.")
    else:
        puertos = listar_puertos()
        etiquetas = [f"{p.device} — {p.description}" for p in puertos]
        mapa_puertos = {f"{p.device} — {p.description}": p.device for p in puertos}

        if puerto_abierto() and st.session_state.tango_port not in mapa_puertos.values():
            etiqueta_actual = f"{st.session_state.tango_port} — conectado"
            etiquetas.insert(0, etiqueta_actual)
            mapa_puertos[etiqueta_actual] = st.session_state.tango_port

        if etiquetas:
            indice = 0
            if st.session_state.tango_port:
                for i, etiqueta in enumerate(etiquetas):
                    if mapa_puertos.get(etiqueta) == st.session_state.tango_port:
                        indice = i
                        break

            puerto_elegido = st.selectbox(
                "Puerto USB",
                etiquetas,
                index=indice,
                disabled=puerto_abierto(),
            )
        else:
            puerto_elegido = None
            st.info("No se detectaron puertos serie. Conectá el ESP32 por USB y recargá la página.")

        col_conectar, col_actualizar = st.columns(2)

        with col_conectar:
            if not puerto_abierto():
                if st.button("Conectar TANGO", disabled=puerto_elegido is None):
                    try:
                        abrir_tango(mapa_puertos[puerto_elegido])
                        st.session_state.registros_disponibles = solicitar_lista_registros()
                        st.session_state.estado_tango = (
                            f"Conectado a {st.session_state.tango_port} · {BAUD_TANGO} baudios."
                        )
                        st.rerun()
                    except Exception as exc:
                        cerrar_tango()
                        st.session_state.estado_tango = f"No se pudo conectar: {exc}"
                        st.error(
                            "No se pudo abrir TANGO. Cerrá el Monitor Serie, el Plotter Serie "
                            "y cualquier otro programa que esté usando el mismo puerto."
                        )
            else:
                if st.button("Desconectar TANGO"):
                    cerrar_tango()
                    st.session_state.estado_tango = "Desconectado."
                    st.rerun()

        with col_actualizar:
            if st.button("Actualizar registros", disabled=not puerto_abierto()):
                try:
                    st.session_state.registros_disponibles = solicitar_lista_registros()
                    st.session_state.estado_tango = "Lista de registros actualizada."
                    st.rerun()
                except Exception as exc:
                    st.error(f"No se pudo leer la lista de registros: {exc}")

        if puerto_abierto():
            st.success(st.session_state.estado_tango)
        else:
            st.caption(st.session_state.estado_tango)

        if puerto_abierto():
            registros = st.session_state.registros_disponibles

            if registros:
                opciones = []
                por_etiqueta = {}
                for item in registros:
                    rid = item.get("record_id", "")
                    label = item.get("label", "")
                    etiqueta = f"{rid} — {label}" if label else rid
                    opciones.append(etiqueta)
                    por_etiqueta[etiqueta] = rid

                registro_elegido = st.selectbox(
                    "Registro disponible en TANGO",
                    opciones,
                    key="selector_registro_tango",
                )

                if st.button("Cargar registro", type="primary"):
                    try:
                        rid = por_etiqueta[registro_elegido]
                        raw = solicitar_registro(rid)
                        nuevo_registro = adaptar_registro_tango(raw)

                        # Los datos clínicos se conservan mientras se siga trabajando
                        # con el mismo registro. Si se carga otro procedimiento, se
                        # limpian para evitar asociar datos de un paciente anterior.
                        registro_anterior = st.session_state.get("registro_tango")
                        id_anterior = (
                            registro_anterior.get("record_id")
                            if isinstance(registro_anterior, dict)
                            else None
                        )
                        id_nuevo = nuevo_registro.get("record_id")

                        if id_anterior != id_nuevo:
                            st.session_state.datos_clinicos = {
                                "nombre_paciente": "",
                                "hc_paciente": "",
                                "extremidad": "",
                                "hora_retiro": "",
                                "notas_medico": ""
                            }

                        st.session_state.registro_tango_raw = raw
                        st.session_state.registro_tango = nuevo_registro
                        st.session_state.estado_tango = f"Registro {rid} cargado correctamente."
                        st.session_state.paso = "formulario"
                        st.rerun()
                    except Exception as exc:
                        st.error(f"No se pudo cargar el registro: {exc}")
            else:
                st.info("TANGO no informó registros disponibles.")

# ============================================================
# VENTANA 2: DATOS DEL PACIENTE
# ============================================================
elif st.session_state.paso == "formulario":

    data = st.session_state.registro_tango
    if data is None:
        st.session_state.paso = "registro"
        st.rerun()

    record_id = data.get("record_id", "—")

    header_html_form = (
        f'<div class="header-box"><div class="header-brand-container">{img_tag}<div>'
        f'<div class="brand-title">TANGO</div>'
        f'<div class="brand-sub">Datos asociados al registro {html.escape(str(record_id))}</div>'
        f'</div></div></div>'
    )
    st.markdown(header_html_form, unsafe_allow_html=True)

    if st.button("← Elegir otro registro"):
        st.session_state.paso = "registro"
        st.rerun()

    st.success(f"Registro cargado: {record_id}")

    datos = st.session_state.datos_clinicos

    with st.form("form_inicial"):
        st.markdown(
            '<div class="card-title">Información del Paciente y Evaluación Clínica</div>',
            unsafe_allow_html=True,
        )

        col1, col2 = st.columns(2)
        with col1:
            nombre_paciente = st.text_input(
                "Nombre del Paciente * (Solo letras)",
                value=datos.get("nombre_paciente", ""),
            )
            hc_paciente = st.text_input(
                "ID / Historia Clínica",
                value=datos.get("hc_paciente", ""),
            )

        with col2:
            extremidad = st.text_input(
                "Extremidad / Ubicación",
                value=datos.get("extremidad", ""),
                placeholder="Ej: Miembro inferior derecho",
            )
            hora_retiro = st.text_input(
                "Hora de retiro (HH:MM)",
                value=datos.get("hora_retiro", ""),
                placeholder="HH:MM",
            )

        notas_medico = st.text_area(
            "Notas del médico",
            value=datos.get("notas_medico", ""),
            placeholder="Escriba aquí observaciones clínicas adicionales...",
            height=120,
        )

        st.write("")
        submit = st.form_submit_button("Guardar e Ir al Informe")

    if submit:
        errores = []

        if not validar_solo_letras(nombre_paciente):
            errores.append(
                "El nombre del paciente solo debe contener letras y espacios (sin números o símbolos)."
            )

        if not validar_formato_hora(hora_retiro):
            errores.append(
                "La hora de retiro debe tener el formato HH:MM (por ejemplo, 14:15)."
            )

        if errores:
            for err in errores:
                st.error(err)
        else:
            st.session_state.datos_clinicos = {
                "nombre_paciente": nombre_paciente.strip(),
                "hc_paciente": hc_paciente.strip(),
                "extremidad": extremidad.strip(),
                "hora_retiro": hora_retiro.strip(),
                "notas_medico": notas_medico.strip(),
            }
            st.session_state.paso = "informe"
            st.rerun()

# ============================================================
# VENTANA 3: INFORME CLÍNICO GENERADO
# ============================================================
elif st.session_state.paso == "informe":

    if st.button("← Volver a editar datos del paciente"):
        st.session_state.paso = "formulario"
        st.rerun()

    data = st.session_state.registro_tango

    if data is None:
        st.warning("No hay un registro TANGO cargado.")
        if st.button("Volver a seleccionar registro"):
            st.session_state.paso = "registro"
            st.rerun()
        st.stop()

    clinicos = st.session_state.datos_clinicos
    sesion = data.get("sesion", {})
    metricas = data.get("metricas", {})
    reajustes = data.get("reajustes", [])
    alertas = data.get("alertas", [])

    nom_p = clinicos.get("nombre_paciente") or "N/A"
    hc_p = clinicos.get("hc_paciente") or "N/A"
    record_id = data.get("record_id", "—")
    duracion = sesion.get("duracion", "—")

    header_html_info = (
        f'<div class="header-box">'
        f'<div class="header-brand-container">{img_tag}<div>'
        f'<div class="brand-title">TANGO</div>'
        f'<div class="brand-sub">Informe de sesión neumática</div></div></div>'
        f'<div class="header-meta"><strong>Paciente:</strong> {html.escape(nom_p)}<br>'
        f'<strong>ID / HC:</strong> {html.escape(hc_p)}<br>'
        f'<strong>Registro:</strong> {html.escape(record_id)}<br>'
        f'<strong>Duración:</strong> {html.escape(str(duracion))}</div></div>'
    )
    st.markdown(header_html_info, unsafe_allow_html=True)

    # Métricas principales
    c1, c2 = st.columns(2)
    with c1:
        presion_oclusion = metricas.get("presion_alcanzar_oclusion_mmHg", "—")
        st.markdown(
            f'<div class="metric-card"><div class="metric-label">Presión al alcanzar oclusión</div>'
            f'<div class="metric-value">{presion_oclusion} '
            f'<span style="font-size: 16px;">mmHg</span></div></div>',
            unsafe_allow_html=True,
        )
    with c2:
        st.markdown(
            f'<div class="metric-card"><div class="metric-label">Tiempo total de oclusión</div>'
            f'<div class="metric-value">{metricas.get("tiempo_oclusion_total", "—")}</div></div>',
            unsafe_allow_html=True,
        )

    st.write("")

    # Métricas de la sesión
    m_html = (
        '<div class="report-card"><div class="card-title">Métricas de la Sesión</div>'
        f'<div class="data-row"><span class="data-key">Presión máxima registrada</span>'
        f'<span class="data-val">{metricas.get("presion_maxima_mmHg", "—")} mmHg</span></div>'
        f'<div class="data-row"><span class="data-key">Tiempo hasta oclusión inicial</span>'
        f'<span class="data-val">{metricas.get("tiempo_hasta_oclusion_inicial", "—")}</span></div>'
        f'<div class="data-row"><span class="data-key">Reajustes automáticos</span>'
        f'<span class="data-val">{metricas.get("reajustes_automaticos", 0)}</span></div>'
        '</div>'
    )
    st.markdown(m_html, unsafe_allow_html=True)

    # Registro del equipo clínico
    texto_notas = clinicos.get("notas_medico") or "Sin observaciones clínicas registradas."
    reg_html = f"""
    <div class="report-card">
        <div class="card-title">Registro del Equipo Clínico</div>
        <div class="data-row">
            <span class="data-key">Extremidad / Ubicación</span>
            <span class="data-val">{html.escape(clinicos.get("extremidad") or "—")}</span>
        </div>
        <div class="data-row">
            <span class="data-key">Hora de retiro</span>
            <span class="data-val">{html.escape(clinicos.get("hora_retiro") or "—")}</span>
        </div>
        <div style="margin-top: 14px;">
            <div class="data-key" style="margin-bottom: 4px;">Notas del médico:</div>
            <div class="notes-box">{html.escape(texto_notas)}</div>
        </div>
    </div>
    """
    st.markdown(reg_html, unsafe_allow_html=True)

    # Historial de reajustes
    reajustes_content = ""
    if reajustes:
        for i, r in enumerate(reajustes, start=1):
            reajustes_content += (
                f'<div class="log-box"><span class="log-time">t = {html.escape(str(r.get("momento", "—")))}</span> — '
                f'<span class="log-title">Reajuste automático #{i}</span>'
                f'<div class="log-detail">Presión: <b>{r.get("de_mmHg", "—")} mmHg → '
                f'{r.get("a_mmHg", "—")} mmHg</b> | Duración: '
                f'{html.escape(str(r.get("duracion", "—")))}</div></div>'
            )
    else:
        reajustes_content = (
            "<p style='color: #64748B; font-style: italic; margin:0;'>"
            "Sin reajustes automáticos durante la sesión.</p>"
        )

    st.markdown(
        f'<div class="report-card"><div class="card-title">Historial de Reajustes</div>'
        f'{reajustes_content}</div>',
        unsafe_allow_html=True,
    )

    # Historial de alertas
    alertas_content = ""
    if alertas:
        for a in alertas:
            clase_alerta = obtener_clase_alerta(a.get("tipo", "aviso"))
            prefijo = "ALARMA" if str(a.get("tipo", "")).lower() == "alarma" else "AVISO"
            alertas_content += (
                f'<div class="log-box {clase_alerta}">'
                f'<span class="log-time">t = {html.escape(str(a.get("momento", "—")))}</span> — '
                f'<span class="log-title">{prefijo} · {html.escape(str(a.get("texto", "")))}</span>'
                f'<div class="log-detail">{html.escape(str(a.get("detalle", "")))}</div></div>'
            )
    else:
        alertas_content = (
            "<p style='color: #64748B; font-style: italic; margin:0;'>"
            "Sin alertas registradas durante la sesión.</p>"
        )

    st.markdown(
        f'<div class="report-card"><div class="card-title">Historial de Alertas del Sistema</div>'
        f'{alertas_content}</div>',
        unsafe_allow_html=True,
    )

    # Descargas
    resumen_txt = (
        f"Informe TANGO\n"
        f"Registro: {record_id}\n"
        f"Paciente: {nom_p}\n"
        f"ID / HC: {hc_p}\n"
        f"Presión al alcanzar oclusión: {metricas.get('presion_alcanzar_oclusion_mmHg', '—')} mmHg\n"
        f"Presión máxima: {metricas.get('presion_maxima_mmHg', '—')} mmHg\n"
        f"Tiempo total de oclusión: {metricas.get('tiempo_oclusion_total', '—')}\n"
        f"Tiempo hasta oclusión inicial: {metricas.get('tiempo_hasta_oclusion_inicial', '—')}\n"
        f"Reajustes: {metricas.get('reajustes_automaticos', 0)}\n"
        f"Notas: {texto_notas}\n"
    )
    st.download_button(
        label="Descargar resumen TXT",
        data=resumen_txt,
        file_name=f"Informe_{record_id}_{hc_p}.txt",
        mime="text/plain",
        key="btn_txt",
    )

    salida_csv = io.StringIO()
    writer = csv.writer(salida_csv)
    writer.writerow([
        "Registro",
        "Paciente",
        "HC",
        "Presion_Oclusion_mmHg",
        "Presion_Maxima_mmHg",
        "Tiempo_Total_Oclusion",
        "Tiempo_Hasta_Oclusion",
        "Reajustes",
        "Notas",
    ])
    writer.writerow([
        record_id,
        nom_p,
        hc_p,
        metricas.get("presion_alcanzar_oclusion_mmHg"),
        metricas.get("presion_maxima_mmHg"),
        metricas.get("tiempo_oclusion_total"),
        metricas.get("tiempo_hasta_oclusion_inicial"),
        metricas.get("reajustes_automaticos", 0),
        texto_notas,
    ])

    st.download_button(
        label="Descargar CSV",
        data=salida_csv.getvalue(),
        file_name=f"Reporte_{record_id}_{hc_p}.csv",
        mime="text/csv",
        key="btn_csv",
    )

    st.caption(
        "Documento generado localmente a partir del registro seleccionado del dispositivo. "
        "Los datos clínicos se incorporan manualmente en esta interfaz."
    )
 
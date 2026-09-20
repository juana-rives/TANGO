import streamlit as st
import base64
import re
from pathlib import Path

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

def obtener_clase_alerta(texto_alerta):
    """Asigna una clase CSS según la naturaleza/gravedad de la alarma."""
    texto_upper = texto_alerta.upper()
    if "PÉRDIDA" in texto_upper or "CRÍTICA" in texto_upper:
        return "alarm-critical"  # Rojo
    elif "RANGO" in texto_upper or "PRESIÓN" in texto_upper:
        return "alarm-warning"   # Ámbar / Naranja
    elif "TIEMPO" in texto_upper or "EXCEDIDO" in texto_upper:
        return "alarm-info"      # Azul
    return "alarm-default"       # Púrpura / Neutro

# ============================================================
# ESTADO DE LA SESIÓN (PERSISTENCIA DE DATOS)
# ============================================================
if "paso" not in st.session_state:
    st.session_state.paso = "formulario"

if "datos_clinicos" not in st.session_state:
    st.session_state.datos_clinicos = {
        "nombre_paciente": "Juan Pérez",
        "hc_paciente": "HC-884920",
        "extremidad": "Miembro inferior derecho",
        "hora_retiro": "14:15",
        "notas_medico": ""
    }

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
# DATOS DE SESIÓN Y OPCIONES DE ALERTAS
# ============================================================
REPORTE_DEMO = {
    "paciente": {
        "nombre": "Juan Pérez",
        "id_paciente": "HC-884920",
        "edad": "45 años"
    },
    "sesion": {
        "fecha": "2026-09-16",
        "hora_inicio": "14:02:10",
        "hora_fin": "14:09:28"
    },
    "metricas": {
        "presion_promedio_mmHg": 218,
        "tiempo_oclusion_total": "00:07:18",
        "presion_alcanzar_oclusion_mmHg": 232,
        "presion_maxima_mmHg": 242,
        "tiempo_hasta_oclusion_inicial": "00:00:24",
        "reajustes_automaticos": 2
    },
    "reajustes": [
        {"hora": "14:03:42", "duracion": "00:00:06", "de_mmHg": 224, "a_mmHg": 232},
        {"hora": "14:07:12", "duracion": "00:00:05", "de_mmHg": 204, "a_mmHg": 218}
    ],
    "alertas": [
        {
            "tipo": "alarma",
            "texto": "ALARMA · PÉRDIDA DE OCLUSIÓN",
            "hora": "14:07:18",
            "detalle": "Presión cayó por debajo del umbral de oclusión. Reajuste iniciado."
        },
        {
            "tipo": "alarma",
            "texto": "ALARMA · PRESIÓN FUERA DE RANGO",
            "hora": "14:05:10",
            "detalle": "Desviación detectada respecto al setpoint configurado."
        },
        {
            "tipo": "alarma",
            "texto": "ALARMA · TIEMPO EXCEDIDO DE OCLUSIÓN",
            "hora": "14:08:00",
            "detalle": "Aviso preventivo por tiempo de torniquete acumulado."
        }
    ]
}

# Imagen base64 local si existe logo.png
LOGO_B64 = get_image_base64("logo.png")
img_tag = f'<img src="data:image/png;base64,{LOGO_B64}" class="header-logo-img" />' if LOGO_B64 else ''

# ============================================================
# VENTANA 1: FORMULARIO DE INGRESO DE DATOS
# ============================================================
if st.session_state.paso == "formulario":
    
    header_html_form = f'<div class="header-box"><div class="header-brand-container">{img_tag}<div><div class="brand-title">TANGO</div><div class="brand-sub">Torniquete Automático Neumático</div></div></div></div>'
    st.markdown(header_html_form, unsafe_allow_html=True)

    datos = st.session_state.datos_clinicos

    with st.form("form_inicial"):
        st.markdown('<div class="card-title">Información del Paciente y Evaluación Clínica</div>', unsafe_allow_html=True)
        
        col1, col2 = st.columns(2)
        with col1:
            nombre_paciente = st.text_input(
                "Nombre del Paciente * (Solo letras)", 
                value=datos.get("nombre_paciente", "")
            )
            hc_paciente = st.text_input(
                "ID / Historia Clínica", 
                value=datos.get("hc_paciente", "")
            )
        
        with col2:
            extremidad = st.text_input(
                "Extremidad / Ubicación", 
                value=datos.get("extremidad", ""),
                placeholder="Ej: Miembro inferior derecho"
            )
            hora_retiro = st.text_input(
                "Hora de retiro (HH:MM)", 
                value=datos.get("hora_retiro", ""),
                placeholder="HH:MM"
            )

        notas_medico = st.text_area(
            "Notas del médico", 
            value=datos.get("notas_medico", ""),
            placeholder="Escriba aquí observaciones clínicas adicionales...", 
            height=120
        )

        st.write("")
        submit = st.form_submit_button("Guardar e Ir al Informe")

    if submit:
        errores = []
        
        if not validar_solo_letras(nombre_paciente):
            errores.append("El nombre del paciente solo debe contener letras y espacios (sin números o símbolos).")
            
        if not validar_formato_hora(hora_retiro):
            errores.append("La hora de retiro debe tener el formato HH:MM (por ejemplo, 14:15).")

        if errores:
            for err in errores:
                st.error(err)
        else:
            st.session_state.datos_clinicos = {
                "nombre_paciente": nombre_paciente.strip(),
                "hc_paciente": hc_paciente.strip(),
                "extremidad": extremidad.strip(),
                "hora_retiro": hora_retiro.strip(),
                "notas_medico": notas_medico.strip()
            }
            st.session_state.paso = "informe"
            st.rerun()

# ============================================================
# VENTANA 2: INFORME CLÍNICO GENERADO
# ============================================================
elif st.session_state.paso == "informe":
    
    if st.button("← Volver a editar datos"):
        st.session_state.paso = "formulario"
        st.rerun()

    data = REPORTE_DEMO
    clinicos = st.session_state.datos_clinicos

    paciente = data.get("paciente", {})
    sesion = data.get("sesion", {})
    metricas = data.get("metricas", {})
    reajustes = data.get("reajustes", [])
    
    # Filtrar estrictamente cualquier alerta relacionada con batería
    alertas_filtradas = [
        a for a in data.get("alertas", []) 
        if "BATER" not in a.get("texto", "").upper() and "BATER" not in a.get("detalle", "").upper()
    ]

    nom_p = clinicos.get('nombre_paciente') or paciente.get('nombre', 'N/A')
    hc_p = clinicos.get('hc_paciente') or paciente.get('id_paciente', 'N/A')
    f_p = f"{sesion.get('fecha', '—')} ({sesion.get('hora_inicio', '—')} – {sesion.get('hora_fin', '—')})"

    header_html_info = f'<div class="header-box"><div class="header-brand-container">{img_tag}<div><div class="brand-title">TANGO</div><div class="brand-sub">Informe de sesión neumática</div></div></div><div class="header-meta"><strong>Paciente:</strong> {nom_p}<br><strong>ID / HC:</strong> {hc_p}<br><strong>Fecha:</strong> {f_p}</div></div>'
    st.markdown(header_html_info, unsafe_allow_html=True)

    # Métricas Principales
    c1, c2 = st.columns(2)
    with c1:
        st.markdown(f'<div class="metric-card"><div class="metric-label">Presión Promedio</div><div class="metric-value">{metricas.get("presion_promedio_mmHg", "—")} <span style="font-size: 16px;">mmHg</span></div></div>', unsafe_allow_html=True)
    with c2:
        st.markdown(f'<div class="metric-card"><div class="metric-label">Tiempo Total Oclusión</div><div class="metric-value">{metricas.get("tiempo_oclusion_total", "—")}</div></div>', unsafe_allow_html=True)

    st.write("")

    # Métricas de la Sesión
    m_html = f'<div class="report-card"><div class="card-title">Métricas de la Sesión</div><div class="data-row"><span class="data-key">Presión al alcanzar oclusión</span><span class="data-val">{metricas.get("presion_alcanzar_oclusion_mmHg", "—")} mmHg</span></div><div class="data-row"><span class="data-key">Presión máxima registrada</span><span class="data-val">{metricas.get("presion_maxima_mmHg", "—")} mmHg</span></div><div class="data-row"><span class="data-key">Tiempo hasta oclusión inicial</span><span class="data-val">{metricas.get("tiempo_hasta_oclusion_inicial", "—")}</span></div><div class="data-row"><span class="data-key">Reajustes automáticos</span><span class="data-val">{metricas.get("reajustes_automaticos", 0)}</span></div></div>'
    st.markdown(m_html, unsafe_allow_html=True)

    # Registro del Equipo Clínico (Notas estructuradas para máxima legibilidad)
    texto_notas = clinicos.get("notas_medico") or "Sin observaciones clínicas registradas."
    reg_html = f'''
    <div class="report-card">
        <div class="card-title">Registro del Equipo Clínico</div>
        <div class="data-row">
            <span class="data-key">Extremidad / Ubicación</span>
            <span class="data-val">{clinicos.get("extremidad") or "—"}</span>
        </div>
        <div class="data-row">
            <span class="data-key">Hora de retiro</span>
            <span class="data-val">{clinicos.get("hora_retiro") or "—"}</span>
        </div>
        <div style="margin-top: 14px;">
            <div class="data-key" style="margin-bottom: 4px;">Notas del médico:</div>
            <div class="notes-box">{texto_notas}</div>
        </div>
    </div>
    '''
    st.markdown(reg_html, unsafe_allow_html=True)

    # Historial de Reajustes
    reajustes_content = ""
    if reajustes:
        for i, r in enumerate(reajustes, start=1):
            reajustes_content += f'<div class="log-box"><span class="log-time">{r.get("hora")}</span> — <span class="log-title">Reajuste automático #{i}</span><div class="log-detail">Presión: <b>{r.get("de_mmHg")} mmHg → {r.get("a_mmHg")} mmHg</b> | Duración: {r.get("duracion")}</div></div>'
    else:
        reajustes_content = "<p style='color: #64748B; font-style: italic; margin:0;'>Sin reajustes automáticos durante la sesión.</p>"

    st.markdown(f'<div class="report-card"><div class="card-title">Historial de Reajustes</div>{reajustes_content}</div>', unsafe_allow_html=True)

    # Historial de Alertas (Visualización multicolor según tipo de alarma)
    alertas_content = ""
    if alertas_filtradas:
        for a in alertas_filtradas:
            clase_alerta = obtener_clase_alerta(a.get("texto", ""))
            alertas_content += f'<div class="log-box {clase_alerta}"><span class="log-time">{a.get("hora")}</span> — <span class="log-title">{a.get("texto")}</span><div class="log-detail">{a.get("detalle", "")}</div></div>'
    else:
        alertas_content = "<p style='color: #64748B; font-style: italic; margin:0;'>Sin alertas de oclusión o presión registradas durante la sesión.</p>"

    st.markdown(f'<div class="report-card"><div class="card-title">Historial de Alertas del Sistema</div>{alertas_content}</div>', unsafe_allow_html=True)

    # Botones de descarga apilados
    st.download_button(
        label="Descargar PDF",
        data=f"Informe Clínico TANGO - Paciente: {nom_p} ({hc_p})\nNotas: {texto_notas}",
        file_name=f"Informe_{hc_p}.pdf",
        mime="application/pdf",
        key="btn_pdf"
    )

    csv_datos = f"Paciente,HC,Presion_Promedio_mmHg,Tiempo_Total,Notas\n\"{nom_p}\",\"{hc_p}\",{metricas.get('presion_promedio_mmHg')},{metricas.get('tiempo_oclusion_total')},\"{texto_notas}\""
    st.download_button(
        label="Descargar CSV",
        data=csv_datos,
        file_name=f"Reporte_{hc_p}.csv",
        mime="text/csv",
        key="btn_csv"
    )

    st.caption("Documento generado localmente a partir de los datos exportados por el dispositivo. No requiere conexión.")
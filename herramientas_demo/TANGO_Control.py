#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Control por botones y teclado para TANGO_HMI.ino.

En Windows, desde la terminal de VS Code, en la carpeta de este archivo:
    py -m pip install pyserial       # solo la primera vez, si falta
    py TANGO_Control.py

1. Cargar TANGO_HMI.ino en el ESP32 desde Arduino IDE.
2. Cerrar el Monitor Serie y el Plotter Serie de Arduino, y el Python anterior.
3. Conectar el ESP32 por USB, elegir su puerto y pulsar Conectar.
4. Mirar la TFT y usar las flechas/Confirmar de esta ventana o del teclado.

La ventana es un mando para la TFT; no reproduce la pantalla del dispositivo.
No descarga registros ni sustituye al futuro visor de datos.

Protocolo del sketch: 115200 baudios, 8N1, un byte sin salto de línea.
U/D/L/R/E = arriba/abajo/izquierda/derecha/confirmar.
0/1/2/9 = cuatro atajos de demostración. Los 21 escenarios se eligen en la TFT:
Menú > Sistema > Escenarios demo. Allí: arriba/abajo eligen fila,
izquierda/derecha cambian página y Confirmar carga el escenario.

Dependencias: Python 3.9+ con tkinter y pyserial. Sin archivos auxiliares.
API de referencia: https://pyserial.readthedocs.io/en/latest/pyserial_api.html
"""

import codecs
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


# Estos valores coinciden con el sketch entregado. No editar el .ino.
BAUD = 115200
BOOT_WAIT_SECONDS = 2.0
POLL_MS = 40
COMMANDS = {
    "U": "Arriba", "D": "Abajo", "L": "Izquierda", "R": "Derecha",
    "E": "Confirmar", "0": "Funcionamiento normal", "1": "Batería baja",
    "2": "Pérdida de oclusión + batería baja", "9": "Procedimiento terminado",
    "S": "Simulación completa de procedimiento",
}
KEYS = {
    "Up": "U", "Down": "D", "Left": "L", "Right": "R",
    "Return": "E", "KP_Enter": "E",
    "0": "0", "1": "1", "2": "2", "9": "9",
    "KP_0": "0", "KP_1": "1", "KP_2": "2", "KP_9": "9",
    "s": "S", "S": "S",
}


class SerialLink:
    """USB en un hilo separado: la ventana sigue respondiendo al conectar.

    El hilo nunca toca widgets de tkinter. Envía avisos a la ventana por cola.
    No se reenvían comandos al reconectar y no se inicia ninguna acción solo.
    """

    def __init__(self, port, serial_factory=None):
        self.port = port
        self.events = queue.Queue(maxsize=500)
        self.commands = queue.Queue(maxsize=32)
        self.stop_event = threading.Event()
        self.ready = threading.Event()
        self.serial_factory = serial_factory or serial.Serial
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()

    def send(self, command):
        if command not in COMMANDS or not self.ready.is_set() or self.stop_event.is_set():
            return False
        try:
            self.commands.put_nowait(command)
        except queue.Full:
            return False
        return True

    def stop(self):
        self.ready.clear()
        self.stop_event.set()

    def _notify(self, kind, value=""):
        # Un log muy largo no puede acumular memoria indefinidamente.
        if self.events.full():
            try:
                self.events.get_nowait()
            except queue.Empty:
                pass
        try:
            self.events.put_nowait((kind, value))
        except queue.Full:
            pass

    def _run(self):
        connection = None
        decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        try:
            connection = self.serial_factory(
                port=None, baudrate=BAUD, timeout=0, write_timeout=0.2,
                xonxoff=False, rtscts=False, dsrdtr=False,
            )
            # No se pulsan RESET/BOOT. Se solicitan líneas de control inactivas
            # antes de abrir. Algunos adaptadores pueden reiniciar igualmente
            # el ESP32 al abrir el puerto; por eso hay una espera inicial.
            connection.dtr = False
            connection.rts = False
            connection.port = self.port
            connection.open()
            self._notify("waiting", self.port)
            if self.stop_event.wait(BOOT_WAIT_SECONDS):
                return
            self.ready.set()
            self._notify("ready", self.port)

            while not self.stop_event.is_set():
                for _ in range(32):
                    try:
                        command = self.commands.get_nowait()
                    except queue.Empty:
                        break
                    if self.stop_event.is_set():
                        break
                    # Exactamente un byte ASCII: no se agrega CR ni LF.
                    payload = command.encode("ascii")
                    if connection.write(payload) != len(payload):
                        raise OSError("El puerto no pudo enviar el comando completo.")
                    self._notify("sent", command)

                available = connection.in_waiting
                if available:
                    chunk = connection.read(min(available, 2048))
                    text = decoder.decode(chunk)
                    # Los mensajes se muestran como texto; nunca se interpretan
                    # como instrucciones ni se usan para inferir el estado clínico.
                    text = "".join(c for c in text if c in "\n\r\t" or c.isprintable())
                    if text:
                        self._notify("received", text)
                self.stop_event.wait(0.02)
        except Exception as exc:
            if not self.stop_event.is_set():
                self._notify("error", str(exc))
        finally:
            self.ready.clear()
            if connection is not None:
                try:
                    connection.close()
                except Exception:
                    pass
            self._notify("closed")


class TangoControl:
    def __init__(self, root):
        self.root = root
        self.link = None
        self.closing = False
        self.port_map = {}
        self.control_buttons = []
        self.held_keys = set()
        self.release_jobs = {}
        self.poll_job = None
        self.log_characters = 0
        self.log_visible = False

        root.title("TANGO · Control de la pantalla")
        root.configure(bg="#F4F7F9")
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.resizable(True, True)
        self._style()
        self._build()
        self._set_controls(False)
        self.refresh_ports()

        # Se interceptan las teclas ANTES de las acciones de los botones para
        # evitar que Enter mande dos comandos o active un botón de la PC.
        root.bind_class("TangoNav", "<KeyPress>", self.key_press)
        root.bind_class("TangoNav", "<KeyRelease>", self.key_release)
        self._install_key_bindings(root)
        root.bind("<FocusOut>", self.clear_keys, add="+")
        root.update_idletasks()
        root.minsize(600, root.winfo_reqheight())
        self.poll_job = root.after(POLL_MS, self.poll)

    def _style(self):
        style = ttk.Style(self.root)
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure(".", font=("Segoe UI", 10), foreground="#1F364E")
        style.configure("TFrame", background="#F4F7F9")
        style.configure("TLabel", background="#F4F7F9")
        style.configure("TLabelframe", background="#F4F7F9")
        style.configure("TLabelframe.Label", background="#F4F7F9", font=("Segoe UI", 10, "bold"))
        style.configure("TButton", padding=(10, 6), background="#FFFFFF")
        style.map("TButton", background=[("active", "#DEE8F1"), ("disabled", "#E4E8EB")])
        style.configure("Nav.TButton", font=("Segoe UI", 12, "bold"), padding=(8, 10))
        style.configure("Title.TLabel", font=("Segoe UI", 20, "bold"))
        style.configure("Muted.TLabel", foreground="#586470")

    def _build(self):
        body = ttk.Frame(self.root, padding=16)
        body.pack(fill="both", expand=True)
        ttk.Label(body, text="TANGO", style="Title.TLabel").pack(anchor="w")
        ttk.Label(body, text="Controlá la pantalla del dispositivo con botones o teclado.",
                  style="Muted.TLabel").pack(anchor="w", pady=(0, 12))

        connection = ttk.LabelFrame(body, text="Conexión USB", padding=10)
        connection.pack(fill="x")
        connection.columnconfigure(0, weight=1)
        self.port_choice = tk.StringVar()
        self.port_combo = ttk.Combobox(connection, textvariable=self.port_choice,
                                       state="readonly", width=39)
        self.port_combo.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.refresh_button = ttk.Button(connection, text="Actualizar", command=self.refresh_ports)
        self.refresh_button.grid(row=0, column=1)
        self.connect_button = ttk.Button(connection, text="Conectar", command=self.toggle_connection)
        self.connect_button.grid(row=0, column=2, padx=(8, 0))
        self.status = tk.StringVar(value="Elegí el puerto del ESP32.")
        self.status_label = ttk.Label(connection, textvariable=self.status, wraplength=560)
        self.status_label.grid(row=1, column=0, columnspan=3, sticky="w", pady=(8, 0))

        navigation = ttk.LabelFrame(body, text="Navegación", padding=10)
        navigation.pack(fill="x", pady=(12, 0))
        for column in range(3):
            navigation.columnconfigure(column, weight=1, uniform="nav")
        for label, command, row, column in [
            ("↑  Arriba", "U", 0, 1),
            ("←  Izquierda", "L", 1, 0),
            ("Confirmar", "E", 1, 1),
            ("Derecha  →", "R", 1, 2),
            ("↓  Abajo", "D", 2, 1),
        ]:
            button = ttk.Button(navigation, text=label, style="Nav.TButton",
                                command=lambda c=command: self.send(c))
            button.grid(row=row, column=column, sticky="ew", padx=4, pady=3)
            self.control_buttons.append(button)
        ttk.Label(navigation, text="Teclado: flechas para moverte · Enter para confirmar.",
                  style="Muted.TLabel").grid(row=3, column=0, columnspan=3, pady=(8, 0))

        scenarios = ttk.LabelFrame(body, text="Atajos de escenarios", padding=10)
        scenarios.pack(fill="x", pady=(12, 0))
        for col in range(2):
            scenarios.columnconfigure(col, weight=1, uniform="quick")
        for i, (label, command) in enumerate([
            ("0 · Normal", "0"), ("1 · Batería baja", "1"),
            ("2 · Pérdida + batería baja", "2"), ("9 · Terminado / registros", "9"),
            ("S · Simulación completa", "S"),
        ]):
            button = ttk.Button(scenarios, text=label, command=lambda c=command: self.send(c))
            button.grid(row=i // 2, column=i % 2, sticky="ew", padx=3, pady=3)
            self.control_buttons.append(button)
        ttk.Label(body, text="Los 21 escenarios y la simulación completa están en la TFT:\nMenú → Sistema → Escenarios demo.",
                  justify="left", wraplength=570).pack(anchor="w", pady=(10, 4))
        ttk.Label(body, text="Candado en la TFT: Enter bloquea y otro Enter desbloquea.",
                  style="Muted.TLabel", wraplength=570).pack(anchor="w")

        bottom = ttk.Frame(body)
        bottom.pack(fill="x", pady=(10, 0))
        self.last_action = tk.StringVar(value="Todavía no se enviaron comandos.")
        ttk.Label(bottom, textvariable=self.last_action, style="Muted.TLabel").pack(side="left")
        self.log_toggle = ttk.Button(bottom, text="Ver mensajes", command=self.toggle_log)
        self.log_toggle.pack(side="right")
        self.log_frame = ttk.Frame(body)
        self.log = tk.Text(self.log_frame, height=6, width=72, wrap="word", state="disabled",
                           font=("Consolas", 9), bg="#FFFFFF", fg="#1F364E", relief="solid", bd=1)
        scroll = ttk.Scrollbar(self.log_frame, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

    def _install_key_bindings(self, widget):
        # En el selector de puerto y el log se conserva el teclado habitual.
        # En Conectar/Actualizar/Ver mensajes, Enter sigue activando ese botón.
        exceptions = (self.port_combo, self.log, self.connect_button, self.refresh_button, self.log_toggle)
        if widget not in exceptions:
            widget.bindtags(("TangoNav",) + widget.bindtags())
        for child in widget.winfo_children():
            self._install_key_bindings(child)

    def _set_controls(self, enabled):
        for button in self.control_buttons:
            button.configure(state="normal" if enabled else "disabled")

    def refresh_ports(self):
        if self.link is not None:
            return
        old_device = self.port_map.get(self.port_choice.get())
        try:
            ports = sorted(list_ports.comports(), key=lambda p: p.device)
        except Exception as exc:
            self.status.set("No se pudieron buscar los puertos USB.")
            self.append_log(str(exc) + "\n")
            return
        self.port_map = {f"{p.device} — {p.description}": p.device for p in ports}
        labels = list(self.port_map)
        self.port_combo.configure(values=labels)
        if not labels:
            self.port_choice.set("")
            self.connect_button.configure(state="disabled")
            self.status.set("No hay puertos. Conectá el ESP32 por USB y pulsá Actualizar.")
            return
        chosen = next((label for label in labels if self.port_map[label] == old_device), labels[0])
        self.port_choice.set(chosen)
        self.connect_button.configure(state="normal")
        self.status.set("Elegí el mismo puerto que usás en Arduino IDE y pulsá Conectar.")

    def toggle_connection(self):
        if self.link is not None:
            self.link.stop()
            self._set_controls(False)
            self.connect_button.configure(state="disabled")
            self.status.set("Desconectando…")
            self.clear_keys()
            return
        port = self.port_map.get(self.port_choice.get())
        if not port:
            self.refresh_ports()
            return
        self._set_controls(False)
        self.port_combo.configure(state="disabled")
        self.refresh_button.configure(state="disabled")
        self.connect_button.configure(text="Desconectar")
        self.status_label.configure(foreground="#586470")
        self.status.set(f"Conectando a {port}…")
        self.link = SerialLink(port)
        self.link.start()

    def send(self, command):
        if self.link is None or not self.link.send(command):
            return
        # El estado de la TFT lo decide el ESP32. Aquí no se simula una
        # confirmación de navegación, audio, bloqueo ni acción clínica.

    def key_press(self, event):
        command = KEYS.get(event.keysym)
        if command is None or event.state & 0xC:  # Ctrl/Alt: atajos del sistema
            return None
        pending = self.release_jobs.pop(event.keysym, None)
        if pending:
            self.root.after_cancel(pending)
        if event.keysym not in self.held_keys:
            self.held_keys.add(event.keysym)
            self.send(command)
        return "break"

    def key_release(self, event):
        if event.keysym not in KEYS:
            return None
        pending = self.release_jobs.pop(event.keysym, None)
        if pending:
            self.root.after_cancel(pending)
        # after_idle absorbe el release/press sintético del auto-repeat de X11.
        self.release_jobs[event.keysym] = self.root.after_idle(self._release_key, event.keysym)
        return "break"

    def _release_key(self, keysym):
        self.release_jobs.pop(keysym, None)
        self.held_keys.discard(keysym)

    def clear_keys(self, _event=None):
        for job in self.release_jobs.values():
            self.root.after_cancel(job)
        self.release_jobs.clear()
        self.held_keys.clear()

    def poll(self):
        if self.closing:
            return
        link = self.link
        if link is not None:
            for _ in range(80):
                try:
                    kind, value = link.events.get_nowait()
                except queue.Empty:
                    break
                if kind == "waiting":
                    self.status.set(f"USB abierto en {value}. Esperando al ESP32…")
                elif kind == "ready" and not link.stop_event.is_set():
                    self._set_controls(True)
                    self.status.set(f"Conectado a {value} · 115200 baudios. Mirá la TFT.")
                    self.status_label.configure(foreground="#19804B")
                    self.root.focus_set()
                elif kind == "sent":
                    self.last_action.set(f"Enviado: {COMMANDS[value]}")
                elif kind == "received":
                    self.append_log(value)
                elif kind == "error":
                    self._set_controls(False)
                    self.status.set("No se pudo mantener la conexión USB.")
                    self.status_label.configure(foreground="#9B1C1C")
                    self.append_log("USB: " + value + "\n")
                    messagebox.showerror("Conexión con TANGO",
                        "Revisá el cable y el puerto seleccionado.\n\n"
                        "Cerrá el Monitor Serie y el Plotter Serie de Arduino, "
                        "y cualquier otro Python que esté usando ese puerto.\n\n"
                        + value, parent=self.root)
                elif kind == "closed":
                    self._set_controls(False)
                    self.link = None
                    self.port_combo.configure(state="readonly")
                    self.refresh_button.configure(state="normal")
                    self.connect_button.configure(text="Conectar", state="normal")
                    self.status.set("Desconectado. Podés elegir un puerto y volver a conectar.")
                    self.status_label.configure(foreground="#586470")
                    self.clear_keys()
                    break
        self.poll_job = self.root.after(POLL_MS, self.poll)

    def append_log(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text.replace("\r\n", "\n"))
        self.log_characters += len(text)
        if self.log_characters > 16000:
            self.log.delete("1.0", "end-12000c")
            self.log_characters = 12000
        self.log.see("end")
        self.log.configure(state="disabled")

    def toggle_log(self):
        self.log_visible = not self.log_visible
        if self.log_visible:
            self.log_frame.pack(fill="both", expand=True, pady=(8, 0))
            self.log_toggle.configure(text="Ocultar mensajes")
        else:
            self.log_frame.pack_forget()
            self.log_toggle.configure(text="Ver mensajes")

    def close(self):
        self.closing = True
        if self.poll_job:
            self.root.after_cancel(self.poll_job)
        self.clear_keys()
        if self.link is not None:
            self.link.stop()
            self.link.thread.join(timeout=0.4)
        self.root.destroy()


def main():
    root = tk.Tk()
    if serial is None or not hasattr(serial, "Serial"):
        root.withdraw()
        messagebox.showerror("Falta pyserial",
            "Abrí la terminal de VS Code y ejecutá:\n\n"
            "py -m pip install pyserial\n\n"
            "Después volvé a abrir TANGO_Control.py.", parent=root)
        root.destroy()
        return
    TangoControl(root)
    root.mainloop()


if __name__ == "__main__":
    main()

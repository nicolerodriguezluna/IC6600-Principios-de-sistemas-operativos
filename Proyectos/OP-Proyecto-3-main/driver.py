import serial
import serial.tools.list_ports
import time
import screen_brightness_control as sbc
import os
import threading
import tkinter as tk
from PIL import Image, ImageTk
import queue
import subprocess

BAUD = 9600
queue_osd = queue.Queue()

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ICON_DIR = os.path.join(BASE_DIR, "icons")

ICON_VOL_0 = os.path.join(ICON_DIR, "vol_0.png")
ICON_VOL_1 = os.path.join(ICON_DIR, "vol_1.png")
ICON_VOL_2 = os.path.join(ICON_DIR, "vol_2.png")
ICON_VOL_3 = os.path.join(ICON_DIR, "vol_3.png")
ICON_BRILLO = os.path.join(ICON_DIR, "brillo.png")

def icono_volumen_por_nivel(valor):
    if valor <= 0:
        return ICON_VOL_0
    elif valor <= 30:
        return ICON_VOL_1
    elif valor <= 70:
        return ICON_VOL_2
    else:
        return ICON_VOL_3


# ============================================================
# OSD
# ============================================================

class OSDManager:
    def __init__(self):
        self.root = tk.Tk()
        self.root.withdraw()

        self.bg_color = "#F6E8FF"      
        self.border_color = "#C59BFF"  
        self.shadow_color = "#000000"
        self.bar_bg = "#ECD9FF"
        self.bar_fill = "#C08CFF"
        self.text_color = "#663B99"

        self.transparent = "#00FFFF"

        self.osd_window = None
        self.canvas = None
        self.img_ref = None
        self.close_job = None
        self.opacity = 0.0

        self.root.after(30, self.osd_loop)

    def crear_ventana(self):
        width = 385
        height = 100

        self.osd_window = tk.Toplevel(self.root)
        self.osd_window.withdraw()
        self.osd_window.overrideredirect(True)
        self.osd_window.attributes("-topmost", True)

        self.osd_window.config(bg=self.transparent)
        self.osd_window.attributes("-transparentcolor", self.transparent)

        x = self.root.winfo_screenwidth()//2 - width//2
        y = self.root.winfo_screenheight()//2 + 210
        self.osd_window.geometry(f"{width}x{height}+{x}+{y}")

        self.canvas = tk.Canvas(
            self.osd_window,
            bg=self.transparent,
            highlightthickness=0,
            bd=0
        )
        self.canvas.place(relwidth=1, relheight=1)

    def draw_round_rect(self, x1, y1, x2, y2, r, fill, outline, width):
        points = [
            x1+r, y1,
            x2-r, y1,
            x2, y1,
            x2, y1+r,
            x2, y2-r,
            x2, y2,
            x2-r, y2,
            x1+r, y2,
            x1, y2,
            x1, y2-r,
            x1, y1+r,
            x1, y1
        ]
        self.canvas.create_polygon(points, smooth=True, fill=fill, outline=outline, width=width)

    def fade_in(self):
        if self.opacity < 1.0:
            self.opacity += 0.12
            self.osd_window.attributes("-alpha", self.opacity)
            self.osd_window.after(20, self.fade_in)

    def fade_out(self):
        if self.opacity > 0:
            self.opacity -= 0.12
            self.osd_window.attributes("-alpha", self.opacity)
            self.osd_window.after(20, self.fade_out)
        else:
            self.osd_window.destroy()
            self.osd_window = None

    def actualizar_osd(self, tipo, valor):

        if self.osd_window is None or not self.osd_window.winfo_exists():
            self.crear_ventana()
            self.opacity = 0
            self.osd_window.attributes("-alpha", 0)
            self.osd_window.deiconify()
            self.fade_in()

        self.canvas.delete("all")
        self.canvas.create_rectangle(10, 10, 365, 110, fill="#D9C6F9", outline="#B77CFF", width=3)

        self.draw_round_rect(
            0, 0, 380, 100,
            r=25,
            fill=self.bg_color,
            outline=self.border_color,
            width=3
        )

        # Icono
        if tipo == "V":
            icon_path = icono_volumen_por_nivel(valor)
        else:
            icon_path = ICON_BRILLO

        img_raw = Image.open(icon_path).resize((50, 50), Image.LANCZOS)
        self.img_ref = ImageTk.PhotoImage(img_raw)
        self.canvas.create_image(55, 47, image=self.img_ref)

        # Barra
        bar_x = 95
        bar_y = 38
        bar_w = 190
        bar_h = 20

        self.draw_round_rect(bar_x, bar_y, bar_x+bar_w, bar_y+bar_h, 10, self.bar_bg, self.bar_bg, 1)

        fill = int(max(0, min(100, valor)) / 100 * bar_w)
        self.draw_round_rect(bar_x, bar_y, bar_x+fill, bar_y+bar_h, 10, self.bar_fill, self.bar_fill, 1)

        self.canvas.create_text(bar_x+bar_w+40, bar_y+bar_h/2,
                                text=f"{valor}%",
                                fill=self.text_color,
                                font=("Segoe UI", 15, "bold"))

        if self.close_job:
            self.osd_window.after_cancel(self.close_job)

        self.close_job = self.osd_window.after(1300, self.fade_out)

    def osd_loop(self):
        try:
            while True:
                tipo, valor = queue_osd.get_nowait()
                self.actualizar_osd(tipo, valor)
        except queue.Empty:
            pass

        self.root.after(30, self.osd_loop)

    def run(self):
        self.root.mainloop()


# ============================================================
# DETECTAR ARDUINO
# ============================================================

def detectar_arduino():
    print("Buscando Arduino...")

    while True:
        for port in [p.device for p in serial.tools.list_ports.comports()]:
            try:
                ser = serial.Serial(port, BAUD, timeout=1)
                time.sleep(1)
                ser.write(b"\n")

                for _ in range(4):
                    line = ser.readline().decode().strip()
                    if line.startswith("V:") or line.startswith("B:") or line == "ARDUINO_INICIADO":
                        print(f"Arduino encontrado en {port}")
                        return ser
                ser.close()
            except:
                pass

        print("Reintentando...")
        time.sleep(2)


# ============================================================
# VOLUMEN Y BRILLO
# ============================================================
def set_volume(v):
    try:
        subprocess.run(
            ["nircmd.exe", "setsysvolume", str(int(v * 655.35))],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            shell=False
        )
    except:
        pass



brillo_objetivo = 0
brillo_actual = 0


def brillo_worker():
    global brillo_actual, brillo_objetivo
    while True:
        if brillo_actual != brillo_objetivo:
            try:
                sbc.set_brightness(brillo_objetivo)
                brillo_actual = brillo_objetivo
            except:
                pass
        time.sleep(0.005)


def driver_thread(osd):
    global brillo_objetivo

    ser = detectar_arduino()
    time.sleep(2)
    volumen = 0

    while True:
        if ser.in_waiting:
            line = ser.readline().decode().strip()
            if not line:
                continue

            print("Recibido:", line)

            if line.startswith("V:"):
                try:
                    volumen = int(line.split(":")[1])
                except:
                    continue
                volumen = max(0, min(100, volumen))
                set_volume(volumen)
                queue_osd.put(("V", volumen))

            elif line.startswith("B:"):
                try:
                    brillo_objetivo = int(line.split(":")[1])
                except:
                    continue
                brillo_objetivo = max(0, min(100, brillo_objetivo))
                queue_osd.put(("B", brillo_objetivo))

        time.sleep(0.0001)


# ============================================================
# EJECUCIÓN DRIVER
# ============================================================
if __name__ == "__main__":
    threading.Thread(target=brillo_worker, daemon=True).start()

    osd = OSDManager()

    threading.Thread(target=driver_thread, args=(osd,), daemon=True).start()

    osd.run()

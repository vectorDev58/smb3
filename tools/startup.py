"""Native first-run ROM picker and preparation indicator; no ROM data is bundled."""
from configparser import ConfigParser, Error as ConfigError
from contextlib import contextmanager
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


def spanish_ui(config_path):
    settings = ConfigParser(interpolation=None, strict=False)
    try:
        settings.read(config_path, encoding="utf-8")
        return settings.get("Game", "Language", fallback="en") == "es"
    except (ConfigError, UnicodeError):
        return False


def choose_rom(spanish=False):
    prompt = ("Elige tu ROM original (USA, PRG0)" if spanish else "Select your original ROM (USA, PRG0)")
    title = f"SMB3 Remastered - {prompt}"
    filter_name = "ROM de NES o ZIP" if spanish else "NES ROM or ZIP"
    if sys.platform == "win32":
        script = ("[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new();"
                  "Add-Type -AssemblyName System.Windows.Forms;"
                  "$picker=[System.Windows.Forms.OpenFileDialog]::new();"
                  f"$picker.Title='{title}';"
                  f"$picker.Filter='{filter_name}|*.nes;*.zip';"
                  "if($picker.ShowDialog() -eq 'OK'){[Console]::WriteLine($picker.FileName)}")
        command = ["powershell", "-NoProfile", "-STA", "-Command", script]
    elif sys.platform == "darwin":
        command = ["osascript", "-e", f'POSIX path of (choose file with prompt "{title}")']
    elif shutil.which("zenity"):
        command = ["zenity", "--file-selection", "--title", title, "--file-filter", f"{filter_name} | *.nes *.zip"]
    else:
        raise OSError("Instala Zenity o usa tools/smb3.py import-rom <archivo>."
                      if spanish else "Install Zenity or use tools/smb3.py import-rom <file>.")
    result = subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace")
    if result.returncode == 1 or (sys.platform == "darwin" and "-128" in result.stderr):
        return None
    if result.returncode:
        raise OSError(result.stderr.strip() or ("No se pudo abrir el selector de ROM."
                                                if spanish else "Could not open the ROM picker."))
    selected = result.stdout.rstrip("\r\n")
    return Path(selected) if selected else None


def show_error(message):
    if sys.platform == "win32":
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, str(message), "SMB3 Remastered", 0x10)
    elif sys.platform == "darwin":
        script = 'on run argv\ndisplay alert "SMB3 Remastered" message (item 1 of argv)\nend run'
        subprocess.run(["osascript", "-e", script, str(message)], capture_output=True)
    elif shutil.which("zenity"):
        subprocess.run(["zenity", "--error", "--no-markup", "--title", "SMB3 Remastered", "--text", str(message)], capture_output=True)
    else:
        print(str(message), file=sys.stderr)


def request_rom(destination, importer, chooser=None, error=None, *, spanish=False):
    error = error or show_error
    while True:
        selected = chooser() if chooser is not None else choose_rom(spanish)
        if selected is None:
            return False
        try:
            importer(selected, destination)
            return True
        except (OSError, ValueError, zipfile.BadZipFile) as failure:
            error(str(failure))


@contextmanager
def preparing(spanish=False):
    progress = None
    if sys.platform.startswith("linux") and shutil.which("zenity") and (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        message = ("Preparando el juego por primera vez. Puede tardar unos minutos." if spanish else
                   "Preparing the game for the first time. This can take a few minutes.")
        progress = subprocess.Popen(["zenity", "--progress", "--pulsate", "--auto-close", "--no-cancel",
            "--title", "SMB3 Remastered", "--text", message],
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        yield
    finally:
        if progress:
            try:
                progress.stdin.write(b"100\n")
                progress.stdin.close()
            except OSError:
                pass
            try:
                progress.wait(timeout=3)
            except subprocess.TimeoutExpired:
                progress.terminate()
                progress.wait(timeout=3)

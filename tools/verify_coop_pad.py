#!/usr/bin/env python3
"""Cooperativo con el jugador 1 en teclado y un solo mando: el mando mueve al jugador 2.
Abre una ventana oculta con un mando virtual; requiere la ROM PRG0."""
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

from smb3 import ROOT, executable, read_rom
from verify_widescreen import coop_state


def main():
    rom, binary = ROOT / "private/baserom.nes", executable()
    read_rom(rom)
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        # The window writes saves and diagnostics beside the executable, so run a copy.
        # shortcut: copies only the executable and links its data (Linux build); copy the DLLs to run it on Windows.
        game = work / binary.name
        shutil.copy(binary, game)
        for name in ("assets", "resource_packs"):
            if (binary.parent / name).exists():
                (work / name).symlink_to(binary.parent / name)
        config = work / "config.ini"
        config.write_text("[Launcher]\nSkipLauncher = 1\n[Input]\nPlayer1Source = 1\nPlayer2Source = 2\n")
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        # Only the virtual controller (no vendor or product id) counts, not the pads on this computer.
        environment = dict(os.environ, SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT="0x0000/0x0000")
        with open(work / "output.txt", "w") as log:
            process = subprocess.Popen([str(game), str(rom), "--hidden", "--tcp", str(port), "--config", str(config),
                                        "--skip-selection", "--coop", "--partner", "luigi", "--no-save"],
                                       cwd=work, env=environment, stdout=log, stderr=subprocess.STDOUT)
        try:
            guid = check(work, port)
        finally:
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        saved = config.read_text()
        assert "Player2Device = \n" in saved, "config.ini guardó el mando que el cooperativo dio al jugador 2"
        assert f"Player1Device = {guid}\n" in saved, "config.ini no guardó el mando elegido en Controles"
        assert "Player2Source = 1\n" in saved and "[Keyboard.Player2]\na = Z\n" in saved, \
            "El jugador 2 pasado al teclado no recibió teclas"
    print("Cooperativo con teclado y un mando: el mando mueve al jugador 2, no al 1, en el mapa y en 1-1; "
          "elegido en Controles, pasa al jugador 1 y queda en config.ini; el jugador 2 en teclado recibe teclas.")


def check(work, port):
    for _ in range(100):
        try:
            connection = socket.create_connection(("127.0.0.1", port))
            break
        except OSError:
            time.sleep(0.1)
    else:
        raise RuntimeError(f"El juego no abrió el puerto TCP; consulta {work / 'output.txt'}")
    reader = connection.makefile("r")

    def command(**line):
        connection.sendall((json.dumps(line) + "\n").encode())
        return json.loads(reader.readline())

    def wait(frames):
        end, deadline = command(cmd="ping")["frame"] + frames, time.time() + 30
        while command(cmd="ping")["frame"] < end:
            assert time.time() < deadline, "El juego dejó de avanzar"
            time.sleep(0.02)

    def ram(address):
        return int(command(cmd="read_ram", addr=address, len=1)["hex"], 16)

    def on_map():
        return ram(0x101) != 0x20 and not ram(0x70a) and ram(0x729) == 0x0d

    def in_level():  # As practice_in_level() in src/remaster.cpp
        return ram(0x101) != 0x20 and 0 < ram(0x70a) < 15 and ram(0xeb)

    def partner():
        path = work / "coop.cycstate"
        command(cmd="save_state", path=str(path))
        wait(2)
        return coop_state(path)

    def lead_x():
        return ram(0x75) * 256 + ram(0x90)

    def hold(device, name, frames):
        command(cmd=device, **({"buttons": name} if device == "pad" else {"name": name}), frames=frames)

    wait(30)
    for _ in range(40):  # START through the title to the map
        if on_map():
            break
        hold("key", "Return", 3)
        wait(50)
    assert on_map(), "No se llegó al mapa del mundo 1"
    wait(120)
    hold("pad", "dpdown", 1)  # Attaches the virtual controller.
    wait(30)
    assert len(command(cmd="state")["pads"]) == 1, "Debe haber un solo mando"
    map_x = ram(0x79)  # World_Map_X
    hold("pad", "dpright", 40)
    wait(80)
    assert ram(0x79) == map_x, "El mando movió al jugador 1 en el mapa"
    hold("key", "Right", 10)
    wait(80)
    assert ram(0x79) != map_x, "El teclado no movió al jugador 1 en el mapa"
    hold("key", "Up", 10)  # 1-1 is above the first junction.
    wait(80)
    hold("key", "Z", 5)
    for _ in range(60):
        if in_level():
            break
        wait(10)
    assert in_level(), "No se entró en 1-1"
    # Right after the start: some seconds later the first Goomba reaches the idle players.
    wait(30)
    before, start = partner(), lead_x()
    hold("pad", "dpright", 60)
    wait(40)
    during = partner()
    assert during["buttons"] & 1 and not during["partner_is_p1"], "El compañero no recibe la derecha del mando"
    assert during["present"] and during["x"] > before["x"] + 8, "El mando no movió al compañero"
    assert lead_x() == start, "El mando movió al jugador 1 en 1-1"
    wait(30)
    hold("key", "Right", 20)
    wait(20)
    assert lead_x() > start + 8, "El teclado no movió al jugador 1 en 1-1"

    def press(key):
        hold("key", key, 2)
        time.sleep(0.3)  # Menus pause the game: wait in time, not frames.

    # Pause, Settings, Controls (fifth row), the controller row: controller 1 for player 1. Then
    # the player 2 tab and its input, from controller to none to keyboard. Backspace goes back to
    # Settings, the pause menu and the game.
    for key in ("Escape", "Down", "Return", "Down", "Down", "Down", "Down", "Return", "Up", "Right",
                "Up", "Up", "Right", "Down", "Right", "Right", "Backspace", "Backspace", "Backspace"):
        press(key)
    start = lead_x()
    hold("pad", "dpleft", 30)  # Away from the Goomba coming from the right.
    wait(20)
    assert not partner()["buttons"] and lead_x() < start - 4, "El mando elegido en Controles no pasó al jugador 1"
    guid = command(cmd="state")["pads"][0]["guid"]
    command(cmd="quit")
    return guid


if __name__ == "__main__":
    main()

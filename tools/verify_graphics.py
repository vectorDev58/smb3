#!/usr/bin/env python3
"""Pruebas locales de presentacion; requieren ROM y una compilacion, no Pillow."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

from smb3 import ROOT, executable, read_rom
from verify import cpu_ram_from_log
from verify_widescreen import captures


def main():
    rom, binary = ROOT / "private/baserom.nes", executable()
    read_rom(rom)
    out = ROOT / "verification/graphics"
    out.mkdir(parents=True, exist_ok=True)
    report = out / "result.json"
    report.unlink(missing_ok=True)
    base_characters = ("mario", "luigi")
    skins = binary.parent / "assets/remaster/Players"
    optional_characters = ("toad", "toadette") if all(
        (skins / name / f"{form}.png").is_file()
        for name in ("Toad", "Toadette") for form in ("Small", "Big", "Fire")
    ) else ()
    characters = base_characters + optional_characters
    route = out / "idle.txt"
    route.write_text("12 START\n13 NONE\n68 START\n69 NONE\n328 RIGHT\n329 NONE\n"
                     "345 UP\n346 NONE\n361 A\n362 NONE\n", encoding="ascii")

    def run(name, options, program=binary):
        result = subprocess.run([str(program), str(rom), "--no-save", "--present-size", "1280x720", *map(str, options)],
                                cwd=program.parent, capture_output=True, timeout=120)
        (out / (name + ".log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"Fallo en {name}; consulta {out / (name + '.log')}")

    def size(path):
        data = path.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", path
        return struct.unpack_from(">II", data, 16)

    state = out / "level.cycstate"
    run("classic", ["--frames", 450, "--input", route, "--graphics", "classic",
                    "--hash-out", out / "classic.hash", "--wav-out", out / "classic.wav",
                    "--screenshot", out / "classic-native.png", "--present-out", out / "classic.png",
                    "--save-state", f"449:{state}"])
    run("native-remastered", ["--frames", 450, "--input", route, "--aspect", "native",
        "--hash-out", out / "native-remastered.hash", "--graphics", "remastered"])
    run("native-classic", ["--frames", 450, "--input", route, "--aspect", "native",
        "--hash-out", out / "native-classic.hash", "--graphics", "classic"])
    assert (out / "native-remastered.hash").read_bytes() == (out / "native-classic.hash").read_bytes(), "El modo nativo altero la simulacion"
    baseline = None
    pictures = []
    for character in characters:
        run(character, ["--frames", 450, "--input", route, "--character", character,
                        "--graphics", "remastered", "--aspect", "16:9",
                        "--hash-out", out / f"{character}.hash", "--wav-out", out / f"{character}.wav",
                        "--screenshot", out / f"{character}-native.png", "--present-out", out / f"{character}.png"])
        current = (out / f"{character}.hash").read_bytes()
        if baseline is None: baseline = current
        assert current == baseline, f"Estado alterado por {character}"
        assert (out / f"{character}.wav").read_bytes() == (out / "mario.wav").read_bytes(), "Audio alterado por el personaje"
        assert (out / f"{character}-native.png").read_bytes() == (out / "mario-native.png").read_bytes(), "PPU alterada por el personaje"
        assert size(out / f"{character}.png") == (426, 240)
        pictures.append(hashlib.sha256((out / f"{character}.png").read_bytes()).hexdigest())
    assert len(set(pictures)) == len(characters), "Cada personaje debe verse distinto"
    print(f"{len(characters)} personajes: misma simulación panorámica, vídeo NES y audio.", flush=True)

    map_route = out / "map.txt"
    map_route.write_text("12 START\n13 NONE\n68 START\n69 NONE\n", encoding="ascii")
    map_state = out / "map.cycstate"
    run("map-classic", ["--frames", 450, "--input", map_route, "--graphics", "classic",
        "--hash-out", out / "map-classic.hash", "--wav-out", out / "map-classic.wav",
        "--screenshot", out / "map-classic-native.png", "--save-state", f"449:{map_state}"])
    pictures = []
    for character in characters:
        name = "map-" + character
        run(name, ["--frames", 450, "--input", map_route, "--character", character,
            "--hash-out", out / f"{name}.hash", "--wav-out", out / f"{name}.wav",
            "--screenshot", out / f"{name}-native.png", "--present-out", out / f"{name}.png"])
        for suffix in (".hash", ".wav", "-native.png"):
            assert (out / f"{name}{suffix}").read_bytes() == (out / f"map-classic{suffix}").read_bytes(), "El personaje altero el mapa original"
        assert size(out / f"{name}.png") == (426, 240)
        pictures.append(hashlib.sha256((out / f"{name}.png").read_bytes()).hexdigest())
    assert len(set(pictures)) == len(characters), "El mapa debe mostrar el personaje elegido"
    inventory_route = out / "inventory.txt"
    inventory_route.write_text("450 B\n451 NONE\n500 B\n501 NONE\n", encoding="ascii")
    for name, frames, opened in (("inventory-open", 490, True), ("inventory-closed", 540, False)):
        memory = out / f"{name}.bin"
        run(name, ["--load-state", map_state, "--input", inventory_route, "--frames", frames,
            "--character", characters[-1], "--frame-log", memory, "--frame-log-frames", f"{frames-1}:{frames-1}",
            "--present-out", out / f"{name}.png"])
        assert bool(cpu_ram_from_log(memory)[0x5f2]) == opened, "El inventario no cambio de estado"
        assert size(out / f"{name}.png") == (426, 240)
    print(f"Mapa: {len(characters)} apariencias e inventario con el botón B.", flush=True)

    for suit in range(7):
        controls = out / f"suit-{suit}.txt"
        controls.write_text(route.read_text().replace("69 NONE\n", f"69 NONE\n69 HOLD 0746 {suit:02X}\n"),
                            encoding="ascii")
        suit_state = out / f"suit-{suit}.cycstate"
        memory = out / f"suit-{suit}.bin"
        run(f"suit-{suit}-start", ["--frames", 450, "--input", controls,
            "--save-state", f"449:{suit_state}", "--frame-log", memory, "--frame-log-frames", "449:449"])
        assert cpu_ram_from_log(memory)[0xed] == suit, "El nivel no recibio el poder del mapa"
        expected_palettes = [(0x16, 0x36, 0x0f), (0x16, 0x36, 0x0f), (0x27, 0x36, 0x16),
                             (0x16, 0x36, 0x0f), (0x2a, 0x36, 0x0f), (0x17, 0x36, 0x0f),
                             (0x30, 0x27, 0x0f)]
        assert tuple(memory.read_bytes()[65:68]) == expected_palettes[suit], "Paleta de poder incorrecta"
        reference = None
        for character in characters:
            name = f"suit-{suit}-{character}"
            run(name, ["--load-state", suit_state, "--frames", 462,
                       "--character", character, "--hash-out", out / f"{name}.hash",
                       "--present-out", out / f"{name}.png"])
            current = (out / f"{name}.hash").read_bytes()
            if reference is None:
                reference = current
            assert reference == current, "El personaje altero la transformacion"
            assert size(out / f"{name}.png") == (426, 240)
    print(f"Siete poderes por {len(characters)} personajes (selección asistida).", flush=True)

    leaf_route = out / "leaf-pickup.txt"
    # Put a collectible leaf beside the player in 1-1 and let the ROM handle the pickup.
    leaf_route.write_text("450 POKE 0661-0668 00\n450 HOLD 7B10-7B3F 80\n"
                          "450 POKE 0661 02\n450 POKE 0671 1E\n450 POKE 0076 00\n450 POKE 0091 18\n"
                          "450 POKE 0088 01\n450 POKE 00A3 8C\n450 POKE 0518 00\n450 POKE 0520 00\n",
                          encoding="ascii")
    leaf_state = out / "suit-1.cycstate"
    for character in characters:
        name = f"leaf-poof-{character}"
        memory = out / f"{name}.bin"
        run(name, ["--load-state", leaf_state, "--input", leaf_route, "--frames", 463,
                   "--character", character, "--frame-log", memory, "--frame-log-frames", "462:462",
                   "--present-out", out / f"{name}.png"])
        ram = cpu_ram_from_log(memory)
        assert ram[0x554] and ram[0xed] == 3, "La hoja no activó la transformación nativa"
        assert (out / f"{name}.png").read_bytes() == (out / "leaf-poof-mario.png").read_bytes(), \
            f"El sprite de {character} tapó el humo de transformación"
    print(f"Humo de transformación visible para los {len(characters)} personajes.", flush=True)

    flight_route = out / "flight.txt"
    flight_inputs = ["450 POKE 03DD 7F", "450 POKE 056E FF"]
    for frame in range(450, 483, 8):
        flight_inputs.extend((f"{frame} A", f"{frame + 5} NONE"))
    flight_inputs.extend(("490 POKE 056E 00", "490 NONE"))
    for frame in range(498, 523, 8):
        flight_inputs.extend((f"{frame} A", f"{frame + 5} NONE"))
    flight_route.write_text("\n".join(flight_inputs) + "\n", encoding="ascii")
    for suit in (3, 5):
        trace = out / f"flight-{suit}.bin"
        options = ["--load-state", out / f"suit-{suit}.cycstate", "--frames", 525,
                   "--input", flight_route]
        baseline = out / f"flight-{suit}-mario.hash"
        run(f"flight-{suit}-mario", options + ["--hash-out", baseline,
            "--frame-log", trace, "--frame-log-frames", "450:524"])
        flown, fluttered = set(), set()
        for _, ram, _ in captures(trace):
            if ram[0xed] == suit and ram[0xd8] and not ram[0x575]:
                assert ram[0xbd] == 0, "El vuelo vertical requiere velocidad horizontal cero"
                (flown if ram[0x56e] else fluttered).add(ram[0xee])
        assert {0x36, 0x37, 0x38} <= flown, "Faltan cuadros de vuelo del traje"
        assert {0x08, 0x09, 0x0a} <= fluttered, "Faltan cuadros de planeo del traje"
        for character in characters[1:]:
            name = f"flight-{suit}-{character}"
            digest = out / f"{name}.hash"
            run(name, options + ["--character", character, "--hash-out", digest,
                "--present-out", out / f"{name}.png", "--present-every", 14])
            assert digest.read_bytes() == baseline.read_bytes(), "El aspecto cambió el vuelo"
    print("Mapache y Tanooki: tres cuadros de vuelo vertical y tres de planeo por traje.", flush=True)

    throw_route = out / "air-throw.txt"
    throw_route.write_text("450 A\n454 NONE\n455 B\n456 NONE\n", encoding="ascii")
    for suit in (2, 6):
        trace = out / f"air-throw-{suit}.bin"
        options = ["--load-state", out / f"suit-{suit}.cycstate", "--frames", 469, "--input", throw_route]
        baseline = out / f"air-throw-{suit}-mario.hash"
        run(f"air-throw-{suit}-mario", options + ["--hash-out", baseline,
            "--frame-log", trace, "--frame-log-frames", "450:468"])
        frames = {ram[0xee] for _, ram, _ in captures(trace) if ram[0xed] == suit and ram[0xd8] and ram[0x513]}
        assert {0x34, 0x35, 0x36} <= frames, "Faltan cuadros de lanzamiento en el aire"
        for character in characters[1:]:
            name = f"air-throw-{suit}-{character}"
            digest = out / f"{name}.hash"
            run(name, options + ["--character", character, "--hash-out", digest,
                "--present-out", out / f"{name}.png", "--present-every", 2])
            assert digest.read_bytes() == baseline.read_bytes(), "El aspecto cambió el lanzamiento"
    print("Fuego y Martillo: lanzamiento en el aire con los cuadros y tiempos nativos.", flush=True)

    for aspect, expected in (("native", (256, 240)), ("16:9", (426, 240)), ("fit", (560, 240))):
        name = "aspect-" + aspect.replace(":", "-")
        run(name, ["--load-state", state, "--frames", 452, "--aspect", aspect,
                   "--present-size", "1680x720", "--present-out", out / f"{name}.png"])
        assert size(out / f"{name}.png") == expected
    with tempfile.TemporaryDirectory(dir=out, prefix="rom-only-") as temporary:
        isolated_binary = Path(temporary) / binary.name
        shutil.copy2(binary, isolated_binary)
        # DLLs, when present, belong beside the Windows executable too.
        for dll in binary.parent.glob("*.dll"):
            shutil.copy2(dll, Path(temporary) / dll.name)
        for character in base_characters:
            run(f"rom-only-{character}", ["--load-state", state, "--frames", 452,
                "--character", character, "--present-out", out / f"rom-only-{character}.png"], isolated_binary)
            assert size(out / f"rom-only-{character}.png") == (426, 240)
        unavailable = subprocess.run([str(isolated_binary), str(rom), "--frames", "1", "--no-save",
                                      "--character", "toadette"], capture_output=True, timeout=30)
        assert unavailable.returncode == 2 and b"requiere el arte local" in unavailable.stderr
    result = {"result": "passed", "characters": len(characters), "map_characters": len(characters),
              "map_inventory_toggle": True, "power_state_screens": 7 * len(characters),
              "leaf_poof_characters": len(characters),
              "flight_and_flutter_suits": [3, 5],
              "air_throw_suits": [2, 6],
              "character_hardware_audio_parity": True, "native_classic_parity": True,
              "aspect_modes": 3, "rom_only_characters": len(base_characters),
              "complete_campaign_playthrough": False,
              "executable_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}
    report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Presentacion y ejecucion sin arte externo comprobadas: {report}")


if __name__ == "__main__":
    main()

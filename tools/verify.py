#!/usr/bin/env python3
"""Local ROM-required gameplay, state and execution-parity checks."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import wave

from smb3 import ROOT, executable, read_rom


def cpu_ram_from_log(path):
    data = path.read_bytes()
    if data[:12] != b"CYCFRAME\x02\x00\x00\x00":
        raise ValueError("Formato inesperado de la captura de memoria.")
    header = struct.unpack_from("<9I", data, 12)
    if header[3] != 2048 or len(data) != 12 + 36 + 288 + sum(header[3:]):
        raise ValueError("La captura debe contener exactamente un fotograma completo.")
    return data[12 + 36 + 288:12 + 36 + 288 + 2048]


def main():
    rom, binary = ROOT / "private/baserom.nes", executable()
    read_rom(rom)
    if not binary.exists():
        raise SystemExit("Primero ejecuta: python3 tools/smb3.py build")
    out = ROOT / "verification/checks"
    out.mkdir(parents=True, exist_ok=True)
    (out / "result.json").unlink(missing_ok=True)

    def run(name, options, image=rom, success=True):
        result = subprocess.run([str(binary), str(image), "--no-save", *map(str, options)],
                                capture_output=True, encoding="utf-8", errors="replace", timeout=180,
                                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        output = result.stdout + result.stderr
        (out / (name + ".log")).write_text(output, encoding="utf-8")
        if (result.returncode == 0) != success:
            raise RuntimeError(f"Fallo en {name}; consulta {out / (name + '.log')}")
        return output

    route = ROOT / "tests/fixtures/first-level.txt"
    native = run("native", ["--frames", 600, "--input", route,
                            "--hash-out", out / "native.hash", "--screenshot", out / "first-level.png",
                            "--frame-log", out / "first-level.bin", "--frame-log-frames", "599:599",
                            "--wav-out", out / "audio.wav", "--save-state", f"300:{out / 'checkpoint.cycstate'}",
                            "--miss-log", out / "seeds.txt"])
    run("interpreter", ["--frames", 600, "--input", route, "--interp-only",
                        "--hash-out", out / "interpreter.hash", "--screenshot", out / "interpreter.png",
                        "--wav-out", out / "interpreter.wav"])
    assert (out / "native.hash").read_bytes() == (out / "interpreter.hash").read_bytes(), "Native/interpreter state mismatch"
    assert (out / "first-level.png").read_bytes() == (out / "interpreter.png").read_bytes(), "Picture mismatch"
    assert (out / "audio.wav").read_bytes() == (out / "interpreter.wav").read_bytes(), "Audio mismatch"
    ram = cpu_ram_from_log(out / "first-level.bin")
    assert ram[0x070A] == 1 and ram[0x0727] == 0, "The input route did not reach World 1-1"
    with wave.open(str(out / "audio.wav"), "rb") as audio:
        assert audio.getnframes() > 0 and any(audio.readframes(audio.getnframes())), "Silent audio"

    run("restored", ["--frames", 600, "--input", route, "--load-state", out / "checkpoint.cycstate",
                     "--hash-out", out / "restored.hash", "--wav-out", out / "restored.wav"])
    lines = (out / "native.hash").read_text().splitlines()
    restored = (out / "restored.hash").read_text().splitlines()
    assert restored, "Empty state replay"
    first = int(restored[0].split()[0])
    assert 300 <= first < 600 and lines[first:] == restored, "Save/load changed the resumed game"
    print("1-1: imagen, audio, CPU y memoria coinciden; guardar/cargar conserva la continuacion.", flush=True)

    worlds = []
    for world in range(8):
        # QA only: change the starting world before the original map loader.
        # This checks map loading, not clearing those worlds in a campaign.
        world_route = out / f"world-{world + 1}.txt"
        world_route.write_text(f"12 START\n13 NONE\n68 START\n69 NONE\n69 HOLD 0727 {world:02X}\n", encoding="ascii")
        options = ["--frames", 450, "--input", world_route]
        run(f"world-{world + 1}-native", options + ["--hash-out", out / f"world-{world + 1}.hash",
            "--aspect", "fit", "--present-size", "1280x720",
            "--present-out", out / f"world-{world + 1}-remastered.png",
            "--screenshot", out / f"world-{world + 1}.png", "--frame-log", out / f"world-{world + 1}.bin",
            "--frame-log-frames", "449:449", "--miss-log", out / "seeds.txt"])
        run(f"world-{world + 1}-interpreter", options + ["--interp-only", "--hash-out", out / "world-interpreter.hash"])
        assert (out / f"world-{world + 1}.hash").read_bytes() == (out / "world-interpreter.hash").read_bytes(), f"World {world + 1} execution mismatch"
        ram = cpu_ram_from_log(out / f"world-{world + 1}.bin")
        assert ram[0x0727] == world and ram[0x070A] == 0, f"World {world + 1} map did not load"
        worlds.append(world + 1)
    print("Mapas de los mundos 1 a 8: cargan y coinciden entre ambas ejecuciones.", flush=True)

    for name, offset in (("wrong-rom", 16), ("wrong-graphics", 16 + 262144)):
        wrong = bytearray(rom.read_bytes())
        wrong[offset] ^= 1
        wrong_path = out / f"{name}.nes"
        wrong_path.write_bytes(wrong)
        rejected = run(name, ["--frames", 1], image=wrong_path, success=False)
        assert "does not match" in rejected, f"Invalid cartridge accepted: {name}"
    cpu_cycles, native_cycles = map(int, re.search(r"frames=\d+ cycles=(\d+) native_cycles=(\d+)", native).groups())
    summary = {
        "rom_sha256": hashlib.sha256(rom.read_bytes()).hexdigest(),
        "executable_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "native_coverage": f"{100 * native_cycles / cpu_cycles:.3f}%",
        "native_cycles": native_cycles,
        "cpu_cycles": cpu_cycles,
        "gameplay_frames": 600,
        "world_maps": worlds,
        "native_interpreter_match": True,
        "save_load_continuation": True,
        "wrong_rom_rejected": True,
        "modified_chr_rejected": True,
        "complete_playthrough_verified": False,
    }
    (out / "result.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Verificacion correcta. Cobertura nativa en la ruta: {summary['native_coverage']}. Resultados: {out}")


if __name__ == "__main__":
    main()

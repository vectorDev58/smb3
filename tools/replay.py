#!/usr/bin/env python3
"""Convert a text FCEUX FM2 input movie into a NESRecomp input schedule."""
import argparse
from pathlib import Path


def convert_fm2(text, offset=0):
    headers = {}
    schedule = []
    previous = [None, None]
    frame = 0
    buttons = ("RIGHT", "LEFT", "DOWN", "UP", "START", "SELECT", "B", "A")
    for line in text.splitlines():
        if not line.startswith("|"):
            key, _, value = line.partition(" ")
            headers[key] = value
            continue
        if headers.get("version") != "3" or headers.get("palFlag", "0") != "0":
            raise ValueError("Se necesita un FM2 de texto, version 3, NTSC.")
        if headers.get("fourscore", "0") != "0" or "savestate" in headers:
            raise ValueError("Solo se aceptan dos controles y arranque en frio.")
        fields = line.split("|")
        if len(fields) < 5 or fields[1] != "0":
            raise ValueError("No se admiten reinicios ni comandos de consola en la grabacion.")
        for port, pad in enumerate(fields[2:4]):
            if pad == "" and headers.get(f"port{port}", "0") == "0":
                pad = "........"
            if len(pad) != 8 or any(c not in (".", expected) for c, expected in zip(pad, "RLDUTSBA")):
                raise ValueError(f"Control no valido en el fotograma {frame}.")
            if pad != previous[port]:
                held = "+".join(name for char, name in zip(pad, buttons) if char != ".") or "NONE"
                if frame + offset < 0:
                    if held != "NONE":
                        raise ValueError("El desplazamiento elimina entradas de la grabacion.")
                else:
                    schedule.append(f"{frame + offset} {port + 1}:{held}")
                previous[port] = pad
        frame += 1
    if not frame:
        raise ValueError("La grabacion no contiene fotogramas.")
    return "\n".join(schedule) + "\n", frame, headers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("movie", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--offset", type=int, default=0)
    args = parser.parse_args()
    schedule, frames, _ = convert_fm2(args.movie.read_text(encoding="latin-1"), args.offset)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(schedule, encoding="ascii")
    print(f"{frames} fotogramas; controles escritos en {args.output}")


if __name__ == "__main__":
    main()

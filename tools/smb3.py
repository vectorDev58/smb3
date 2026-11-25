#!/usr/bin/env python3
"""Importar, compilar y jugar SMB3 PC. Solo necesita la biblioteca estandar."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from graphics import import_character_reference, import_controls, import_graphics, stage_graphics

ROOT = Path(__file__).resolve().parents[1]
ROM_SIZE = 393232
ROM_SHA256 = "6ea0777ca520ba7ad7b0ea0f6452140d59aa0b1dffe5045cb1e8020dde10c267"


def read_rom(source, *, spanish=True):
    source = Path(source)
    if source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            entries = [entry for entry in archive.infolist()
                       if not entry.is_dir() and Path(entry.filename).suffix.lower() == ".nes"]
            if len(entries) != 1:
                raise ValueError("El ZIP debe contener exactamente una ROM .nes." if spanish else
                                 "The ZIP must contain exactly one .nes ROM.")
            if entries[0].file_size != ROM_SIZE:
                raise ValueError("La ROM del ZIP tiene un tamano distinto al esperado." if spanish else
                                 "The ROM inside the ZIP has an unexpected size.")
            data = archive.read(entries[0])
    else:
        if source.stat().st_size != ROM_SIZE:
            raise ValueError("La ROM debe tener 393232 bytes, incluida su cabecera iNES." if spanish else
                             "The ROM must be 393,232 bytes including its iNES header.")
        data = source.read_bytes()
    if data[:4] != b"NES\x1a" or hashlib.sha256(data).hexdigest() != ROM_SHA256:
        raise ValueError("ROM no compatible. Se necesita Super Mario Bros. 3 (USA), PRG0. Consulta README.md."
                         if spanish else "Unsupported ROM. Select Super Mario Bros. 3 (USA), PRG0. See README.md.")
    return data


def import_rom(source, destination=None, *, spanish=True):
    data = read_rom(source, spanish=spanish)
    destination = Path(destination) if destination else ROOT / "private/baserom.nes"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".nes.tmp")
    temporary.write_bytes(data)
    temporary.replace(destination)
    return destination


def run(command, cwd=ROOT):
    print("+ " + " ".join(str(part) for part in command), flush=True)
    result = subprocess.run([str(part) for part in command], cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace",
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    print(result.stdout, end="", flush=True)
    result.check_returncode()


def prepare_dependencies():
    dependencies = json.loads((ROOT / "dependencies.json").read_text(encoding="utf-8"))
    for name, dependency in dependencies.items():
        destination = ROOT / ".deps" / name
        revision = dependency["revision"]
        if not (destination / ".git").exists():
            run(["git", "init", destination])
        head = subprocess.run(["git", "-C", str(destination), "rev-parse", "--verify", "HEAD"],
                              capture_output=True, text=True)
        if head.returncode:
            run(["git", "-C", destination, "fetch", "--depth", "1", dependency["url"], revision])
            run(["git", "-C", destination, "checkout", "--detach", revision])
            head = subprocess.run(["git", "-C", str(destination), "rev-parse", "HEAD"],
                                  check=True, capture_output=True, text=True)
        current = head.stdout.strip()
        if current != revision:
            raise ValueError(f"{destination} tiene otra revision. Conserva tus cambios antes de reemplazarla.")
        changes = subprocess.check_output(
            ["git", "-C", str(destination), "status", "--porcelain", "--untracked-files=no"], text=True)
        if changes:
            raise ValueError(f"{destination} tiene cambios locales; se conservan. Usa las revisiones de dependencies.json para esta compilacion.")
    run(["git", "-C", ROOT / ".deps/nesrecomp", "submodule", "update", "--init", "--depth", "1",
         "lib/recomp-net", "lib/retcomm-rbengine"])


def executable():
    candidates = [ROOT / "build/smb3", ROOT / "build/Release/smb3.exe", ROOT / "build/smb3.exe"]
    return next((path for path in candidates if path.is_file()), candidates[0])


def build():
    prepare_dependencies()
    command = ["cmake", "-S", ROOT, "-B", ROOT / "build",
               "-DCMAKE_BUILD_TYPE=Release", "-DSMB3_NATIVE_RECOMP=OFF"]
    if not (ROOT / "build/CMakeCache.txt").exists() and not os.environ.get("CMAKE_GENERATOR"):
        if sys.platform == "win32":
            command += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        elif shutil.which("ninja"):
            command += ["-G", "Ninja"]
    run(command)
    run(["cmake", "--build", ROOT / "build", "--config", "Release", "--parallel", "4"])
    ensure_config()
    stage_graphics(ROOT, executable().parent)
    print(f"Listo: {executable()}")


def ensure_config():
    destination = executable().parent / "config.ini"
    if not destination.exists():
        shutil.copyfile(ROOT / "config.default.ini", destination)


def play():
    from startup import preparing, request_rom, show_error, spanish_ui
    rom = ROOT / "private/baserom.nes"
    spanish = spanish_ui(executable().parent / "config.ini")

    def import_selected(source, destination):
        return import_rom(source, destination, spanish=spanish)

    try:
        if rom.exists():
            read_rom(rom, spanish=spanish)
        elif not request_rom(rom, import_selected, spanish=spanish):
            return 0
        if not executable().exists():
            with preparing(spanish):
                build()
        ensure_config()
        stage_graphics(ROOT, executable().parent)
        environment = dict(os.environ, NESRECOMP_NO_LAUNCHER="1")
        return subprocess.call([str(executable()), str(rom)], cwd=executable().parent, env=environment)
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        title = "No se pudo preparar el juego." if spanish else "Could not prepare the game."
        show_error(f"{title}\n\n{error}")
        return 1


def source_files():
    top_files = {"CMakeLists.txt", "game.toml", "config.default.ini", "dependencies.json",
                 ".gitignore", ".gitattributes", "README.md", "LICENSE", "THIRD_PARTY.md",
                 "AGENTS.md", "play.sh", "play.cmd"}
    folders = {"tools", "tests", "docs", "licenses", ".github", "recomp", "src"}
    public_extensions = {".c", ".cpp", ".h", ".ini", ".json", ".md", ".py", ".toml", ".txt", ".yml"}
    candidates = [ROOT / name for name in top_files if (ROOT / name).exists()]
    for folder in folders:
        candidates.extend((ROOT / folder).rglob("*"))
    for path in sorted(candidates):
        relative = path.relative_to(ROOT)
        if relative.parts[0] not in folders and relative.as_posix() not in top_files:
            continue
        if path.is_symlink():
            raise ValueError(f"El paquete de fuentes no admite enlaces simbolicos: {relative}")
        if path.is_file() and "__pycache__" not in relative.parts:
            if relative.as_posix() not in top_files and (path.suffix.lower() not in public_extensions or
                    path.name in {"character_art.h", "runtime_hooks.inc"}):
                raise ValueError(f"Archivo privado o recurso gráfico dentro de las fuentes: {relative}")
            yield path


def source_zip():
    destination = ROOT / "dist/smb3-pc-source.zip"
    destination.parent.mkdir(exist_ok=True)
    paths = list(source_files())
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in paths:
            archive.write(path, Path("smb3-pc") / path.relative_to(ROOT))
    print(f"Fuentes sin ROM ({len(paths)} archivos): {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    importer = commands.add_parser("import-rom", help="Importa tu .nes o .zip en private/.")
    importer.add_argument("source", type=Path)
    graphics = commands.add_parser("import-graphics", help="Prepara localmente el ZIP SMB3-mastered v0.9.")
    graphics.add_argument("source", type=Path)
    graphics.add_argument("--powers", type=Path, help="Carpeta local con los atlas Campaign.png de Toad y Toadette.")
    characters = commands.add_parser("import-characters", help="Prepara las poses de selector, mapa y vuelo de la hoja aportada.")
    characters.add_argument("source", type=Path)
    characters.add_argument("--luigi-campaign", type=Path, help="Atlas local opcional de animaciones de Luigi.")
    controls = commands.add_parser("import-controls", help="Prepara iconos locales para teclado y mando Xbox.")
    controls.add_argument("source", type=Path)
    commands.add_parser("build", help="Descarga las dependencias fijadas y compila.")
    commands.add_parser("play", help="Abre el lanzador; compila si hace falta.")
    commands.add_parser("source-zip", help="Crea un ZIP de fuentes sin ROM, partidas ni dependencias.")
    args = parser.parse_args()
    try:
        if args.command == "import-rom":
            print(f"ROM verificada e importada: {import_rom(args.source)}")
        elif args.command == "import-graphics":
            count = import_graphics(args.source, ROOT / "private/remaster-art", args.powers)
            stage_graphics(ROOT, executable().parent)
            print(f"Graficos preparados para uso local: {count} imagenes.")
        elif args.command == "import-characters":
            import_character_reference(args.source, ROOT / "private/character-reference", args.luigi_campaign)
            stage_graphics(ROOT, executable().parent)
            print("Hoja de personajes preparada para uso local.")
        elif args.command == "import-controls":
            import_controls(args.source, ROOT / "private/control-reference/Controls")
            stage_graphics(ROOT, executable().parent)
            print("Iconos de Vryell preparados para uso local.")
        elif args.command == "build":
            build()
        elif args.command == "source-zip":
            source_zip()
        else:
            return play()
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

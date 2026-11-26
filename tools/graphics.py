"""Importacion local del arte SMB3-mastered; no instala un sistema de packs."""
import hashlib
from io import BytesIO
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import tempfile
import zipfile


PACK_ROOT = "SMB3-mastered/Sprites/"
PLAYER_SHEETS = {
    f"Players/{character}/{form}.png": (256, height)
    for character in ("Toad", "Toadette")
    for form, height in (("Small", 192), ("Big", 256), ("Fire", 256))
}
ANIMATION_SHEETS = {
    "Blocks/QuestionBlock.json": ("Blocks/QuestionBlock.png", (64, 64)),
    "Tilesets/Terrain/BrickBlock.json": ("Tilesets/Terrain/BrickBlock.png", (64, 64)),
    "Items/Coin.json": ("Items/StaticCoin.png", (64, 112)),
}


def png_size(data, name):
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"PNG no válido: {name}")
    width, height = struct.unpack(">II", data[16:24])
    if not (1 <= width <= 4096 and 1 <= height <= 4096 and width * height <= 4194304):
        raise ValueError(f"Dimensiones de PNG no admitidas: {name}")
    return width, height


def import_graphics(source, destination, powers=None):
    """Importar los fondos y hojas compatibles del pack aportado a una copia local."""
    destination = Path(destination)
    with zipfile.ZipFile(source) as archive:
        if len(archive.infolist()) > 1000:
            raise ValueError("Demasiados archivos en el ZIP de graficos.")
        images = {}
        total = 0
        metadata = {}
        special_images = {**PLAYER_SHEETS, **dict(ANIMATION_SHEETS.values())}
        for entry in archive.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or ".." in path.parts or "\\" in entry.filename:
                raise ValueError("Ruta no permitida en el ZIP de graficos.")
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError("El ZIP de graficos contiene enlaces simbolicos.")
            if entry.is_dir() or not entry.filename.startswith(PACK_ROOT):
                continue
            name = entry.filename.removeprefix(PACK_ROOT)
            if name in ANIMATION_SHEETS:
                if name in metadata or entry.file_size > 16384:
                    raise ValueError(f"Metadatos de animación inválidos: {name}")
                metadata[name] = archive.read(entry)
                continue
            if path.suffix.lower() != ".png" or not (name.startswith("Backgrounds/") or name in special_images):
                continue
            relative = Path(name)
            total += entry.file_size
            if entry.file_size > 8 * 1024 * 1024 or total > 64 * 1024 * 1024:
                raise ValueError("El ZIP de graficos supera el limite de tamano.")
            if relative in images:
                raise ValueError("Imagen duplicada en el ZIP de graficos.")
            data = archive.read(entry)
            size = png_size(data, relative)
            if name in special_images and size != special_images[name]:
                raise ValueError(f"Hoja gráfica incompatible: {relative}")
            images[relative] = data
        for config, (sheet, _) in ANIMATION_SHEETS.items():
            if Path(sheet) not in images:
                continue
            try:
                sequence = json.loads(metadata[config])["animations"]["default"]
                frames = [[i * 16, 0, 16, 16] for i in range(4)]
                if sequence["speed"] != 5 or sequence["frames"] != frames or sequence["loop"] is not True:
                    raise ValueError("Secuencia incompatible")
            except (KeyError, TypeError, ValueError) as error:
                raise ValueError(f"Animación incompatible: {config}") from error
        for key in (Path("Backgrounds/Skies/DaySky.png"), Path("Backgrounds/Hills/Overworld.png")):
            if key not in images:
                raise ValueError(f"Falta {key}; se necesita SMB3-mastered v0.9.")
            width, height = struct.unpack(">II", images[key][16:24])
            if width < 256 or height < 128:
                raise ValueError(f"Fondo incompatible: {key}")
    if powers is not None:
        for character in ("Toad", "Toadette"):
            key = Path("Players") / character / "Campaign.png"
            source_image = Path(powers) / character / key.name
            if source_image.stat().st_size > 2 * 1024 * 1024:
                raise ValueError(f"Atlas de poderes demasiado grande: {source_image}")
            data = source_image.read_bytes()
            if png_size(data, key) != (3072, 224):
                raise ValueError(f"Atlas de poderes incompatible: {source_image}")
            images[key] = data
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise ValueError(f"Ya existe {destination}; conserva esa copia antes de volver a importar.")
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=".graphics-") as temporary:
        staging = Path(temporary) / "art"
        staging.mkdir()
        for relative, data in images.items():
            target = staging / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        manifest = {"name": "SMB3-mastered", "version": "0.9", "author": "RadzPrower",
                    "original_art": "Nintendo", "local_only": True,
                    "images": {path.as_posix(): hashlib.sha256(data).hexdigest()
                               for path, data in sorted(images.items())}}
        (staging / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        staging.rename(destination)
    return len(images)


def stage_graphics(root, binary_directory):
    for directory in ("remaster-art", "character-reference", "control-reference"):
        source = Path(root) / "private" / directory
        if source.is_dir():
            shutil.copytree(source, Path(binary_directory) / "assets/remaster", dirs_exist_ok=True)


def validate_full_graphics(directory):
    required = {**PLAYER_SHEETS, **dict(ANIMATION_SHEETS.values()), "Controls/Hints.png": (224, 32),
                "Backgrounds/Skies/DaySky.png": None, "Backgrounds/Hills/Overworld.png": None}
    for character in ("Mario", "Luigi", "Toad", "Toadette"):
        required[f"Players/{character}/Selector.png"] = (64, 32)
    for character in ("Luigi", "Toad", "Toadette"):
        required.update({f"Players/{character}/{name}.png": size for name, size in
                         (("Campaign", (3072, 224)), ("Map", (192, 224)), ("Airborne", (352, 224)))})
    missing = [name for name in required if not (directory / name).is_file()]
    if missing:
        raise ValueError("Falta arte local para el paquete completo. " + ", ".join(sorted(missing)))
    for path in directory.rglob("*"):
        if path.is_dir():
            continue
        name = path.relative_to(directory).as_posix()
        if name in {"manifest.json", "character-reference.json"}:
            continue
        if path.suffix != ".png":
            raise ValueError(f"Archivo inesperado entre los gráficos locales: {name}")
        with path.open("rb") as image:
            size = png_size(image.read(24), name)
        if required.get(name) and size != required[name]:
            raise ValueError(f"Dimensiones de PNG incompatibles: {name}")


def import_character_reference(source, destination, luigi_campaign=None):
    """Preparar las poses del selector, del mapa y del vuelo de la hoja aportada."""
    try:
        from PIL import Image
    except ImportError as error:
        raise ValueError("La importación de la hoja de personajes requiere Pillow.") from error

    source, destination = Path(source), Path(destination)
    if source.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("La hoja de personajes supera el límite de tamaño.")
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if destination.exists():
        metadata = destination / "character-reference.json"
        saved = json.loads(metadata.read_text(encoding="utf-8")) if metadata.is_file() else {}
        if not isinstance(saved, dict) or saved.get("source_sha256") != source_hash:
            raise ValueError(f"Ya existe {destination} y corresponde a otra hoja de personajes.")

    campaign = None
    if luigi_campaign is not None:
        if Path(luigi_campaign).stat().st_size > 2 * 1024 * 1024:
            raise ValueError("El atlas de Luigi supera el límite de tamaño.")
        campaign = Path(luigi_campaign).read_bytes()
        if png_size(campaign, luigi_campaign) != (3072, 224):
            raise ValueError("El atlas Campaign.png de Luigi debe medir 3072 × 224.")

    with Image.open(source) as image:
        if image.size != (1961, 4246) or getattr(image, "n_frames", 1) != 1:
            raise ValueError("Se necesita la hoja expandida de personajes de 1961 × 4246.")
        sheet = image.convert("RGB")

    cell_backgrounds = {(145, 160, 204), (58, 111, 51)}
    backgrounds = cell_backgrounds | {(47, 51, 104)}

    def frame_image(x, top, bottom, name):
        cell = sheet.crop((x, top, x + 32, bottom)).convert("RGBA")
        opacity = bytes(0 if cell.getpixel((px, py))[:3] in backgrounds else 255
                        for py in range(cell.height) for px in range(cell.width))
        cell.putalpha(Image.frombytes("L", cell.size, opacity))
        if not cell.getbbox():
            raise ValueError(f"Cuadro vacío en la hoja de {name}.")
        return cell

    columns = (1, 429, 499, 668, 429, 668, 499)
    spin_columns = (71, 104, 137, 170)
    airborne_rows = (24, 212, 484, 751, 1114, 1423, 1786)
    # Columns follow AirbornePose in src/characters.h. Empty cells keep the standard pose.
    standard_jump = (351, 384, 421)
    tail_jump = (454, 487, None, 697, 730, 763, 524, 557, 590)
    airborne_columns = ((285, None, 355), standard_jump, standard_jump, tail_jump,
                        (244,), tail_jump, standard_jump)
    layouts = (
        ("Luigi", 981, (199, 471, 738, 1101, 1410, 1773, 2040), 429),
        ("Toad", 0, (2240, 2512, 2779, 3142, 3451, 3814, 4081), 429),
        ("Toadette", 981, (2240, 2512, 2779, 3142, 3451, 3814, 4081), 606),
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=".characters-") as temporary:
        staging = Path(temporary) / "art"
        for name, offset, top in (("Mario", 0, 272), ("Luigi", 981, 272),
                                  ("Toad", 0, 2313), ("Toadette", 981, 2313)):
            selector = Image.new("RGBA", (64, 32))
            for index, (column, y) in enumerate(((524, top - 60), (268, top))):
                selector.paste(frame_image(offset + column, y, y + 32, name), (index * 32, 0))
            path = staging / "Players" / name / "Selector.png"
            path.parent.mkdir(parents=True, exist_ok=True)
            selector.save(path)
        for name, offset, separators, frog_column in layouts:
            atlas = Image.new("RGBA", (192, 224))
            airborne = Image.new("RGBA", (352, 224))
            for suit, (column, separator) in enumerate(zip(columns, separators)):
                if suit == 4:
                    column = frog_column
                frames = (column, column + 33) + (spin_columns if suit == 0 else ())
                for frame, left in enumerate(frames):
                    x, bottom = offset + left, separator - 1
                    top = bottom
                    while bottom - top < 32 and sheet.getpixel((x, top - 1)) in cell_backgrounds:
                        top -= 1
                    if bottom - top not in (16, 24, 26):
                        raise ValueError(f"Cuadro de mapa incompatible en la hoja de {name}.")
                    cell = frame_image(x, top, bottom, name)
                    atlas.paste(cell, (frame * 32, suit * 32 + 32 - cell.height))
                top = airborne_rows[suit] + (2041 if name != "Luigi" else 0)
                height = 32 if suit else 16
                for frame, column in enumerate(airborne_columns[suit]):
                    if column is not None:
                        cell = frame_image(offset + column, top, top + height, name)
                        airborne.paste(cell, (frame * 32, suit * 32 + 32 - height))
                if suit in (2, 6):
                    for frame, column in enumerate((1, 34), start=9):
                        cell = frame_image(offset + column, top + 55, top + 87, name)
                        airborne.paste(cell, (frame * 32, suit * 32))
            path = staging / "Players" / name / "Map.png"
            path.parent.mkdir(parents=True, exist_ok=True)
            atlas.save(path)
            airborne.save(path.with_name("Airborne.png"))
        if campaign is not None:
            (staging / "Players/Luigi/Campaign.png").write_bytes(campaign)
        credits = {"title": "Expanded SMM2 SMB3 Sprites for Mario, Luigi, Toad and Toadette",
                   "authors": ["GlacialSiren484", "SuperCoolFernie", "PixelMarioXP"],
                   "source_sha256": source_hash,
                   "local_only": True}
        (staging / "character-reference.json").write_text(json.dumps(credits, indent=2) + "\n", encoding="utf-8")
        if destination.exists():
            for path in staging.glob("Players/*/*.png"):
                target = destination / path.relative_to(staging)
                if path.name == "Map.png" and target.exists():
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                path.replace(target)
        else:
            staging.rename(destination)


def import_controls(source, destination):
    """Build local menu hints from Vryell's Controllers and Keyboard pack."""
    try:
        from PIL import Image
    except ImportError as error:
        raise ValueError("La importación de iconos requiere Pillow.") from error

    with zipfile.ZipFile(source) as archive:
        def read_sheet(name, size):
            path = f"Controllers and Keyboard/{name}.png"
            entries = [entry for entry in archive.infolist() if entry.filename == path]
            if len(entries) != 1 or stat.S_ISLNK(entries[0].external_attr >> 16):
                raise ValueError(f"El ZIP necesita exactamente una hoja válida: {path}")
            if entries[0].file_size > 64 * 1024:
                raise ValueError("La hoja de controles supera el límite de tamaño.")
            data = archive.read(entries[0])
            if png_size(data, path) != size:
                raise ValueError(f"Dimensiones inesperadas en {path}.")
            with Image.open(BytesIO(data)) as sheet:
                return sheet.convert("RGBA")

        keyboard = read_sheet("kb_dark_all", (256, 416))
        controller = read_sheet("controller_xbox", (112, 432))
    # Order matches HintIcon in frontend.h. Each cell leaves room for two directions.
    keyboard_cells = (((208, 192), (208, 224)), ((208, 208), (208, 240)),
                      ((144, 384),), ((144, 0),), ((144, 384),), ((208, 112),), ((144, 0),))
    controller_cells = (((16, 160), (48, 160)), ((64, 160), (32, 160)),
                        ((48, 64),), ((48, 96),), ((48, 176),), ((48, 0),), ((48, 352),))
    atlas = Image.new("RGBA", (32 * len(keyboard_cells), 32))
    for row, (sheet, cells) in enumerate(((keyboard, keyboard_cells), (controller, controller_cells))):
        for index, icons in enumerate(cells):
            left = 32 * index + (8 if len(icons) == 1 else 0)
            for column, (x, y) in enumerate(icons):
                atlas.paste(sheet.crop((x, y, x + 16, y + 16)), (left + column * 16, row * 16))
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        output = Path(temporary) / "Hints.png"
        atlas.save(output)
        output.replace(destination / "Hints.png")

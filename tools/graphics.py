"""Copia el arte local que prepararon versiones anteriores de los importadores."""
from pathlib import Path
import shutil


def stage_graphics(root, binary_directory):
    """El ejecutable usa assets/remaster antes que resource_packs; ambos quedan fuera de Git."""
    for directory in ("remaster-art", "character-reference", "control-reference"):
        source = Path(root) / "private" / directory
        if source.is_dir():
            shutil.copytree(source, Path(binary_directory) / "assets/remaster", dirs_exist_ok=True)

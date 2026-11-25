import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from graphics import import_controls, import_graphics, png_size, stage_graphics


def png(width=256, height=128):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    pixels = zlib.compress(bytes((width * 4 + 1) * height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", pixels) + chunk(b"IEND", b"")


class GraphicsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "art.zip"
        self.destination = self.root / "private/remaster-art"

    def archive(self, extra=None, background_png=None):
        with zipfile.ZipFile(self.source, "w", zipfile.ZIP_DEFLATED) as archive:
            for name in ("Skies/DaySky.png", "Hills/Overworld.png"):
                archive.writestr(f"SMB3-mastered/Sprites/Backgrounds/{name}", background_png or png())
            archive.writestr("SMB3-mastered/Sprites/Players/Mario/Big.png", png())
            archive.writestr("SMB3-mastered/Audio/unused.mp3", b"not imported")
            if extra:
                archive.writestr(*extra)

    def test_import_stage_and_original_preserved(self):
        self.archive()
        original = hashlib.sha256(self.source.read_bytes()).hexdigest()
        self.assertEqual(import_graphics(self.source, self.destination), 2)
        self.assertTrue((self.destination / "manifest.json").is_file())
        self.assertFalse((self.destination / "Audio").exists())
        self.assertFalse((self.destination / "Players").exists())
        stage_graphics(self.root, self.root / "build")
        self.assertEqual((self.root / "build/assets/remaster/Backgrounds/Skies/DaySky.png").read_bytes(), png())
        self.assertEqual(hashlib.sha256(self.source.read_bytes()).hexdigest(), original)
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)

    def test_reject_traversal_before_writing(self):
        self.archive(("SMB3-mastered/Sprites/../../escape.png", png()))
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)
        self.assertFalse(self.destination.exists())
        self.assertFalse((self.root / "escape.png").exists())

    def test_reject_missing_backgrounds_and_wrong_dimensions(self):
        with zipfile.ZipFile(self.source, "w") as archive:
            archive.writestr("SMB3-mastered/Sprites/Backgrounds/Skies/DaySky.png", png())
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)
        self.archive(background_png=png(16, 16))
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)
        self.assertFalse(self.destination.exists())

    def test_reject_invalid_png_and_duplicate(self):
        self.archive(background_png=b"invalid PNG")
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)
        import warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            self.archive(("SMB3-mastered/Sprites/Backgrounds/Skies/DaySky.png", png()))
        with self.assertRaises(ValueError):
            import_graphics(self.source, self.destination)

    def test_imports_compatible_character_and_animated_tile(self):
        self.archive()
        prefix = "SMB3-mastered/Sprites/"
        with zipfile.ZipFile(self.source, "a") as archive:
            archive.writestr(prefix + "Players/Toad/Small.png", png(256, 192))
            archive.writestr(prefix + "Blocks/QuestionBlock.png", png(64, 64))
            archive.writestr(prefix + "Blocks/QuestionBlock.json", json.dumps({
                "animations": {"default": {"frames": [[i * 16, 0, 16, 16] for i in range(4)],
                                           "speed": 5, "loop": True}}
            }))
        self.assertEqual(import_graphics(self.source, self.destination), 4)
        self.assertTrue((self.destination / "Players/Toad/Small.png").is_file())
        self.assertTrue((self.destination / "Blocks/QuestionBlock.png").is_file())
        self.assertFalse((self.destination / "Blocks/QuestionBlock.json").exists())

    def test_rejects_animation_with_missing_metadata(self):
        self.archive(("SMB3-mastered/Sprites/Items/StaticCoin.png", png(64, 112)))
        with self.assertRaisesRegex(ValueError, "Animación incompatible"):
            import_graphics(self.source, self.destination)
        self.assertFalse(self.destination.exists())

    def test_stages_supplemental_map_sprites_with_the_pack(self):
        self.archive()
        import_graphics(self.source, self.destination)
        atlas = self.root / "private/character-reference/Players/Toad/Map.png"
        atlas.parent.mkdir(parents=True)
        atlas.write_bytes(png(192, 224))
        stage_graphics(self.root, self.root / "build")
        staged = self.root / "build/assets/remaster"
        self.assertEqual((staged / "Players/Toad/Map.png").read_bytes(), atlas.read_bytes())
        self.assertTrue((staged / "Backgrounds/Skies/DaySky.png").is_file())

    @unittest.skipUnless(importlib.util.find_spec("PIL"), "Pillow is required for the icon importer")
    def test_imports_control_icons_independently_and_preserves_them_on_failure(self):
        destination = self.root / "private/control-reference/Controls"
        keyboard = "Controllers and Keyboard/kb_dark_all.png"
        with zipfile.ZipFile(self.source, "w") as archive:
            archive.writestr(keyboard, png(256, 416))
            archive.writestr("Controllers and Keyboard/controller_xbox.png", png(112, 432))
        import_controls(self.source, destination)
        atlas = (destination / "Hints.png").read_bytes()
        self.assertEqual(png_size(atlas, "Hints.png"), (224, 32))
        stage_graphics(self.root, self.root / "build")
        self.assertEqual((self.root / "build/assets/remaster/Controls/Hints.png").read_bytes(), atlas)
        self.assertFalse((self.root / "private/character-reference").exists())

        with zipfile.ZipFile(self.source, "w") as archive:
            archive.writestr(keyboard, png(256, 416))
        with self.assertRaisesRegex(ValueError, "controller_xbox"):
            import_controls(self.source, destination)
        self.assertEqual((destination / "Hints.png").read_bytes(), atlas)


if __name__ == "__main__":
    unittest.main()

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from graphics import stage_graphics


class GraphicsTests(unittest.TestCase):
    def test_stages_previously_prepared_art_beside_the_executable(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for directory, name in (("remaster-art", "Backgrounds/Skies/DaySky.png"),
                                    ("character-reference", "Players/Toad/Map.png"),
                                    ("control-reference", "Controls/Hints.png")):
                source = root / "private" / directory / name
                source.parent.mkdir(parents=True)
                source.write_bytes(name.encode())
            stage_graphics(root, root / "build")
            staged = root / "build/assets/remaster"
            for name in ("Backgrounds/Skies/DaySky.png", "Players/Toad/Map.png", "Controls/Hints.png"):
                self.assertEqual((staged / name).read_bytes(), name.encode())

    def test_nothing_is_staged_without_prepared_art(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage_graphics(root, root / "build")
            self.assertFalse((root / "build").exists())


if __name__ == "__main__":
    unittest.main()

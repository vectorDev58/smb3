import hashlib
from pathlib import Path
import sys
import tempfile
import struct
import unittest
from unittest.mock import Mock, patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import replay
import smb3
import startup
from verify import cpu_ram_from_log
from verify_widescreen import captures


class RomImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = b"NES\x1a" + bytes(smb3.ROM_SIZE - 4)
        digest = patch.object(smb3, "ROM_SHA256", hashlib.sha256(self.data).hexdigest())
        digest.start()
        self.addCleanup(digest.stop)

    def test_import_validated_rom_from_zip_without_extracting_paths(self):
        source = self.root / "game.zip"
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("../../escape.nes", self.data)
        destination = self.root / "private/baserom.nes"
        self.assertEqual(smb3.import_rom(source, destination), destination)
        self.assertEqual(destination.read_bytes(), self.data)
        self.assertFalse((self.root / "escape.nes").exists())

    def test_bad_rom_does_not_overwrite_existing_data(self):
        source, destination = self.root / "wrong.nes", self.root / "baserom.nes"
        source.write_bytes(self.data[:-1] + b"x")
        destination.write_bytes(b"preserve")
        with self.assertRaisesRegex(ValueError, "no compatible"):
            smb3.import_rom(source, destination)
        with self.assertRaisesRegex(ValueError, "Unsupported ROM"):
            smb3.import_rom(source, destination, spanish=False)
        self.assertEqual(destination.read_bytes(), b"preserve")

    def test_reject_multiple_roms(self):
        source = self.root / "ambiguous.zip"
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("first.nes", self.data)
            archive.writestr("second.nes", self.data)
        with self.assertRaisesRegex(ValueError, "exactamente una"):
            smb3.read_rom(source)

    def test_reject_wrong_size_before_reading_archive_entry(self):
        source = self.root / "oversize.zip"
        with zipfile.ZipFile(source, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("game.nes", self.data + b"x")
        with self.assertRaisesRegex(ValueError, "tamano"):
            smb3.read_rom(source)

    def test_original_file_stays_intact(self):
        source = self.root / "game.nes"
        source.write_bytes(self.data)
        smb3.import_rom(source, self.root / "private/baserom.nes")
        self.assertEqual(source.read_bytes(), self.data)

    def test_first_run_retries_an_invalid_rom_before_importing(self):
        wrong, valid = self.root / "wrong.nes", self.root / "game.nes"
        wrong.write_bytes(self.data[:-1] + b"x")
        valid.write_bytes(self.data)
        choices = iter((wrong, valid))
        error = Mock()
        destination = self.root / "private/baserom.nes"
        self.assertTrue(startup.request_rom(destination, smb3.import_rom, lambda: next(choices), error))
        error.assert_called_once()
        self.assertEqual(destination.read_bytes(), self.data)
        self.assertEqual(valid.read_bytes(), self.data)

    def test_first_run_cancel_does_not_import_or_build(self):
        importer = Mock()
        self.assertFalse(startup.request_rom(self.root / "baserom.nes", importer, lambda: None))
        importer.assert_not_called()
        with patch.object(smb3, "ROOT", self.root), patch.object(startup, "request_rom", return_value=False), patch.object(smb3, "build") as build:
            self.assertEqual(smb3.play(), 0)
            build.assert_not_called()

    def test_source_picker_uses_the_saved_menu_language(self):
        config = self.root / "config.ini"
        self.assertFalse(startup.spanish_ui(config))
        config.write_text("[Game]\nLanguage = es\n", encoding="utf-8")
        with patch.object(startup.sys, "platform", "linux"), patch.object(startup.shutil, "which", return_value="zenity"), \
                patch.object(startup.subprocess, "run", return_value=Mock(returncode=1)) as run:
            self.assertIsNone(startup.choose_rom(startup.spanish_ui(config)))
            self.assertIn("ROM de NES o ZIP | *.nes *.zip", run.call_args.args[0])

    def test_prepared_game_opens_its_frontend_and_keeps_the_environment(self):
        rom = self.root / "private/baserom.nes"
        rom.parent.mkdir()
        rom.write_bytes(self.data)
        binary = self.root / "build/smb3"
        binary.parent.mkdir()
        binary.touch()
        with patch.object(smb3, "ROOT", self.root), patch.object(smb3, "ensure_config"), \
                patch.object(smb3, "stage_graphics"), patch.object(smb3.subprocess, "call", return_value=0) as launch, \
                patch.dict(smb3.os.environ, {"SMB3_EXISTING_VARIABLE": "preserved"}):
            self.assertEqual(smb3.play(), 0)
            self.assertEqual(launch.call_args.args[0], [str(binary), str(rom)])
            self.assertEqual(launch.call_args.kwargs["env"]["NESRECOMP_NO_LAUNCHER"], "1")
            self.assertEqual(launch.call_args.kwargs["env"]["SMB3_EXISTING_VARIABLE"], "preserved")


class SourcePackageTests(unittest.TestCase):
    def test_only_public_sources_are_packaged(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(smb3, "ROOT", Path(directory)):
            root = Path(directory)
            for name in ("README.md", "private/baserom.nes", "build/smb3", ".deps/engine.c",
                         "tools/tool.py", "tools/__pycache__/tool.pyc"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            files = {path.relative_to(root).as_posix() for path in smb3.source_files()}
            self.assertEqual(files, {"README.md", "tools/tool.py"})

    def test_rom_in_public_folder_stops_packaging(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(smb3, "ROOT", Path(directory)):
            path = Path(directory) / "tests/accidental.NES"
            path.parent.mkdir()
            path.touch()
            with self.assertRaisesRegex(ValueError, "Archivo privado"):
                list(smb3.source_files())

    def test_packaging_rejects_both_image_files_and_encoded_sprite_tables(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(smb3, "ROOT", Path(directory)):
            root = Path(directory)
            (root / "src").mkdir()
            for name in ("sprite.png", "character_art.h"):
                path = root / "src" / name
                path.touch()
                with self.assertRaisesRegex(ValueError, "Archivo privado o recurso gráfico"):
                    list(smb3.source_files())
                path.unlink()


class MemoryLogTests(unittest.TestCase):
    def test_multiple_full_frames_and_truncation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frames.bin"
            blob = bytes(288 + 2048 + 2048 + 8192)
            data = b"CYCFRAME\x02\x00\x00\x00" + b"".join(
                struct.pack("<9I", frame, 1, 0, 2048, 2048, 8192, 0, 0, 0) + blob for frame in (20, 21))
            path.write_bytes(data)
            frames = list(captures(path))
            self.assertEqual([frame for frame, _, _ in frames], [20, 21])
            self.assertEqual(len(frames[0][1]), 2048)
            self.assertEqual(len(frames[1][2]), 8192)
            path.write_bytes(data[:-1])
            with self.assertRaises(AssertionError):
                list(captures(path))

    def test_complete_single_frame_and_truncation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame.bin"
            ram = bytes(range(256)) * 8
            data = b"CYCFRAME\x02\x00\x00\x00" + struct.pack("<9I", 10, 1, 0, 2048, 0, 0, 0, 0, 0) + bytes(288) + ram
            path.write_bytes(data)
            self.assertEqual(cpu_ram_from_log(path), ram)
            path.write_bytes(data[:-1])
            with self.assertRaises(ValueError):
                cpu_ram_from_log(path)


class ReplayTests(unittest.TestCase):
    HEADER = "version 3\npalFlag 0\nport0 1\nport1 0\nfourscore 0\n"

    def test_button_mapping_and_hold_compression(self):
        text = self.HEADER + "|0|........|||\n|0|R..UTSBA|||\n|0|R..UTSBA|||\n|0|........|||\n"
        schedule, frames, _ = replay.convert_fm2(text)
        self.assertEqual(frames, 4)
        self.assertEqual(schedule, "0 1:NONE\n0 2:NONE\n1 1:RIGHT+UP+START+SELECT+B+A\n3 1:NONE\n")

    def test_both_controllers(self):
        text = self.HEADER.replace("port1 0", "port1 1") + "|0|.......A|.L......||\n"
        self.assertEqual(replay.convert_fm2(text)[0], "0 1:A\n0 2:LEFT\n")

    def test_negative_offset_can_only_remove_neutral_frames(self):
        text = self.HEADER + "|0|........|||\n|0|.......A|||\n"
        self.assertEqual(replay.convert_fm2(text, -1)[0], "0 1:A\n")
        with self.assertRaisesRegex(ValueError, "elimina entradas"):
            replay.convert_fm2(text, -2)

    def test_unsupported_movies_are_rejected(self):
        for text in (self.HEADER.replace("palFlag 0", "palFlag 1") + "|0|........|||\n",
                     self.HEADER + "|1|........|||\n", self.HEADER + "|0|bad|||\n",
                     self.HEADER + "savestate data\n|0|........|||\n", self.HEADER):
            with self.subTest(movie=text), self.assertRaises(ValueError):
                replay.convert_fm2(text)


if __name__ == "__main__":
    unittest.main()

"""Exercise CI contract validators without building the engine or downloading tools."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests/libgodot"))

from check_api import validate_api
from check_library import library_manifest

ROOT = Path(__file__).resolve().parents[2]
HELPERS = ROOT / "tests/libgodot"


class APIContract(unittest.TestCase):
    def setUp(self):
        self.api = json.loads((HELPERS / "fixtures/macos_api.json").read_text())

    def method(self, class_name, method_name):
        entry = next(c for c in self.api["classes"] if c["name"] == class_name)
        return next(m for m in entry["methods"] if m["name"] == method_name)

    def test_engine_dump_excerpt(self):
        self.assertIn("DisplayServerEmbedded.detach_native_surface", validate_api(self.api))

    def test_wrong_engine_version(self):
        self.api["header"]["version_minor"] = 5
        with self.assertRaises(ValueError):
            validate_api(self.api)

    def test_missing_surface_class(self):
        self.api["classes"] = [c for c in self.api["classes"] if c["name"] != "RenderingNativeSurfaceApple"]
        with self.assertRaises(ValueError):
            validate_api(self.api)

    def test_changed_reference_ownership(self):
        entry = next(c for c in self.api["classes"] if c["name"] == "RenderingNativeSurfaceApple")
        entry["is_refcounted"] = False
        with self.assertRaises(ValueError):
            validate_api(self.api)

    def test_wrong_integer_width_or_binding(self):
        original = copy.deepcopy(self.api)
        for mutate in ("width", "return", "hash", "static"):
            with self.subTest(mutate=mutate):
                self.api = copy.deepcopy(original)
                method = self.method("DisplayServerEmbedded", "detach_native_surface")
                if mutate == "width":
                    method["arguments"][0]["meta"] = "int32"
                elif mutate == "return":
                    method["return_value"]["type"] = "int"
                elif mutate == "hash":
                    method["hash"] = 0
                else:
                    method["is_static"] = True
                with self.assertRaises(ValueError):
                    validate_api(self.api)

    def test_legacy_hash_and_defaults(self):
        self.method("RenderingServer", "set_boot_image")["hash_compatibility"] = []
        with self.assertRaises(ValueError):
            validate_api(self.api)

    def test_editor_without_optional_vulkan_sdk(self):
        self.api["classes"] = [c for c in self.api["classes"] if c["name"] != "RenderingNativeSurfaceVulkan"]
        self.assertNotIn("RenderingNativeSurfaceVulkan.create", validate_api(self.api))


class LibraryContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.library = self.root / "libgodot.dylib"
        self.library.write_bytes(b"library fixture")
        self.symbols = self.root / "symbols.txt"
        self.symbols.write_text("0000 T _libgodot_create_godot_instance\n0010 T _libgodot_destroy_godot_instance\n")

    def test_defined_c_exports(self):
        result = library_manifest(self.library, self.symbols, "macos", "a" * 40, "template_debug", "shared_library")
        self.assertIn("libgodot_destroy_godot_instance", result["exports"])

    def test_ios_archive_requires_device_helper_definitions(self):
        exports = self.symbols.read_text()
        sdl = {"SDL_IsTablet", "SDL_IsTV", "SDL_IsIPad", "SDL_IsAppleTV"}
        for missing in (sdl, {"SDL_IsIPad"}, {"SDL_IsAppleTV"}):
            with self.subTest(missing=missing):
                self.symbols.write_text(
                    exports
                    + "".join(f"0000 T _{name}\n" for name in sorted(sdl - missing))
                    + "".join(f"         U _{name}\n" for name in sorted(missing))
                )
                with self.assertRaisesRegex(ValueError, "Missing defined iOS SDL symbols"):
                    library_manifest(
                        self.library, self.symbols, "ios", "a" * 40, "template_release", "static_library", True
                    )
        self.symbols.write_text(exports + "".join(f"0000 T _{name}\n" for name in sorted(sdl)))
        result = library_manifest(
            self.library, self.symbols, "ios", "a" * 40, "template_release", "static_library", True
        )
        self.assertEqual(result["ios_sdl_symbols"], sorted(sdl))
        self.assertEqual(result["exports"], ["libgodot_create_godot_instance", "libgodot_destroy_godot_instance"])

    def test_undefined_or_mangled_exports_fail(self):
        for symbol in ("U _libgodot_create_godot_instance", "T __Z_libgodot_create_godot_instance"):
            with self.subTest(symbol=symbol):
                self.symbols.write_text(symbol + "\n0010 T _libgodot_destroy_godot_instance\n")
                with self.assertRaises(ValueError):
                    library_manifest(self.library, self.symbols, "macos", "a" * 40, "template_debug", "shared_library")

    def test_android_uses_platform_entry_point(self):
        with self.assertRaises(ValueError):
            library_manifest(self.library, self.symbols, "android", "a" * 40, "template_release", "shared_library")
        self.symbols.write_text(
            "0000 T libgodot_create_godot_instance_android\n0010 T libgodot_destroy_godot_instance\n"
        )
        self.assertEqual(
            library_manifest(self.library, self.symbols, "android", "a" * 40, "template_release", "shared_library")[
                "platform"
            ],
            "android",
        )

    def test_cli_rejects_other_revision_or_changed_binary(self):
        command = [
            sys.executable,
            str(HELPERS / "check_library.py"),
            "--platform",
            "macos",
            "--target",
            "template_debug",
            "--library-type",
            "shared_library",
            "--library",
            str(self.library),
            "--symbols",
            str(self.symbols),
            "--manifest",
            str(self.root / "manifest.json"),
            "--revision",
            "a" * 40,
        ]
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
        self.assertEqual(subprocess.run(command + ["--verify"], capture_output=True).returncode, 0)
        command[-1] = "b" * 40
        self.assertNotEqual(subprocess.run(command + ["--verify"], capture_output=True).returncode, 0)
        command[-1] = "a" * 40
        self.library.write_bytes(b"different library")
        self.assertNotEqual(subprocess.run(command + ["--verify"], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()

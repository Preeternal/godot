"""Check LibGodot source selection without running SCons, compilers or generators."""

import ast
import unittest
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]


class SourceEnvironment(dict[str, Any]):
    def __init__(self, directory, library_type, rendering_device=True):
        super().__init__(
            library_type=library_type,
            rendering_device=rendering_device,
            APPLE_PLATFORM="ios",
            APPLE_SDK_PATH="unused",
            LIBSUFFIX=".a",
            LIBS=[],
            generate_bundle=False,
        )
        self.directory = directory
        self.drivers_sources = []
        self.servers_sources = []
        self.sources = []
        self.scripts = []
        self.swift_configured = False

    def Clone(self):
        return self

    def Append(self, **kwargs):
        pass

    def Prepend(self, **kwargs):
        pass

    def add_source_files(self, target, files):
        paths = list(self.directory.glob(files)) if isinstance(files, str) else [self.directory / f for f in files]
        # A misspelled non-wildcard string must not silently disappear through Glob.
        if isinstance(files, str) and "*" not in files:
            assert paths, files
        for path in paths:
            assert path.is_file(), path
        target.extend(paths)
        self.sources.extend(paths)

    def add_library(self, name, files):
        self.add_source_files([], files)
        return []

    def Depends(self, *args):
        pass

    def CommandNoCache(self, *args):
        return []

    def Run(self, callback):
        return callback


def collect(script, library_type, rendering_device=True):
    path = ROOT / script
    env = SourceEnvironment(path.parent, library_type, rendering_device)
    tree = ast.parse(path.read_text())
    # Execute only source-selection code with an inert SCons environment.
    tree.body = [n for n in tree.body if not isinstance(n, (ast.Import, ast.ImportFrom))]

    def sconscript(name):
        child = path.parent / name
        assert child.is_file(), child
        env.scripts.append(child)

    def swift_builder(*args):
        env.swift_configured = True
        # The 4.7 helper configures builders; it returns no module objects.
        return None

    exec(
        compile(tree, str(path), "exec"),
        {
            "env": env,
            "Import": lambda name: None,
            "SConscript": sconscript,
            "Glob": lambda pattern: list(path.parent.glob(pattern)),
            "Dir": lambda name: type("Directory", (), {"abspath": str(path.parent)})(),
            "setup_swift_builder": swift_builder,
            "combine_libs_apple_embedded": lambda: None,
            "generate_bundle": lambda: None,
        },
    )
    return env


class LibGodotSources(unittest.TestCase):
    def test_rendering_sources(self):
        device_sources = {
            "rendering_device.cpp",
            "rendering_device_binds.cpp",
            "rendering_device_commons.cpp",
            "rendering_device_driver.cpp",
            "rendering_device_graph.cpp",
            "rendering_shader_container.cpp",
        }
        for rd in (False, True):
            env = collect("servers/rendering/SCsub", "static_library", rd)
            expected = set((ROOT / "servers/rendering").glob("*.cpp"))
            if not rd:
                expected = {p for p in expected if p.name not in device_sources}
            self.assertEqual(set(env.sources), expected)

    def test_apple_library_and_executable(self):
        for kind in ("static_library", "shared_library", "executable"):
            with self.subTest(kind=kind):
                apple = collect("drivers/apple_embedded/SCsub", kind)
                ios = collect("platform/ios/SCsub", kind)
                names = {p.name for p in apple.sources + ios.sources}
                self.assertEqual(apple.swift_configured, kind == "executable")
                self.assertEqual("app.swift" in names, kind == "executable")
                self.assertEqual("main_ios.mm" in names, kind == "executable")
                self.assertEqual("godot_app_delegate.mm" in names, kind == "executable")
                self.assertEqual("libgodot_ios.mm" in names, kind != "executable")


if __name__ == "__main__":
    unittest.main()

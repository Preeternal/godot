#!/usr/bin/env python3
"""Run actual drawable query/flip methods against host-side window and EGL fixtures.

This checks C++ behavior without a mobile SDK or GPU. It does not exercise
driver initialization, framebuffer presentation, or the full engine ABI.
"""

import argparse
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def method(path, signature):
    source = (ROOT / path).read_text()
    start = source.index(signature)
    return source[start : source.index("\n}", start) + 2]


PREFIX = r"""
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct Size2i {
    int x = 0, y = 0;
    Size2i() = default;
    Size2i(int p_x, int p_y) : x(p_x), y(p_y) {}
    bool operator==(const Size2i &other) const { return x == other.x && y == other.y; }
};
namespace DisplayServerEnums { using WindowID = int; }
int errors = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "Check failed: " << #expr << '\n'; return 1; } } while (0)
#define ERR_FAIL_INDEX_V(i, n, r) do { if ((i) < 0 || (i) >= (n)) { ++errors; return r; } } while (0)
#define ERR_FAIL_COND_V(c, r) do { if (c) { ++errors; return r; } } while (0)
#define ERR_FAIL_V_MSG(r, msg) do { (void)(msg); ++errors; return r; } while (0)
template <class... Args> int vformat(const char *, Args...) { return 0; }

using EGLint = int;
constexpr int EGL_NO_SURFACE = 0, EGL_WIDTH = 1, EGL_HEIGHT = 2;
struct Drawable { int width, height; bool fail_width = false, fail_height = false; };
std::map<int, Drawable> drawables;
int queries = 0;
bool eglQuerySurface(int display, int surface, int attr, EGLint *value) {
    assert(display == 7);
    ++queries;
    const Drawable &drawable = drawables.at(surface);
    if (attr == EGL_WIDTH && !drawable.fail_width) { *value = drawable.width; return true; }
    if (attr == EGL_HEIGHT && !drawable.fail_height) { *value = drawable.height; return true; }
    return false;
}
EGLint eglGetError() { return 0x300d; }
const char *getEGLErrorMessage(EGLint) { return "fixture EGL_BAD_SURFACE"; }
class EGLManager {
public:
    struct GLWindow { bool initialized = true; int gldisplay_id = 0; int egl_surface = 11; };
    struct GLDisplay { int egl_display = 7; };
    std::vector<GLWindow> windows{GLWindow{}, GLWindow{true, 0, 12}};
    std::vector<GLDisplay> displays{GLDisplay{}};
    Size2i window_get_size(DisplayServerEnums::WindowID p_window_id) const;
};

struct Layer { struct { struct { double width = 322, height = 602; } size; } bounds; };
struct WindowData { int backingWidth = 966, backingHeight = 1806; Layer layer; };
struct Windows : std::map<int, WindowData> {
    bool has(int id) const { return count(id) != 0; }
    const WindowData &operator[](int id) const { return at(id); }
};
class GLManagerApple {
public:
    Windows windows;
    Size2i window_get_size(DisplayServerEnums::WindowID p_id) const;
};
class DisplayServerEmbedded {
public:
    std::string rendering_driver;
    bool is_rendering_flipped() const;
};
"""

MAIN = r"""
int main() {
    DisplayServerEmbedded embedded;
    for (const char *driver : {"opengl3", "opengl3_angle", "metal", "vulkan", "dummy"}) {
        embedded.rendering_driver = driver;
        bool expected = false;
#if defined(IOS_ENABLED) || defined(ANDROID_ENABLED)
        expected = std::string(driver) == "opengl3";
#endif
        CHECK(embedded.is_rendering_flipped() == expected);
    }

#if defined(IOS_ENABLED) || defined(MACOS_ENABLED)
    Layer layer;
    GLManagerApple apple;
    // Different point/pixel dimensions catch accidental scale omission and double scaling.
    apple.windows.emplace(0, WindowData{966, 1806, layer});
    apple.windows.emplace(4, WindowData{640, 480, layer});
    for (int scale : {1, 2, 3}) {
        apple.windows.at(0).backingWidth = 322 * scale;
        apple.windows.at(0).backingHeight = 602 * scale;
        CHECK(apple.window_get_size(0) == Size2i(322 * scale, 602 * scale));
        CHECK(apple.window_get_size(4) == Size2i(640, 480));
    }
    apple.windows.erase(0);
    CHECK(apple.window_get_size(0) == Size2i());
    apple.windows.emplace(0, WindowData{480, 640, layer});
    CHECK(apple.window_get_size(0) == Size2i(480, 640));
#endif

#if defined(ANDROID_ENABLED)
    EGLManager egl;
    drawables.emplace(11, Drawable{1076, 805});
    drawables.emplace(12, Drawable{320, 240});
    CHECK(egl.window_get_size(0) == Size2i(1076, 805));
    CHECK(egl.window_get_size(1) == Size2i(320, 240));
    // Resize changes the drawable, not a cached requested size or current-window pointer.
    drawables.at(11) = Drawable{805, 1076};
    CHECK(egl.window_get_size(0) == Size2i(805, 1076));
    const int before = queries;
    CHECK(egl.window_get_size(-1) == Size2i());
    CHECK(egl.window_get_size(9) == Size2i());
    egl.windows.at(0).initialized = false;
    CHECK(egl.window_get_size(0) == Size2i());
    egl.windows.at(0).initialized = true;
    egl.windows.at(0).egl_surface = EGL_NO_SURFACE;
    CHECK(egl.window_get_size(0) == Size2i());
    egl.windows.at(0).egl_surface = 11;
    egl.windows.at(0).gldisplay_id = -1;
    CHECK(egl.window_get_size(0) == Size2i());
    CHECK(queries == before);
    egl.windows.at(0).gldisplay_id = 0;
    drawables.at(11).fail_width = true;
    CHECK(egl.window_get_size(0) == Size2i());
    drawables.at(11).fail_width = false;
    drawables.at(11).fail_height = true;
    CHECK(egl.window_get_size(0) == Size2i());
    drawables.at(11).fail_height = false;
    // Reattach with a different native surface must immediately use its dimensions.
    egl.windows.at(0).egl_surface = 12;
    CHECK(egl.window_get_size(0) == Size2i(320, 240));
    CHECK(errors == 7);
#endif
    std::cout << "drawable contract PASS\n";
}
"""


class DrawableContract(unittest.TestCase):
    def check_platform(self, platform):
        source = (
            PREFIX
            + "\n".join((
                method("drivers/egl/egl_manager.cpp", "Size2i EGLManager::window_get_size("),
                method("drivers/apple/rendering_native_surface_apple.mm", "Size2i GLManagerApple::window_get_size("),
                method(
                    "servers/display/display_server_embedded.cpp", "bool DisplayServerEmbedded::is_rendering_flipped("
                ),
            ))
            + MAIN
        )
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        with tempfile.TemporaryDirectory(prefix="libgodot-drawables-") as directory:
            path = Path(directory)
            (path / "probe.cpp").write_text(source)
            command = compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror"]
            if platform:
                command.append(f"-D{platform}_ENABLED")
            command += [str(path / "probe.cpp"), "-o", str(path / "probe")]
            subprocess.run(command, check=True)
            result = subprocess.run([str(path / "probe")], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "drawable contract PASS\n")

    def test_ios_drawable_pixels_and_native_gl_flip(self):
        self.check_platform("IOS")

    def test_android_surface_queries_and_native_gl_flip(self):
        self.check_platform("ANDROID")

    def test_macos_flip_and_pixel_size_unchanged(self):
        self.check_platform("MACOS")

    def test_other_platform_flip_unchanged(self):
        self.check_platform("")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args, remaining = parser.parse_known_args()
    ROOT = args.source_root.resolve()
    unittest.main(argv=[__file__, *remaining])

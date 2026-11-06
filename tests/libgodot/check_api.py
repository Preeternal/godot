"""Validate the embedding contract in a macOS editor's GDExtension API dump."""

import argparse
import json
from pathlib import Path
from typing import Any, Optional

# Method hashes also used by the C ABI host and existing native surface callers.
# Argument metadata matters: int32/int64/uint64 all have JSON type "int".
MethodContract = tuple[int, Optional[str], list[tuple[str, Optional[str]]], bool]
CONTRACT: dict[str, dict[str, MethodContract]] = {
    "GodotInstance": {
        **{name: (2240911060, "bool", [], False) for name in ("start", "is_started", "iteration")},
        **{name: (3218959716, None, [], False) for name in ("stop", "focus_in", "focus_out", "pause", "resume")},
    },
    "DisplayServerEmbedded": {
        "set_native_surface": (2894024005, None, [("RenderingNativeSurface", None)], True),
        "detach_native_surface": (3067735520, "bool", [("int", "int64")], False),
    },
    "Window": {"set_native_surface": (2894024005, None, [("RenderingNativeSurface", None)], False)},
    "RenderingNativeSurfaceApple": {"create": (4233771684, "RenderingNativeSurfaceApple", [("int", "uint64")], True)},
    "RenderingNativeSurfaceVulkan": {"create": (1056151817, "RenderingNativeSurfaceVulkan", [("int", "uint64")], True)},
}
OWNERSHIP = {
    "GodotInstance": ("Object", False),
    "RenderingNativeSurface": ("RefCounted", True),
    "RenderingNativeSurfaceApple": ("RenderingNativeSurface", True),
    "RenderingNativeSurfaceVulkan": ("RenderingNativeSurface", True),
}


def validate_api(api: dict[str, Any]) -> list[str]:
    if (api["header"]["version_major"], api["header"]["version_minor"]) != (4, 7):
        raise ValueError("Expected a Godot 4.7 API dump")
    classes = {entry["name"]: entry for entry in api["classes"]}
    if "RenderingNativeSurface" not in classes:
        raise ValueError("RenderingNativeSurface is missing")
    for name, ownership in OWNERSHIP.items():
        if name == "RenderingNativeSurfaceVulkan" and name not in classes:
            continue
        if name not in classes or (classes[name]["inherits"], classes[name]["is_refcounted"]) != ownership:
            raise ValueError(f"{name}: unexpected base class or reference-counted ownership")
    checked = []
    for class_name, methods in CONTRACT.items():
        # Upstream macOS editor permits a missing optional Vulkan SDK. The library
        # job always builds Vulkan with Volk and its runtime host requires this bind.
        if class_name == "RenderingNativeSurfaceVulkan" and class_name not in classes:
            continue
        if class_name not in classes:
            raise ValueError(f"{class_name} is missing")
        actual_methods = {entry["name"]: entry for entry in classes[class_name].get("methods", [])}
        for name, (method_hash, result, arguments, is_static) in methods.items():
            entry = actual_methods.get(name)
            if entry is None:
                raise ValueError(f"{class_name}.{name} is missing")
            actual_arguments = [(arg["type"], arg.get("meta")) for arg in entry.get("arguments", [])]
            actual = (entry["hash"], entry.get("return_value", {}).get("type"), actual_arguments, entry["is_static"])
            expected = (method_hash, result, arguments, is_static)
            if actual != expected:
                raise ValueError(f"{class_name}.{name}: expected {expected}, got {actual}")
            checked.append(f"{class_name}.{name}")

    rendering_methods = {entry["name"]: entry for entry in classes["RenderingServer"]["methods"]}
    for name, old_hashes in (
        ("set_boot_image", {3759744527, 2244367877}),
        ("set_boot_image_with_stretch", {1104470771}),
    ):
        entry = rendering_methods[name]
        if not old_hashes.issubset(entry.get("hash_compatibility", [])):
            raise ValueError(f"RenderingServer.{name}: missing legacy method hashes")
        arguments = entry["arguments"]
        if (
            len(arguments) != 5
            or arguments[3].get("default_value") != "0"
            or arguments[4].get("default_value") != "true"
        ):
            raise ValueError(f"RenderingServer.{name}: invalid screen/filter defaults")
        checked.append(f"RenderingServer.{name}")
    return checked


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("api", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        checked = validate_api(json.loads(args.api.read_text()))
    except (KeyError, ValueError) as error:
        parser.exit(1, f"LibGodot API contract failed: {error}\n")
    result = {"checked_methods": checked, "vulkan_in_editor_dump": "RenderingNativeSurfaceVulkan.create" in checked}
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"LibGodot API contract: PASS ({len(checked)} methods)")


if __name__ == "__main__":
    main()

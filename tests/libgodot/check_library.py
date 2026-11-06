"""Record or verify the revision, digest and public C exports of a CI library."""

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any


def library_manifest(
    library: Path,
    symbols: Path,
    platform: str,
    revision: str,
    target: str,
    library_type: str,
    require_ios_sdl: bool = False,
) -> dict[str, Any]:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Expected a full Git commit SHA")
    if not library.is_file() or library.stat().st_size == 0:
        raise ValueError(f"Library is missing or empty: {library}")
    create = "libgodot_create_godot_instance_android" if platform == "android" else "libgodot_create_godot_instance"
    required = {create, "libgodot_destroy_godot_instance"}
    defined = set()
    for line in symbols.read_text().splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[-2] == "T":
            defined.add(parts[-1].removeprefix("_"))
    if missing := required - defined:
        raise ValueError(f"Missing defined C exports: {sorted(missing)}")
    sdl_symbols = {"SDL_IsTablet", "SDL_IsTV", "SDL_IsIPad", "SDL_IsAppleTV"}
    if require_ios_sdl:
        if platform != "ios":
            raise ValueError("The iOS SDL check requires an iOS library")
        if missing := sdl_symbols - defined:
            raise ValueError(f"Missing defined iOS SDL symbols: {sorted(missing)}")
    digest = hashlib.sha256()
    with library.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    result = {
        "revision": revision,
        "platform": platform,
        "architecture": "arm64",  # Checked with lipo/readelf by the build job.
        "target": target,
        "library_type": library_type,
        "file": library.name,
        "bytes": library.stat().st_size,
        "sha256": digest.hexdigest(),
        "exports": sorted(required),
    }
    if require_ios_sdl:
        result["ios_sdl_symbols"] = sorted(sdl_symbols)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("macos", "ios", "android"), required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--library-type", choices=("static_library", "shared_library"), required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--require-ios-sdl", action="store_true")
    args = parser.parse_args()
    try:
        result = library_manifest(
            args.library,
            args.symbols,
            args.platform,
            args.revision,
            args.target,
            args.library_type,
            args.require_ios_sdl,
        )
        if args.verify:
            if json.loads(args.manifest.read_text()) != result:
                raise ValueError("Library manifest does not match this revision, digest or build profile")
        else:
            args.manifest.write_text(json.dumps(result, indent=2) + "\n")
    except (OSError, ValueError) as error:
        parser.exit(1, f"LibGodot library check failed: {error}\n")
    print(f"LibGodot {args.platform} library: PASS ({result['sha256']})")


if __name__ == "__main__":
    main()

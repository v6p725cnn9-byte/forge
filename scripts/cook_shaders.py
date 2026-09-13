#!/usr/bin/env python3
"""Cook HLSL through SDL_shadercross, or verify the checked-in outputs without a compiler."""

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"
COOKED = SHADERS / "cooked"
STAGES = {
    "unlit.vert": {"stage": "vertex", "uniform_buffers": 1, "samplers": 0},
    "unlit.frag": {"stage": "fragment", "uniform_buffers": 0, "samplers": 0},
    "pbr.vert": {"stage": "vertex", "uniform_buffers": 1, "samplers": 0},
    "pbr_instanced.vert": {"stage": "vertex", "uniform_buffers": 1, "samplers": 0},
    "pbr_skinned.vert": {"stage": "vertex", "uniform_buffers": 2, "samplers": 0},
    "pbr.frag": {"stage": "fragment", "uniform_buffers": 1, "samplers": 9},
    "shadow.vert": {"stage": "vertex", "uniform_buffers": 1, "samplers": 0},
    "shadow.frag": {"stage": "fragment", "uniform_buffers": 0, "samplers": 0},
    "tonemap.vert": {"stage": "vertex", "uniform_buffers": 0, "samplers": 0},
    "tonemap.frag": {"stage": "fragment", "uniform_buffers": 1, "samplers": 2},
    "bloom.frag": {"stage": "fragment", "uniform_buffers": 1, "samplers": 1},
}
FORMATS = {"msl": "MSL", "spv": "SPIRV", "dxil": "DXIL", "json": "JSON"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    return {str(path.relative_to(ROOT)): digest(path)
            for path in sorted(SHADERS.glob("*.hlsl"))} | {"scripts/cook_shaders.py": digest(pathlib.Path(__file__))}


def verify():
    try:
        manifest = json.loads((COOKED / "manifest.json").read_text())
        if manifest["inputs"] != inputs():
            raise ValueError("HLSL sources or cook recipe changed")
        expected = {name + "." + ext for name in STAGES for ext in FORMATS}
        if set(manifest["outputs"]) != expected:
            raise ValueError("incomplete output manifest")
        for name, sha in manifest["outputs"].items():
            if digest(COOKED / name) != sha:
                raise ValueError("output changed: " + name)
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"Stale/missing cooked shaders: {error}. Run python3 scripts/cook_shaders.py")
    print("Cooked shaders verified (MSL, SPIR-V, DXIL)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--shadercross", help="Path to the SDL_shadercross CLI")
    args = parser.parse_args()
    if args.check:
        verify()
        return
    suffix = ".exe" if os.name == "nt" else ""
    compiler = args.shadercross or os.environ.get("FORGE_SHADERCROSS")
    if not compiler:
        local = ROOT / (".cache/shadercross-build/shadercross" + suffix)
        compiler = str(local) if local.is_file() else shutil.which("shadercross")
    if not compiler:
        raise SystemExit("SDL_shadercross not found. Run python3 scripts/build_shadercross.py or pass --shadercross.")
    COOKED.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="forge-shaders-") as temporary:
        output = pathlib.Path(temporary)
        for name, spec in STAGES.items():
            for extension, format_name in FORMATS.items():
                subprocess.run([compiler, str(SHADERS / (name + ".hlsl")), "-s", "HLSL",
                                "-d", format_name, "-t", spec["stage"], "-e", "main",
                                "-o", str(output / (name + "." + extension))], check=True)
            reflection = json.loads((output / (name + ".json")).read_text())
            expected = {"uniform_buffers": spec["uniform_buffers"], "samplers": spec["samplers"],
                        "storage_buffers": 0, "storage_textures": 0}
            if any(reflection.get(key) != value for key, value in expected.items()):
                raise SystemExit(f"{name}: resource layout {reflection} != {expected}")
        manifest = {"schema": 1, "compiler": "SDL_shadercross", "inputs": inputs(),
                    "outputs": {path.name: digest(path) for path in sorted(output.iterdir())}}
        for path in output.iterdir():
            shutil.copyfile(path, COOKED / path.name)
        (COOKED / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    verify()


if __name__ == "__main__":
    main()

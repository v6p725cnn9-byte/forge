#!/usr/bin/env python3
"""Build the pinned offline shader compiler locally; never installed system-wide."""

import argparse
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
REVISION = "1ff05bec573988a98ef9e0260b4da44f512b8367"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=6)
    args = parser.parse_args()
    source = ROOT / ".cache/SDL_shadercross"
    build = ROOT / ".cache/shadercross-build"
    if not source.exists():
        subprocess.run(["git", "clone", "https://github.com/libsdl-org/SDL_shadercross.git", str(source)], check=True)
    subprocess.run(["git", "-C", str(source), "checkout", "--detach", REVISION], check=True)
    subprocess.run(["git", "-C", str(source), "submodule", "update", "--init", "--recursive", "--depth", "1"], check=True)
    subprocess.run([
        "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=/opt/homebrew;/usr/local",
        "-DSDLSHADERCROSS_VENDORED=ON", "-DSDLSHADERCROSS_SPIRVCROSS_SHARED=OFF",
        "-DSDLSHADERCROSS_TESTS=OFF", "-DSDLSHADERCROSS_INSTALL=OFF",
        "-DSPIRV_SKIP_TESTS=ON", "-DSPIRV_SKIP_EXECUTABLES=ON",
    ], check=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "shadercross", "--parallel", str(args.jobs)], check=True)
    print("Shader compiler ready in", build)


if __name__ == "__main__":
    main()

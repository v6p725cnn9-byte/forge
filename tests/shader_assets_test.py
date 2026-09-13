"""A build must reject stale HLSL, missing bytecode and tampered bytecode."""

import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="forge-shader-check-") as temporary:
    fixture = pathlib.Path(temporary)
    shutil.copytree(ROOT / "shaders", fixture / "shaders")
    (fixture / "scripts").mkdir()
    shutil.copyfile(ROOT / "scripts/cook_shaders.py", fixture / "scripts/cook_shaders.py")

    def check(expected):
        result = subprocess.run([sys.executable, str(fixture / "scripts/cook_shaders.py"), "--check"],
                                capture_output=True, text=True)
        if (result.returncode == 0) != expected:
            raise AssertionError(result.stdout + result.stderr)

    check(True)
    source = fixture / "shaders/pbr.vert.hlsl"
    original = source.read_bytes()
    source.write_bytes(original + b"\n// edited\n")
    check(False)
    source.write_bytes(original)
    shader = fixture / "shaders/cooked/pbr.vert.spv"
    original = shader.read_bytes()
    shader.write_bytes(b"bad shader")
    check(False)
    shader.unlink()
    check(False)
    shader.write_bytes(original)
    check(True)
print("Stale and damaged shader rejection passed")

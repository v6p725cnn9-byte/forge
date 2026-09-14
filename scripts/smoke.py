#!/usr/bin/env python3
"""Exercise GPU rendering and resize, then check startup failures. Requires a desktop session."""

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    environment = dict(os.environ, FORGE_SMOKE="90", FORGE_SMOKE_RESIZE="1")
    with tempfile.TemporaryDirectory(prefix="forge-smoke-") as temporary:
        for key in ("FORGE_CONNECT", "FORGE_PORT", "FORGE_SMOKE_MENU", "FORGE_SMOKE_INVENTORY"):
            environment.pop(key, None)
        environment["FORGE_SETTINGS"] = str(pathlib.Path(temporary) / "settings.cfg")
        def run(binary, env, success):
            result = subprocess.run([str(binary)], env=env, cwd=temporary, capture_output=True, text=True, timeout=30)
            output = result.stdout + result.stderr
            print(output, end="")
            if (result.returncode == 0) != success:
                raise SystemExit(f"Unexpected exit code {result.returncode}")
            return output

        output = run(executable, environment, True)
        if "Smoke passed: 90 frames presented" not in output:
            raise SystemExit("Smoke did not finish all frames")
        targets = re.findall(r"Render targets: (\d+x\d+) pixels", output)
        if len(targets) < 3 or targets[0] == targets[1] or targets[0] != targets[-1]:
            raise SystemExit("Resize/restore did not recreate render targets")
        for marker in ("failed:", "Validation Error", "validateRenderPassDescriptor", "Assertion failed"):
            if marker in output:
                raise SystemExit("GPU validation failure: " + marker)
        if executable.name in ("forge", "forge_survival", "forge.exe", "forge_survival.exe"):
            output = run(executable, dict(environment, FORGE_SMOKE_MENU="1"), True)
            if "Menu scene: vista" not in output or "Smoke passed: 90 frames presented" not in output:
                raise SystemExit("3D menu smoke did not finish")
            output = run(executable, dict(environment, FORGE_SMOKE_INVENTORY="1"), True)
            if "Smoke passed: 90 frames presented" not in output:
                raise SystemExit("Inventory/crafting smoke did not finish")
            run(executable, dict(environment, FORGE_CONNECT="invalid"), False)
        run(executable, dict(environment, FORGE_SMOKE="invalid"), False)
        # A relocated executable without assets must fail, even when the source tree exists.
        isolated = pathlib.Path(temporary) / executable.name
        shutil.copyfile(executable, isolated)
        isolated.chmod(0o755)
        for library in executable.parent.glob("*.dll"):
            shutil.copyfile(library, pathlib.Path(temporary) / library.name)
        output = run(isolated, environment, False)
        if "No complete cooked shader set" not in output:
            raise SystemExit("Missing-asset startup did not report its cause")
    print("GPU smoke, resize, invalid input and missing-asset checks passed")


if __name__ == "__main__":
    main()

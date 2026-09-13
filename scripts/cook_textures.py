#!/usr/bin/env python3
"""Cook the Flight Helmet texture recipe with Khronos toktx, preserving XYZ normals."""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODEL = ROOT / 'assets/models/FlightHelmet'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inputs():
    return {p.name: digest(p) for p in sorted(MODEL.glob('*.png'))} | {'recipe': digest(pathlib.Path(__file__))}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toktx')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    manifest = MODEL / 'textures.json'
    if args.check:
        try:
            data = json.loads(manifest.read_text())
            assert data['inputs'] == inputs(), 'source or recipe changed'
            assert set(data['outputs']) == {p.with_suffix('.ktx2').name for p in MODEL.glob('*.png')}, 'incomplete texture set'
            for name, sha in data['outputs'].items():
                assert digest(MODEL / name) == sha, name + ' changed'
        except (OSError, ValueError, KeyError, AssertionError) as e:
            raise SystemExit(f'Stale/missing texture cook: {e}. Run python3 scripts/cook_textures.py')
        print('KTX2 texture cook verified')
        return
    compiler = args.toktx or shutil.which('toktx') or str(ROOT / '.cache/ktx-build/Release/toktx')
    with tempfile.TemporaryDirectory(prefix='forge-texture-cook-') as temporary:
        stage = pathlib.Path(temporary)
        for source in sorted(MODEL.glob('*.png')):
            name = source.stem
            srgb = name.endswith('_BaseColor')
            normal = name.endswith('_Normal')
            command = [compiler, '--t2', '--genmipmap', '--encode', 'uastc', '--uastc_quality', '2',
                       '--zcmp', '9', '--threads', '4', '--assign_oetf', 'srgb' if srgb else 'linear',
                       '--target_type', 'RGBA']
            if normal:
                command += ['--normal_mode', '--normalize', '--input_swizzle', 'rgb1']
            command += [str(stage / (name + '.ktx2')), str(source)]
            env = dict(os.environ)
            env.pop('TOKTX_OPTIONS', None)
            subprocess.run(command, env=env, check=True)
            print(name, flush=True)
        data = {'inputs': inputs(), 'outputs': {p.name: digest(p) for p in sorted(stage.iterdir())}}
        for path in stage.iterdir():
            shutil.copyfile(path, MODEL / path.name)
        manifest.write_text(json.dumps(data, indent=2)+'\n')

if __name__ == '__main__':
    main()

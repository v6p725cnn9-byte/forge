#!/usr/bin/env python3
"""Repair scene roots in Kenney Nature Kit 2.1 exports using pygltflib.

The original meshes, buffers, materials and transforms are preserved. Install
pygltflib in your tooling environment before running this optional import step.
"""

import pathlib

from pygltflib import GLTF2


def normalize(path):
    model = GLTF2().load(str(path))
    parents = {child: index for index, node in enumerate(model.nodes) for child in node.children}
    for scene in model.scenes:
        roots = []
        for index in scene.nodes:
            visited = set()
            while index in parents:
                if index in visited:
                    raise ValueError(f"Cyclic node hierarchy: {path}")
                visited.add(index)
                index = parents[index]
            if index not in roots:
                roots.append(index)
        scene.nodes = roots
    model.save_binary(str(path))
    print(f"Normalized scene roots: {path.name}")


if __name__ == "__main__":
    root = pathlib.Path(__file__).resolve().parents[1]
    for path in sorted((root / "assets/models/Nature").glob("*.glb")):
        normalize(path)

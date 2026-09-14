"""Attach the supplied retargeted walk to the astronaut by bone name (pygltflib)."""
import copy
import pathlib
from pygltflib import GLTF2

ROOT = pathlib.Path(__file__).resolve().parents[1]
folder = ROOT / 'assets/models/Astronaut'
model = GLTF2().load(str(folder / 'Astronaut.glb'))
motion = GLTF2().load(str(folder / 'Walk.glb'))
names = {node.name: i for i, node in enumerate(model.nodes)}
blob = model.binary_blob()
blob += b'\0' * (-len(blob) % 4)
offset = len(blob)
view_base, accessor_base = len(model.bufferViews), len(model.accessors)
for original in motion.bufferViews:
    view = copy.deepcopy(original)
    view.buffer = 0
    view.byteOffset = (view.byteOffset or 0) + offset
    model.bufferViews.append(view)
for original in motion.accessors:
    accessor = copy.deepcopy(original)
    if accessor.bufferView is not None:
        accessor.bufferView += view_base
    if accessor.sparse:
        raise ValueError('Sparse animation accessor is not supported by this import step')
    model.accessors.append(accessor)
clip = copy.deepcopy(motion.animations[0])
clip.name = 'Walk'
for sampler in clip.samplers:
    sampler.input += accessor_base
    sampler.output += accessor_base
for channel in clip.channels:
    channel.target.node = names[motion.nodes[channel.target.node].name]
model.animations = [clip]
blob += motion.binary_blob()
model.buffers[0].byteLength = len(blob)
model.set_binary_blob(blob)
model.save_binary(str(folder / 'Astronaut.glb'))
print(f'Imported Walk: {len(clip.channels)} channels mapped by node name')

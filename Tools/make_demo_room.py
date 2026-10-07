"""Create a tiny GLB room to smoke-test meter scale, collision, and winding."""
import json
import struct
from pathlib import Path

vertices = []
normals = []
indices = []


def quad(a, b, c, d, normal):
    start = len(vertices)
    vertices.extend((a, b, c, d))
    normals.extend((normal,) * 4)
    indices.extend((start, start + 1, start + 2, start, start + 2, start + 3))


# glTF: X = right, Y = up, Z = depth. Front is open.
quad((-2, 0, -2), (-2, 0, 2), (2, 0, 2), (2, 0, -2), (0, 1, 0))
quad((-2, 0, 2), (-2, 2.8, 2), (2, 2.8, 2), (2, 0, 2), (0, 0, -1))
quad((-2, 0, -2), (-2, 2.8, -2), (-2, 2.8, 2), (-2, 0, 2), (1, 0, 0))
quad((2, 0, 2), (2, 2.8, 2), (2, 2.8, -2), (2, 0, -2), (-1, 0, 0))


def packed_vec3(items):
    return b"".join(struct.pack("<fff", *item) for item in items)


position_data = packed_vec3(vertices)
normal_data = packed_vec3(normals)
index_data = struct.pack("<" + "H" * len(indices), *indices)
binary = position_data + normal_data + index_data
binary += b"\0" * (-len(binary) % 4)
document = {
    "asset": {"version": "2.0", "generator": "HouseSandbox demo"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "Room"}],
    "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2}]}],
    "buffers": [{"byteLength": len(binary)}],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(position_data)},
        {"buffer": 0, "byteOffset": len(position_data), "byteLength": len(normal_data)},
        {"buffer": 0, "byteOffset": len(position_data) + len(normal_data), "byteLength": len(index_data)},
    ],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": len(vertices), "type": "VEC3"},
        {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
    ],
}
encoded_json = json.dumps(document, separators=(",", ":")).encode()
encoded_json += b" " * (-len(encoded_json) % 4)
length = 12 + 8 + len(encoded_json) + 8 + len(binary)
glb = (struct.pack("<III", 0x46546C67, 2, length) +
       struct.pack("<II", len(encoded_json), 0x4E4F534A) + encoded_json +
       struct.pack("<II", len(binary), 0x004E4942) + binary)
target = Path(__file__).resolve().parent / "models" / "demo_room.glb"
target.parent.mkdir(exist_ok=True)
target.write_bytes(glb)
print(target, len(glb), "bytes", len(indices) // 3, "triangles")

#include "whitehole/render/model_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace whitehole::render {
namespace {

// Per-vertex attribute fetch. Missing attributes stay at their defaults so a
// batch without normals or texture coordinates still produces triangles.
bool fetchVertex(const smg::BmdModel& model, const smg::BmdPrimitive& primitive, std::size_t vertexIndex,
                 ModelVertex& out) {
    if (vertexIndex >= primitive.positionIndices.size()) {
        return false;
    }
    const auto positionIndex = static_cast<std::size_t>(primitive.positionIndices[vertexIndex]);
    if (positionIndex >= model.positions.size()) {
        return false;
    }
    out.position = model.positions[positionIndex];

    if (vertexIndex < primitive.normalIndices.size()) {
        const auto normalIndex = static_cast<std::size_t>(primitive.normalIndices[vertexIndex]);
        if (normalIndex < model.normals.size()) {
            out.normal = model.normals[normalIndex];
        }
    }

    const auto& texcoordIndices = primitive.texcoordIndices[0];
    if (vertexIndex < texcoordIndices.size()) {
        const auto texcoordIndex = static_cast<std::size_t>(texcoordIndices[vertexIndex]);
        if (texcoordIndex < model.texcoords[0].size()) {
            const auto& uv = model.texcoords[0][texcoordIndex];
            out.texCoord = {uv.x, uv.y};
        }
    }
    return true;
}

math::Vec3f faceNormal(const ModelVertex& a, const ModelVertex& b, const ModelVertex& c) {
    const math::Vec3f normal = math::Vec3f::cross(b.position - a.position, c.position - a.position);
    return normal.length() < 0.000001F ? math::Vec3f{0.0F, 1.0F, 0.0F} : normal.normalized();
}

// Emits one triangle. Vertices without normals fall back to the face normal so
// flat-shaded previews stay readable instead of turning black. Out-of-range
// indices drop the triangle rather than clamping to a wrong vertex.
void pushTriangle(ModelMesh& mesh, const smg::BmdModel& model, const smg::BmdPrimitive& primitive,
                  std::size_t first, std::size_t second, std::size_t third, const std::array<float, 4>& color,
                  std::int32_t materialIndex) {
    ModelTriangle triangle;
    if (!fetchVertex(model, primitive, first, triangle.a) || !fetchVertex(model, primitive, second, triangle.b) ||
        !fetchVertex(model, primitive, third, triangle.c)) {
        return;
    }
    const math::Vec3f normal = faceNormal(triangle.a, triangle.b, triangle.c);
    ModelVertex* vertices[3] = {&triangle.a, &triangle.b, &triangle.c};
    for (ModelVertex* vertex : vertices) {
        if (vertex->normal.length() < 0.000001F) {
            vertex->normal = normal;
        }
    }
    triangle.color = color;
    triangle.materialIndex = materialIndex;
    mesh.triangles.push_back(triangle);
}

// Converts one primitive's vertex stream to triangles. GX draw lists are
// triangles, strips, fans or quads; everything else is a line or point list,
// which a solid preview has nothing to draw for.
void convertPrimitive(ModelMesh& mesh, const smg::BmdModel& model, const smg::BmdPrimitive& primitive,
                      const std::array<float, 4>& color, std::int32_t materialIndex) {
    using smg::BmdPrimitiveType;

    const std::size_t count = primitive.positionIndices.size();
    switch (static_cast<BmdPrimitiveType>(primitive.type)) {
        case BmdPrimitiveType::Triangles:
            for (std::size_t index = 0; index + 2 < count; index += 3) {
                pushTriangle(mesh, model, primitive, index, index + 1, index + 2, color, materialIndex);
            }
            break;
        case BmdPrimitiveType::TriangleStrip:
            for (std::size_t index = 0; index + 2 < count; ++index) {
                // GX strips alternate winding; the vertex order is preserved so
                // the triangle list matches what the game draws.
                pushTriangle(mesh, model, primitive, index, index + 1, index + 2, color, materialIndex);
            }
            break;
        case BmdPrimitiveType::TriangleFan:
            for (std::size_t index = 1; index + 1 < count; ++index) {
                pushTriangle(mesh, model, primitive, 0, index, index + 1, color, materialIndex);
            }
            break;
        case BmdPrimitiveType::Quads:
            for (std::size_t index = 0; index + 3 < count; index += 4) {
                pushTriangle(mesh, model, primitive, index, index + 1, index + 2, color, materialIndex);
                pushTriangle(mesh, model, primitive, index, index + 2, index + 3, color, materialIndex);
            }
            break;
        default:
            mesh.skippedPrimitives++;
            break;
    }
}

// Diffuse colour of a scene-graph node's material, white when it has none.
std::array<float, 4> materialColor(const smg::BmdModel& model, std::int16_t materialIndex) {
    if (materialIndex < 0 || static_cast<std::size_t>(materialIndex) >= model.materials.size()) {
        return {1.0F, 1.0F, 1.0F, 1.0F};
    }
    return model.materials[static_cast<std::size_t>(materialIndex)].diffuseColor;
}

} // namespace

ModelMesh buildModelMesh(const smg::BmdModel& model) {
    ModelMesh mesh;

    // The INF1 scene graph decides what is drawn and with which material; the
    // same batch can be referenced by several nodes, which is how BMD instancing
    // works, so every node contributes its own triangles.
    for (const auto& node : model.sceneGraph) {
        if (node.nodeType != 0) {
            continue; // joint nodes carry no geometry
        }
        const auto batchIndex = static_cast<std::size_t>(node.nodeId);
        if (batchIndex >= model.batches.size()) {
            continue;
        }
        const auto& batch = model.batches[batchIndex];
        const std::array<float, 4> color = materialColor(model, node.materialIndex);
        for (const auto& packet : batch.packets) {
            for (const auto& primitive : packet.primitives) {
                convertPrimitive(mesh, model, primitive, color, node.materialIndex);
            }
        }
    }

    if (!mesh.triangles.empty()) {
        mesh.boundsMin = mesh.triangles.front().a.position;
        mesh.boundsMax = mesh.triangles.front().a.position;
        for (const auto& triangle : mesh.triangles) {
            const ModelVertex* vertices[3] = {&triangle.a, &triangle.b, &triangle.c};
            for (const ModelVertex* vertex : vertices) {
                mesh.boundsMin = {std::min(mesh.boundsMin.x, vertex->position.x),
                                  std::min(mesh.boundsMin.y, vertex->position.y),
                                  std::min(mesh.boundsMin.z, vertex->position.z)};
                mesh.boundsMax = {std::max(mesh.boundsMax.x, vertex->position.x),
                                  std::max(mesh.boundsMax.y, vertex->position.y),
                                  std::max(mesh.boundsMax.z, vertex->position.z)};
            }
        }
        mesh.radius = (mesh.boundsMax - mesh.boundsMin).length() * 0.5F;
    }

    return mesh;
}

} // namespace whitehole::render
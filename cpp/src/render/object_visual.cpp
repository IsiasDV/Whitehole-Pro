#include "whitehole/render/object_visual.hpp"

#include <array>
#include <cmath>

namespace whitehole::render {
namespace {

// Category visuals. Order must match the ObjectCategory enum.
constexpr std::array<CategoryStyle, 10> kStyles{{
    {ObjectCategory::Player,   "Player",   "cylinder",   {0.30F, 0.90F, 0.45F}, CategoryStyle::Shape::Cylinder},
    {ObjectCategory::Enemy,    "Enemy",    "pyramid",    {0.95F, 0.30F, 0.25F}, CategoryStyle::Shape::Pyramid},
    {ObjectCategory::Item,     "Item",     "sphere",     {1.00F, 0.80F, 0.20F}, CategoryStyle::Shape::Sphere},
    {ObjectCategory::Terrain,  "Terrain",  "cube",       {0.55F, 0.65F, 0.80F}, CategoryStyle::Shape::Cube},
    {ObjectCategory::Camera,   "Camera",   "octahedron", {0.40F, 0.75F, 1.00F}, CategoryStyle::Shape::Octahedron},
    {ObjectCategory::Gravity,  "Gravity",  "octahedron", {0.60F, 0.45F, 0.95F}, CategoryStyle::Shape::Octahedron},
    {ObjectCategory::Zone,     "Zone",     "cube",       {0.95F, 0.55F, 0.90F}, CategoryStyle::Shape::Cube},
    {ObjectCategory::MapPart,  "Map part", "cube",       {0.35F, 0.85F, 0.85F}, CategoryStyle::Shape::Cube},
    {ObjectCategory::Cutscene, "Cutscene", "octahedron", {0.90F, 0.60F, 0.35F}, CategoryStyle::Shape::Octahedron},
    {ObjectCategory::Misc,     "Object",   "cube",       {0.62F, 0.68F, 0.78F}, CategoryStyle::Shape::Cube},
}};

struct NameRule {
    std::string_view keyword;
    ObjectCategory category;
};

// Name-based refinement for "obj" placements. Matching is a case-insensitive
// substring test on the object name; the first hit wins.
const NameRule kNameRules[]{
    {"mario", ObjectCategory::Player},
    {"luigi", ObjectCategory::Player},
    {"kinopio", ObjectCategory::Player},
    {"peach", ObjectCategory::Player},
    {"rosetta", ObjectCategory::Player},
    {"kuribo", ObjectCategory::Enemy},
    {"karon", ObjectCategory::Enemy},
    {"packun", ObjectCategory::Enemy},
    {"boss", ObjectCategory::Enemy},
    {"dossun", ObjectCategory::Enemy},
    {"choppa", ObjectCategory::Enemy},
    {"blooper", ObjectCategory::Enemy},
    {"hammer", ObjectCategory::Enemy},
    {"bullet", ObjectCategory::Enemy},
    {"torpedo", ObjectCategory::Enemy},
    {"star", ObjectCategory::Item},
    {"coin", ObjectCategory::Item},
    {"gemstone", ObjectCategory::Item},
    {"silver", ObjectCategory::Item},
    {"planet", ObjectCategory::Terrain},
    {"ground", ObjectCategory::Terrain},
    {"floor", ObjectCategory::Terrain},
    {"rock", ObjectCategory::Terrain},
};
bool containsIgnoreCase(std::string_view text, std::string_view keyword) {
    if (keyword.size() > text.size()) {
        return false;
    }
    const auto fold = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    for (std::size_t offset = 0; offset + keyword.size() <= text.size(); ++offset) {
        bool match = true;
        for (std::size_t i = 0; i < keyword.size(); ++i) {
            if (fold(text[offset + i]) != fold(keyword[i])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

void pushTriangle(std::vector<math::Vec3f>& out, const math::Vec3f& a, const math::Vec3f& b, const math::Vec3f& c) {
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
}

} // namespace

const CategoryStyle& categoryStyle(ObjectCategory category) {
    const auto index = static_cast<std::size_t>(category);
    if (index >= kStyles.size()) {
        return kStyles.back();
    }
    return kStyles[index];
}

std::size_t categoryCount() noexcept {
    return kStyles.size();
}

ObjectCategory classifyObject(const std::string& kind, const std::string& name) {
    if (kind == "start") {
        return ObjectCategory::Player;
    }
    if (kind == "camera") {
        return ObjectCategory::Camera;
    }
    if (kind == "area") {
        return ObjectCategory::Zone;
    }
    if (kind == "gravity") {
        return ObjectCategory::Gravity;
    }
    if (kind == "mappart") {
        return ObjectCategory::MapPart;
    }
    if (kind == "cutscene" || kind == "demo") {
        return ObjectCategory::Cutscene;
    }
    if (kind == "stage") {
        return ObjectCategory::Zone;
    }
    for (const auto& rule : kNameRules) {
        if (containsIgnoreCase(name, rule.keyword)) {
            return rule.category;
        }
    }
    return ObjectCategory::Misc;
}

const CategoryStyle& objectStyle(const std::string& kind, const std::string& name) {
    return categoryStyle(classifyObject(kind, name));
}
std::vector<math::Vec3f> shapeTriangles(CategoryStyle::Shape shape) {
    std::vector<math::Vec3f> triangles;
    switch (shape) {
    case CategoryStyle::Shape::Cube: {
        const math::Vec3f p000{-1, -1, 1};
        const math::Vec3f p100{1, -1, 1};
        const math::Vec3f p110{1, 1, 1};
        const math::Vec3f p010{-1, 1, 1};
        const math::Vec3f p001{-1, -1, -1};
        const math::Vec3f p101{1, -1, -1};
        const math::Vec3f p111{1, 1, -1};
        const math::Vec3f p011{-1, 1, -1};

        pushTriangle(triangles, p000, p100, p110);
        pushTriangle(triangles, p000, p110, p010);
        pushTriangle(triangles, p001, p011, p111);
        pushTriangle(triangles, p001, p111, p101);
        pushTriangle(triangles, p010, p110, p111);
        pushTriangle(triangles, p010, p111, p011);
        pushTriangle(triangles, p001, p100, p000);
        pushTriangle(triangles, p001, p101, p100);
        pushTriangle(triangles, p100, p110, p111);
        pushTriangle(triangles, p100, p111, p101);
        pushTriangle(triangles, p010, p011, p001);
        pushTriangle(triangles, p010, p001, p000);
        break;
    }
    case CategoryStyle::Shape::Sphere: {
        constexpr int kSlices = 12;
        constexpr int kStacks = 8;
        const auto point = [](int slice, int stack) {
            const float yaw = 6.2831853F * static_cast<float>(slice) / static_cast<float>(kSlices);
            const float pitch =
                3.1415927F * static_cast<float>(stack) / static_cast<float>(kStacks) - 1.5707964F;
            return math::Vec3f{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
        };
        for (int stack = 0; stack < kStacks; ++stack) {
            for (int slice = 0; slice < kSlices; ++slice) {
                const math::Vec3f a = point(slice, stack);
                const math::Vec3f b = point(slice + 1, stack);
                const math::Vec3f c = point(slice + 1, stack + 1);
                const math::Vec3f d = point(slice, stack + 1);
                pushTriangle(triangles, a, b, c);
                pushTriangle(triangles, a, c, d);
            }
        }
        break;
    }
    case CategoryStyle::Shape::Pyramid: {
        const math::Vec3f apex{0, 1, 0};
        const math::Vec3f base00{-1, -1, 1};
        const math::Vec3f base10{1, -1, 1};
        const math::Vec3f base11{1, -1, -1};
        const math::Vec3f base01{-1, -1, -1};
        pushTriangle(triangles, base00, base10, apex);
        pushTriangle(triangles, base10, base11, apex);
        pushTriangle(triangles, base11, base01, apex);
        pushTriangle(triangles, base01, base00, apex);
        pushTriangle(triangles, base00, base01, base11);
        pushTriangle(triangles, base00, base11, base10);
        break;
    }
    case CategoryStyle::Shape::Octahedron: {
        const math::Vec3f top{0, 1, 0};
        const math::Vec3f bottom{0, -1, 0};
        const math::Vec3f front{0, 0, 1};
        const math::Vec3f back{0, 0, -1};
        const math::Vec3f left{-1, 0, 0};
        const math::Vec3f right{1, 0, 0};
        pushTriangle(triangles, front, right, top);
        pushTriangle(triangles, right, back, top);
        pushTriangle(triangles, back, left, top);
        pushTriangle(triangles, left, front, top);
        pushTriangle(triangles, right, front, bottom);
        pushTriangle(triangles, back, right, bottom);
        pushTriangle(triangles, left, back, bottom);
        pushTriangle(triangles, front, left, bottom);
        break;
    }
    case CategoryStyle::Shape::Cylinder: {
        constexpr int kSlices = 12;
        const auto rim = [](int slice, float y) {
            const float yaw = 6.2831853F * static_cast<float>(slice) / static_cast<float>(kSlices);
            return math::Vec3f{std::cos(yaw), y, std::sin(yaw)};
        };
        for (int slice = 0; slice < kSlices; ++slice) {
            const math::Vec3f a = rim(slice, 1);
            const math::Vec3f b = rim(slice + 1, 1);
            const math::Vec3f c = rim(slice + 1, -1);
            const math::Vec3f d = rim(slice, -1);
            pushTriangle(triangles, a, b, c);
            pushTriangle(triangles, a, c, d);
            pushTriangle(triangles, math::Vec3f{0, 1, 0}, a, b);
            pushTriangle(triangles, math::Vec3f{0, -1, 0}, d, c);
        }
        break;
    }
    }
    return triangles;
}

} // namespace whitehole::render
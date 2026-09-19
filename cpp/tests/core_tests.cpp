#include "whitehole/app/settings.hpp"
#include "whitehole/db/data_holder.hpp"
#include "whitehole/db/name_table.hpp"
#include "whitehole/db/hints.hpp"
#include "whitehole/db/areamanagerlimits.hpp"
#include "whitehole/db/shortcuts.hpp"
#include "whitehole/db/modelsubstitutions.hpp"
#include "whitehole/db/specialrenderers.hpp"
#include "whitehole/db/object_db.hpp"
#include "whitehole/io/binary_file.hpp"
#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/io/rarc.hpp"
#include "whitehole/io/yaz0.hpp"
#include "whitehole/util/json.hpp"
#include "whitehole/util/text.hpp"
#include "whitehole/math/geometry.hpp"
#include "whitehole/render/camera.hpp"
#include "whitehole/render/object_visual.hpp"
#include "whitehole/render/viewport_scene.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/bti.hpp"
#include "whitehole/smg/game_archive.hpp"
#include "whitehole/smg/hash.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testBinaryData() {
    whitehole::io::BinaryWriter writer(whitehole::io::Endian::big);
    writer.writeU8(0x12);
    writer.writeU16(0x3456);
    writer.writeU32(0x789ABCDE);
    writer.writeF32(1.25F);
    writer.writeString("Galaxy");

    whitehole::io::BinaryReader reader(writer.data(), whitehole::io::Endian::big);
    expect(reader.readU8() == 0x12, "8-bit binary round trip failed");
    expect(reader.readU16() == 0x3456, "16-bit binary round trip failed");
    expect(reader.readU32() == 0x789ABCDE, "32-bit binary round trip failed");
    expect(std::abs(reader.readF32() - 1.25F) < 0.00001F, "float binary round trip failed");
    expect(reader.readString() == "Galaxy", "string binary round trip failed");

    bool rejected = false;
    try {
        (void)reader.readU8();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "out-of-bounds binary read was not rejected");
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto unique = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        path = std::filesystem::temp_directory_path() / ("whitehole-native-tests-" + unique);
        if (!std::filesystem::create_directory(path)) {
            throw std::runtime_error("could not create temporary test directory");
        }
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(path); }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    std::filesystem::path path;
};

void testDirectoryFilesystem() {
    TemporaryDirectory temporary;
    whitehole::io::DirectoryFilesystem project(temporary.path);
    project.createDirectory("/StageData/TestGalaxy");
    project.write("/StageData/TestGalaxy/TestGalaxyMap.arc", {1, 2, 3});
    expect(project.directoryExists("/StageData/TestGalaxy"), "project directory was not created");
    expect(project.fileExists("/StageData/TestGalaxy/TestGalaxyMap.arc"), "project file was not created");
    expect(project.read("/StageData/TestGalaxy/TestGalaxyMap.arc") == std::vector<std::uint8_t>({1, 2, 3}),
           "project file contents changed");
    expect(project.directories("/StageData") == std::vector<std::string>({"TestGalaxy"}),
           "project directory listing changed");
    expect(project.files("/StageData/TestGalaxy") == std::vector<std::string>({"TestGalaxyMap.arc"}),
           "project file listing changed");

    project.renameFile("/StageData/TestGalaxy/TestGalaxyMap.arc", "Renamed.arc");
    expect(project.fileExists("/StageData/TestGalaxy/Renamed.arc"), "project file rename failed");
    project.renameDirectory("/StageData/TestGalaxy", "RenamedGalaxy");
    expect(project.fileExists("/StageData/RenamedGalaxy/Renamed.arc"), "project directory rename failed");

    bool traversalRejected = false;
    try {
        (void)project.read("/../outside");
    } catch (const std::runtime_error&) {
        traversalRejected = true;
    }
    expect(traversalRejected, "project path traversal was not rejected");

    project.removeFile("/StageData/RenamedGalaxy/Renamed.arc");
    project.removeDirectory("/StageData/RenamedGalaxy");
    expect(!project.directoryExists("/StageData/RenamedGalaxy"), "project directory removal failed");
}

void testYaz0() {
    std::vector<std::uint8_t> input;
    const std::string pattern = "Whitehole Pro native archive support! ";
    for (int index = 0; index < 100; ++index) {
        input.insert(input.end(), pattern.begin(), pattern.end());
        input.push_back(static_cast<std::uint8_t>(index));
    }
    const auto compressed = whitehole::io::yaz0::compress(input);
    expect(whitehole::io::yaz0::isCompressed(compressed), "Yaz0 output has no header");
    expect(compressed.size() < input.size(), "Yaz0 did not compress repetitive data");
    expect(whitehole::io::yaz0::decompress(compressed) == input, "Yaz0 round trip failed");
    expect(whitehole::io::yaz0::compress(compressed) == compressed, "Yaz0 double compression changed data");
    expect(whitehole::io::yaz0::decompress(whitehole::io::yaz0::compress(std::vector<std::uint8_t>{})).empty(),
           "empty Yaz0 round trip failed");

    bool rejected = false;
    try {
        (void)whitehole::io::yaz0::decompress({'Y', 'a', 'z', '0'});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "truncated Yaz0 data was not rejected");
}

std::vector<std::uint8_t> makeTinyRarc(whitehole::io::Endian endian) {
    whitehole::io::BinaryWriter writer(endian);
    writer.writeU32(0x52415243); // RARC
    writer.writeU32(0xA3);      // file size
    writer.writeU32(0x20);      // header size
    writer.writeU32(0x80);      // data offset, relative to 0x20
    writer.writeU32(3);
    writer.writeU32(3);
    writer.writeU32(0);
    writer.writeU32(0);
    writer.writeU32(1);         // node count
    writer.writeU32(0x20);      // node table at 0x40
    writer.writeU32(3);         // entry count
    writer.writeU32(0x30);      // entry table at 0x50
    writer.writeU32(16);        // string table length
    writer.writeU32(0x70);      // string table at 0x90
    writer.writeU32(0);
    writer.writeU32(0);

    writer.seek(0x40);
    writer.writeU32(0x524F4F54); // ROOT
    writer.writeU32(5);          // "root"
    writer.writeU16(0);
    writer.writeU16(3);
    writer.writeU32(0);

    const auto writeDirectoryEntry = [&](std::uint16_t nameOffset, std::uint32_t node) {
        writer.writeU16(0xFFFF);
        writer.writeU16(0);
        if (endian == whitehole::io::Endian::big) {
            writer.writeU16(0x0200);
            writer.writeU16(nameOffset);
        } else {
            writer.writeU16(nameOffset);
            writer.writeU16(0x0200);
        }
        writer.writeU32(node);
        writer.writeU32(0x10);
        writer.writeU32(0);
    };
    writeDirectoryEntry(0, 0);
    writeDirectoryEntry(2, 0xFFFFFFFF);

    writer.writeU16(0);
    writer.writeU16(0);
    if (endian == whitehole::io::Endian::big) {
        writer.writeU16(0x1100);
        writer.writeU16(10);
    } else {
        writer.writeU16(10);
        writer.writeU16(0x1100);
    }
    writer.writeU32(0);
    writer.writeU32(3);
    writer.writeU32(0);

    writer.seek(0x90);
    writer.writeString(".");
    writer.writeString("..");
    writer.writeString("root");
    writer.writeString("file");
    writer.seek(0xA0);
    writer.writeU8(1);
    writer.writeU8(2);
    writer.writeU8(3);
    return std::move(writer).take();
}

void testRarcEndianness() {
    for (const auto endian : {whitehole::io::Endian::big, whitehole::io::Endian::little}) {
        whitehole::io::RarcArchive archive(makeTinyRarc(endian));
        expect(archive.endian() == endian, "RARC endian detection failed");
        expect(archive.rootName() == "root", "RARC root name changed");
        expect(archive.entries().size() == 1, "RARC entry table was not parsed");
        expect(archive.entries().front().path == "root/file", "RARC entry path changed");
        expect(archive.read(archive.entries().front()) == std::vector<std::uint8_t>({1, 2, 3}),
               "RARC entry contents changed");
        archive.replace("ROOT/FILE", {9, 8, 7, 6});
        const whitehole::io::RarcArchive rewritten(archive.serialize(false));
        expect(rewritten.endian() == endian, "rewritten RARC endian changed");
        expect(rewritten.entries().size() == 1, "rewritten RARC entry count changed");
        expect(rewritten.read(rewritten.entries().front()) == std::vector<std::uint8_t>({9, 8, 7, 6}),
               "RARC replacement was not serialized");
    }
}

void testMath() {
    using whitehole::math::Matrix4;
    using whitehole::math::Vec3f;
    const auto transformed = Matrix4::translation({10, 20, 30}).transformPoint({1, 2, 3});
    expect(std::abs(transformed.x - 11) < 0.00001F, "matrix X translation failed");
    expect(std::abs(transformed.y - 22) < 0.00001F, "matrix Y translation failed");
    expect(std::abs(transformed.z - 33) < 0.00001F, "matrix Z translation failed");
    const auto composed = Matrix4::translation({10, 20, 30}) * Matrix4::scale({2, 3, 4});
    const auto composedPoint = composed.transformPoint({1, 2, 3});
    expect(std::abs(composedPoint.x - 12) < 0.00001F, "matrix composition scaled X translation");
    expect(std::abs(composedPoint.y - 26) < 0.00001F, "matrix composition scaled Y translation");
    expect(std::abs(composedPoint.z - 42) < 0.00001F, "matrix composition scaled Z translation");
    expect(std::abs(Vec3f::dot({1, 0, 0}, {0, 1, 0})) < 0.00001F, "vector dot product failed");
    const auto cross = Vec3f::cross({1, 0, 0}, {0, 1, 0});
    expect(std::abs(cross.z - 1) < 0.00001F, "vector cross product failed");
}

void testViewportCamera() {
    whitehole::render::ViewportCamera camera;
    camera.target = {100.0F, 0.0F, 0.0F};
    camera.yawRadians = 0.0F;
    camera.pitchRadians = 0.0F;
    camera.distance = 500.0F;

    const auto eye = camera.eye();
    expect(std::abs(eye.x - 600.0F) < 0.01F, "viewport camera eye is wrong");

    // Center of the screen must ray-cast straight at the orbit target.
    const auto ray = camera.screenToRay(400.0F, 300.0F, 800.0F, 600.0F);
    const auto toTarget = whitehole::math::Vec3f{100.0F - ray.origin.x, 0.0F - ray.origin.y, 0.0F - ray.origin.z};
    const float alignment = whitehole::math::Vec3f::dot(ray.direction, toTarget.normalized());
    expect(alignment > 0.999F, "viewport camera center ray misses target");

    // Round trip through world->screen keeps the target centered.
    float screenX = 0.0F;
    float screenY = 0.0F;
    expect(camera.worldToScreen(camera.target, 800.0F, 600.0F, screenX, screenY), "target behind viewport camera");
    expect(std::abs(screenX - 400.0F) < 1.0F && std::abs(screenY - 300.0F) < 1.0F,
           "viewport camera projection is off-center");

    const float before = camera.distance;
    camera.dolly(1.0F);
    expect(camera.distance < before, "viewport camera dolly-in failed");
    camera.frameTarget({1.0F, 2.0F, 3.0F}, 250.0F);
    expect(std::abs(camera.target.x - 1.0F) < 0.001F && std::abs(camera.distance - 250.0F) < 0.001F,
           "viewport camera framing failed");
}

void testViewportScene() {
    whitehole::smg::PlacementObject object;
    object.name = "Kinopio";
    object.kind = "obj";
    object.position = {100.0F, 0.0F, 0.0F};
    object.rotation = {0.0F, 0.0F, 0.0F};
    object.scale = {1.0F, 1.0F, 1.0F};

    whitehole::render::ViewportScene scene;
    scene.rebuild({object});
    expect(scene.boxes().size() == 1, "viewport scene dropped the object");
    const auto& box = scene.boxes().front();
    expect(std::abs(box.center.x - 100.0F) < 0.001F, "viewport box center is wrong");

    // Box matrix must map the unit-box corner onto position + half extent.
    const auto corner = box.world.transformPoint({1.0F, 1.0F, 1.0F});
    expect(std::abs(corner.x - 125.0F) < 0.01F, "viewport box world matrix is wrong");

    whitehole::render::ViewportCamera camera;
    camera.target = object.position;
    camera.yawRadians = 0.0F;
    camera.pitchRadians = 0.0F;
    camera.distance = 500.0F;
    const auto hit = scene.pick(camera, 400.0F, 300.0F, 800.0F, 600.0F);
    expect(hit.has_value() && *hit == 0, "viewport picking missed the centered object");
    // Far corner of the screen should miss the single centered box.
    expect(!scene.pick(camera, 799.0F, 599.0F, 800.0F, 600.0F).has_value(), "viewport picking hit empty space");
}

void testObjectVisual() {
    using whitehole::render::ObjectCategory;
    using whitehole::render::categoryStyle;
    using whitehole::render::classifyObject;
    using whitehole::render::objectStyle;
    using whitehole::render::shapeTriangles;

    // Table kind drives the category first.
    expect(classifyObject("start", "Mario") == ObjectCategory::Player, "start objects should classify as Player");
    expect(classifyObject("camera", "CameraPos") == ObjectCategory::Camera, "camera table should classify as Camera");
    expect(classifyObject("area", "AreaVolume") == ObjectCategory::Zone, "area table should classify as Zone");
    expect(classifyObject("gravity", "GravitySphere") == ObjectCategory::Gravity, "gravity table should classify as Gravity");
    expect(classifyObject("mappart", "Elevator") == ObjectCategory::MapPart, "mappart table should classify as MapPart");
    expect(classifyObject("cutscene", "Demo") == ObjectCategory::Cutscene, "cutscene table should classify as Cutscene");

    // Object names refine plain "obj" placements.
    expect(classifyObject("obj", "Kinopio") == ObjectCategory::Player, "Kinopio should classify as Player");
    expect(classifyObject("obj", "Kuribo") == ObjectCategory::Enemy, "Kuribo should classify as Enemy");
    expect(classifyObject("obj", "BossKuriboJunior") == ObjectCategory::Enemy, "Boss names should classify as Enemy");
    expect(classifyObject("obj", "PowerStar") == ObjectCategory::Item, "PowerStar should classify as Item");
    expect(classifyObject("obj", "PurpleCoin") == ObjectCategory::Item, "coins should classify as Item");
    expect(classifyObject("obj", "PlanetDifferencesAndBeyond") == ObjectCategory::Terrain, "planets should classify as Terrain");
    expect(classifyObject("obj", "OceanWave") == ObjectCategory::Misc, "unknown names should classify as Misc");

    // Every category has a distinct, valid color (the whole visual system
    // depends on categories being tellable apart at a glance).
    for (std::size_t a = 0; a < whitehole::render::categoryCount(); ++a) {
        const auto& styleA = categoryStyle(static_cast<ObjectCategory>(a));
        expect(std::abs(styleA.color[0]) + std::abs(styleA.color[1]) + std::abs(styleA.color[2]) > 0.1F,
               "category color must not be black");
        for (std::size_t b = a + 1; b < whitehole::render::categoryCount(); ++b) {
            const auto& styleB = categoryStyle(static_cast<ObjectCategory>(b));
            const bool same = std::abs(styleA.color[0] - styleB.color[0]) < 0.01F &&
                              std::abs(styleA.color[1] - styleB.color[1]) < 0.01F &&
                              std::abs(styleA.color[2] - styleB.color[2]) < 0.01F;
            expect(!same, "category colors must be distinct");
            expect(styleA.shape != styleB.shape || std::string_view(styleA.label) != std::string_view(styleB.label),
                   "categories must differ in shape or label");
        }
    }

    // Shape meshes are unit-sized triangle soup.
    const auto cube = shapeTriangles(whitehole::render::CategoryStyle::Shape::Cube);
    expect(cube.size() == 12 * 3, "cube mesh should have 12 triangles");
    const auto sphere = shapeTriangles(whitehole::render::CategoryStyle::Shape::Sphere);
    expect(sphere.size() == 12 * 8 * 2 * 3, "sphere mesh triangle count is wrong");
    const auto pyramid = shapeTriangles(whitehole::render::CategoryStyle::Shape::Pyramid);
    expect(pyramid.size() == 6 * 3, "pyramid mesh should have 6 triangles");
    const auto octa = shapeTriangles(whitehole::render::CategoryStyle::Shape::Octahedron);
    expect(octa.size() == 8 * 3, "octahedron mesh should have 8 triangles");
    const auto cylinder = shapeTriangles(whitehole::render::CategoryStyle::Shape::Cylinder);
    expect(cylinder.size() == 12 * 4 * 3, "cylinder mesh triangle count is wrong");

    const float maxComponent = [](const std::vector<whitehole::math::Vec3f>& triangles) {
        float worst = 0.0F;
        for (const auto& vertex : triangles) {
            worst = std::max(worst, std::max({std::abs(vertex.x), std::abs(vertex.y), std::abs(vertex.z)}));
        }
        return worst;
    }(cube);
    expect(maxComponent < 1.0001F, "cube mesh must stay within the unit cube");

    // The scene stores the category so renderers and lists can share it.
    whitehole::smg::PlacementObject object;
    object.name = "Kuribo";
    object.kind = "obj";
    object.scale = {0.001F, 0.001F, 0.001F};
    whitehole::render::ViewportScene scene;
    scene.rebuild({object});
    expect(scene.boxes().size() == 1, "viewport scene dropped the object");
    expect(scene.boxes().front().category == ObjectCategory::Enemy, "viewport scene lost the object category");
    // Micro-scaled objects are clamped to the minimum visual scale so they
    // stay visible and clickable.
    expect(std::abs(scene.boxes().front().halfExtents.x - 25.0F * whitehole::render::kMinVisualScale) < 0.01F,
           "minimum visual scale clamp failed");
    // objectStyle ties classification to visuals in one call.
    expect(&objectStyle("obj", "PowerStar") == &categoryStyle(ObjectCategory::Item),
           "objectStyle should match classifyObject");
}

void testHashes() {
    expect(whitehole::smg::jmapHash("name") == 0x00337A8B, "JMap hash does not match the game algorithm");
    expect(whitehole::smg::superFastHash("") == 0, "empty SuperFastHash changed");
    expect(whitehole::smg::superFastHash("Whitehole") == 0x680E328E, "SuperFastHash compatibility vector changed");
}

std::vector<std::uint8_t> makeTinyBcsv(whitehole::io::Endian endian) {
    whitehole::io::BinaryWriter writer(endian);
    writer.writeU32(1);    // rows
    writer.writeU32(2);    // fields
    writer.writeU32(0x28); // data offset
    writer.writeU32(8);    // row size

    writer.writeU32(whitehole::smg::jmapHash("number"));
    writer.writeU32(0x0000FFFF);
    writer.writeU16(0);
    writer.writeU8(0);
    writer.writeU8(static_cast<std::uint8_t>(whitehole::smg::BcsvType::integer));

    writer.writeU32(whitehole::smg::jmapHash("label"));
    writer.writeU32(0xFFFFFFFF);
    writer.writeU16(4);
    writer.writeU8(0);
    writer.writeU8(static_cast<std::uint8_t>(whitehole::smg::BcsvType::stringOffset));

    writer.writeU32(42);
    writer.writeU32(0);
    writer.writeString("Comet");
    return std::move(writer).take();
}

void expectTablesEqual(const whitehole::smg::BcsvTable& left, const whitehole::smg::BcsvTable& right,
                       const std::string& context) {
    expect(left.entrySize() == right.entrySize(), context + ": row size changed");
    expect(left.fields().size() == right.fields().size(), context + ": field count changed");
    expect(left.rows().size() == right.rows().size(), context + ": row count changed");
    for (std::size_t index = 0; index < left.fields().size(); ++index) {
        const auto& a = left.fields()[index];
        const auto& b = right.fields()[index];
        expect(a.hash == b.hash && a.mask == b.mask && a.offset == b.offset
                   && a.shift == b.shift && a.type == b.type,
               context + ": field descriptor changed");
    }
    for (std::size_t row = 0; row < left.rows().size(); ++row) {
        expect(left.rows()[row].values.size() == right.rows()[row].values.size(),
               context + ": row width changed");
        for (std::size_t field = 0; field < left.rows()[row].values.size(); ++field) {
            const auto& a = left.rows()[row].values[field];
            const auto& b = right.rows()[row].values[field];
            expect(a.index() == b.index(), context + ": value type changed");
            if (std::holds_alternative<float>(a)) {
                const auto av = std::get<float>(a);
                const auto bv = std::get<float>(b);
                expect(av == bv || (std::isnan(av) && std::isnan(bv)), context + ": float value changed");
            } else {
                expect(a == b, context + ": value changed");
            }
        }
    }
}

void testBcsvEndianness() {
    for (const auto endian : {whitehole::io::Endian::big, whitehole::io::Endian::little}) {
        const whitehole::smg::BcsvTable table(makeTinyBcsv(endian), endian);
        expect(table.fields().size() == 2 && table.rows().size() == 1, "tiny BCSV shape changed");
        expect(std::get<std::int32_t>(table.rows()[0].values[0]) == 42, "BCSV integer parsing failed");
        expect(std::get<std::string>(table.rows()[0].values[1]) == "Comet", "BCSV string parsing failed");
        const whitehole::smg::BcsvTable rewritten(table.serialize(), endian);
        expectTablesEqual(table, rewritten, "tiny BCSV round trip");

        auto invalid = table;
        invalid.rows()[0].values[0] = std::int32_t{70000};
        bool overflowRejected = false;
        try {
            (void)invalid.serialize();
        } catch (const std::runtime_error&) {
            overflowRejected = true;
        }
        expect(overflowRejected, "BCSV silently truncated an integer outside its field mask");

        auto invalidByteMask = table;
        invalidByteMask.fields()[0].type = whitehole::smg::BcsvType::byte;
        invalidByteMask.fields()[0].mask = 0x100;
        invalidByteMask.rows()[0].values[0] = std::int8_t{1};
        bool maskRejected = false;
        try {
            (void)invalidByteMask.serialize();
        } catch (const std::runtime_error&) {
            maskRejected = true;
        }
        expect(maskRejected, "BCSV accepted mask bits outside a field's storage width");
    }
}

void testProjectArchives() {
    const auto templates = std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data" / "templates";
    std::size_t archiveCount = 0;
    std::size_t bcsvCount = 0;
    for (const auto& item : std::filesystem::directory_iterator(templates)) {
        if (item.path().extension() != ".arc") {
            continue;
        }
        const auto archive = whitehole::io::RarcArchive::open(item.path());
        expect(archive.wasCompressed(), item.path().string() + " should be Yaz0 compressed");
        expect(!archive.rootName().empty(), item.path().string() + " has no RARC root");
        expect(!archive.entries().empty(), item.path().string() + " has no RARC entries");
        for (const auto& entry : archive.entries()) {
            const auto explicitBcsv = std::filesystem::path(entry.path).extension() == ".bcsv";
            const auto jmapTable = entry.path.find("/jmp/") != std::string::npos;
            if (entry.directory || (!explicitBcsv && !jmapTable)) {
                continue;
            }
            const whitehole::smg::BcsvTable table(archive.read(entry), archive.endian());
            const whitehole::smg::BcsvTable rewritten(table.serialize(), archive.endian());
            expectTablesEqual(table, rewritten, item.path().filename().string() + ":" + entry.path);
            ++bcsvCount;
        }
        const whitehole::io::RarcArchive repacked(archive.serialize(true));
        expect(repacked.wasCompressed(), item.path().string() + " was not recompressed");
        expect(repacked.endian() == archive.endian(), item.path().string() + " endian changed after repacking");
        expect(repacked.entries().size() == archive.entries().size(), item.path().string() + " entry count changed");
        for (std::size_t index = 0; index < archive.entries().size(); ++index) {
            const auto& before = archive.entries()[index];
            const auto& after = repacked.entries()[index];
            expect(before.path == after.path && before.directory == after.directory,
                   item.path().string() + ": archive tree changed after repacking");
            if (!before.directory) {
                expect(archive.read(before) == repacked.read(after),
                       item.path().string() + ": file data changed after repacking");
            }
        }
        ++archiveCount;
    }
    expect(archiveCount == 9, "not every bundled archive template was tested");
    expect(bcsvCount >= 20, "too few bundled BCSV tables were tested");
}

void testArchiveTableEdit() {
    const auto source = std::filesystem::path(WHITEHOLE_SOURCE_DIR)
        / "data" / "templates" / "SMG1OneStarGalaxyScenario.arc";
    auto archive = whitehole::io::RarcArchive::open(source);
    const auto tableEntry = std::find_if(archive.entries().begin(), archive.entries().end(), [](const auto& entry) {
        return !entry.directory && std::filesystem::path(entry.path).filename() == "scenariodata.bcsv";
    });
    expect(tableEntry != archive.entries().end(), "scenario table is missing from the test archive");
    whitehole::smg::BcsvTable table(archive.read(*tableEntry), archive.endian());
    expect(!table.rows().empty() && !table.rows()[0].values.empty(), "scenario table has no editable value");
    table.rows()[0].values[0] = std::int32_t{77};
    archive.replace(tableEntry->path, table.serialize());

    const whitehole::io::RarcArchive saved(archive.serialize(true));
    const auto savedEntry = std::find_if(saved.entries().begin(), saved.entries().end(), [&](const auto& entry) {
        return entry.path == tableEntry->path;
    });
    expect(savedEntry != saved.entries().end(), "edited scenario table disappeared after archive save");
    const whitehole::smg::BcsvTable savedTable(saved.read(*savedEntry), saved.endian());
    expect(std::get<std::int32_t>(savedTable.rows()[0].values[0]) == 77,
           "edited BCSV value did not survive archive recompression");
}

void testNameTables() {
    whitehole::db::NameTable galaxies;
    galaxies.loadJson(std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data" / "galaxies.json");
    expect(galaxies.displayName("EggStarGalaxy").find("Good Egg") != std::string::npos,
           "galaxy display names did not load");
}

void testStageAndGameModels() {
    const auto templates = std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data" / "templates";
    auto stage = whitehole::smg::StageArchive::openMapFile(templates / "SMG2BigGalaxyMap.arc");
    expect(!stage.objects().empty(), "template map loaded no placement objects");
    expect(stage.objects().size() >= 10, "template map loaded fewer objects than expected");
    auto& object = stage.objects().front();
    const auto original = object.position.x;
    object.position.x = original + 12.5F;
    TemporaryDirectory output;
    const auto savedPath = output.path / "edited-map.arc";
    stage.saveTo(savedPath);
    const auto reloaded = whitehole::smg::StageArchive::openMapFile(savedPath);
    expect(!reloaded.objects().empty(), "saved map reloaded no objects");
    expect(std::abs(reloaded.objects().front().position.x - (original + 12.5F)) < 0.01F,
           "edited object position did not survive save");

    TemporaryDirectory workspace;
    std::filesystem::create_directories(workspace.path / "SystemData");
    std::filesystem::create_directories(workspace.path / "StageData" / "TestGalaxy");
    whitehole::io::writeFile(workspace.path / "SystemData" / "ObjNameTable.arc", {0x52, 0x41, 0x52, 0x43});
    std::filesystem::copy_file(templates / "SMG2BigGalaxyScenario.arc",
                               workspace.path / "StageData" / "TestGalaxy" / "TestGalaxyScenario.arc");
    std::filesystem::copy_file(templates / "SMG2BigGalaxyMap.arc",
                               workspace.path / "StageData" / "TestGalaxy" / "TestGalaxyMap.arc");
    whitehole::smg::GameArchive game(workspace.path);
    expect(game.gameType() == 2, "synthetic workspace was not detected as SMG2");
    expect(game.galaxyExists("TestGalaxy"), "synthetic galaxy was not listed");
    const auto galaxy = game.openGalaxy("TestGalaxy");
    expect(!galaxy.zones().empty(), "synthetic galaxy has no zones");
}

void testBtiDecoding() {
    using whitehole::io::Endian;
    using whitehole::smg::decodeBtiImage;
    using whitehole::smg::parseBti;

    // RGB565 (format 4): 4x4 of full-white word 0xFFFF -> opaque white.
    {
        std::vector<std::uint8_t> data(32, 0xFF);
        auto img = decodeBtiImage(data, 0, 4, 4, 4, 0, Endian::big);
        expect(img.width == 4 && img.height == 4, "RGB565 dimensions were not 4x4");
        expect(img.rgba.size() == 4u * 4u * 4u, "RGB565 rgba byte count was wrong");
        expect(img.rgba[0] == 255 && img.rgba[3] == 255, "RGB565 0xFFFF should decode to white opaque");
    }

    // RGB5A3 opaque (format 5, bit 15 set): 0xFFFF -> white opaque. One 4x4 block = 32 bytes.
    {
        std::vector<std::uint8_t> data(32, 0xFF);
        auto img = decodeBtiImage(data, 0, 5, 2, 2, 0, Endian::big);
        expect(img.rgba[0] == 255 && img.rgba[3] == 255, "RGB5A3 opaque 0xFFFF should be white opaque");
    }

    // RGB5A3 ARGB3444 (format 5, bit 15 clear): exercises the expand4 fix.
    {
        std::vector<std::uint8_t> maxWord(32, 0);
        maxWord[0] = 0x7F; maxWord[1] = 0xFF; // a=7, r=15, g=15, b=15
        auto img = decodeBtiImage(maxWord, 0, 5, 1, 1, 0, Endian::big);
        expect(img.rgba[0] == 255 && img.rgba[3] == 255, "RGB5A3 ARGB3444 max should be opaque white");
        std::vector<std::uint8_t> blueWord(32, 0);
        blueWord[0] = 0x00; blueWord[1] = 0x0F; // blue nibble 15, alpha nibble 0
        auto img2 = decodeBtiImage(blueWord, 0, 5, 1, 1, 0, Endian::big);
        expect(img2.rgba[2] == 255, "RGB5A3 blue nibble 0xF must expand to 255 (expand4 fix)");
        expect(img2.rgba[3] == 0, "RGB5A3 alpha nibble 0 must expand to 0");
        std::vector<std::uint8_t> midWord(32, 0);
        midWord[0] = 0x00; midWord[1] = 0x05; // blue nibble 5
        auto img3 = decodeBtiImage(midWord, 0, 5, 1, 1, 0, Endian::big);
        expect(img3.rgba[2] == 85, "RGB5A3 nibble 0x5 must expand to 85 (expand4)");
    }


    // IA4 (format 2): 2x2 inside one 8x4 block (32 bytes consumed).
    {
        std::vector<std::uint8_t> data(32, 0);
        data[0] = 0x55; // intensity 5, alpha 5 -> expand4(5) = 85
        data[1] = 0xFF; // intensity 15, alpha 15 -> 255
        auto img = decodeBtiImage(data, 0, 2, 2, 2, 0, Endian::big);
        const std::size_t p00 = (0u * img.width + 0u) * 4u;
        const std::size_t p10 = (0u * img.width + 1u) * 4u;
        expect(img.rgba[p00] == 85 && img.rgba[p00 + 3] == 85, "IA4 nibble 5 should expand to 85");
        expect(img.rgba[p10] == 255, "IA4 nibble 15 should expand to 255");
    }

    // I8 (format 1): 2x2 inside one 8x4 block (32 bytes consumed).
    {
        std::vector<std::uint8_t> data(32, 0);
        data[0] = 0x80;
        data[1] = 0x10;
        data[8] = 0xFF;
        data[9] = 0x00;
        auto img = decodeBtiImage(data, 0, 1, 2, 2, 0, Endian::big);
        expect(img.rgba[0] == 0x80 && img.rgba[3] == 255, "I8 pixel (0,0) was wrong");
        expect(img.rgba[8] == 0xFF, "I8 pixel (0,1) was wrong");
        expect(img.rgba[12] == 0x00 && img.rgba[15] == 255, "I8 pixel (1,1) was wrong");
    }

    // C8 (format 9) palettized: rgb565 palette, entry 0 white, entry 1 black.
    {
        std::vector<std::uint8_t> palette = {0xFF, 0xFF, 0x00, 0x00};
        std::vector<std::uint8_t> data(32, 0);
        data[1] = 1;
        data[9] = 1;
        auto img = decodeBtiImage(data, 0, 9, 2, 2, 0, Endian::big, palette, 1);
        expect(img.rgba[0] == 255 && img.rgba[3] == 255, "C8 palette entry 0 should map white");
        expect(img.rgba[4] == 0 && img.rgba[7] == 255, "C8 palette entry 1 should map black");
        expect(img.rgba[8] == 255, "C8 palette entry 0 for pixel (0,1) wrong");
        expect(img.rgba[12] == 0 && img.rgba[15] == 255, "C8 palette entry 1 for pixel (1,1) wrong");
    }

    // CMPR (format 14): one 8x8 macro block = 32 bytes; colorA > colorB -> color3 = (2*c0+c1)/3.
    {
        std::vector<std::uint8_t> data(32, 0);
        data[0] = 0xFF; data[1] = 0xFF; // colorA = 0xFFFF (white)
        data[4] = 0x80;                 // pixel(0,0) index 2 -> color3 = 170
        auto img = decodeBtiImage(data, 0, 14, 4, 4, 0, Endian::big);
        expect(img.rgba[0] == 170 && img.rgba[1] == 170 && img.rgba[2] == 170 && img.rgba[3] == 255,
               "CMPR color3 interpolation was wrong");
    }

    // CMPR: colorA <= colorB -> fourth color is transparent (alpha 0).
    {
        std::vector<std::uint8_t> data(32, 0);
        data[2] = 0xFF; data[3] = 0xFF; // colorB = 0xFFFF (white), colorA = 0
        data[4] = 0xC0;                 // pixel(0,0) index 3 -> color4 = transparent white
        auto img = decodeBtiImage(data, 0, 14, 4, 4, 0, Endian::big);
        expect(img.rgba[0] == 255 && img.rgba[1] == 255 && img.rgba[2] == 255 && img.rgba[3] == 0,
               "CMPR transparent color4 must have alpha 0");
    }

    // parseBti: standalone .bti entry at entryOffset 0 (I8, 4x4).
    {
        std::vector<std::uint8_t> blob(64, 0);
        blob[0] = 1;
        blob[2] = 0; blob[3] = 4;
        blob[4] = 0; blob[5] = 4;
        blob[24] = 0;
        blob[28] = 0; blob[29] = 0; blob[30] = 0; blob[31] = 32;
        for (int i = 0; i < 16; ++i) {
            blob[32 + i] = static_cast<std::uint8_t>(i * 8 + i);
        }
        const auto bti = parseBti(blob, 0, Endian::big);
        expect(bti.width == 4 && bti.height == 4, "parseBti width/height wrong");
        expect(bti.mipmaps.size() == 1, "parseBti should decode one mip level");
        expect(bti.mipmaps[0].rgba.size() == 4u * 4u * 4u, "parseBti mip byte count wrong");
        expect(bti.mipmaps[0].rgba[0] == 0 && bti.mipmaps[0].rgba[3] == 255, "parseBti I8 pixel (0,0) wrong");
    }

    // parseBti: embedded entry at a NON-zero entryOffset (absolute offset fix).
    {
        constexpr std::size_t prefix = 16;
        std::vector<std::uint8_t> blob(prefix + 32 + 32, 0);
        blob[prefix + 0] = 1;
        blob[prefix + 2] = 0; blob[prefix + 3] = 2;
        blob[prefix + 4] = 0; blob[prefix + 5] = 2;
        blob[prefix + 24] = 0;
        blob[prefix + 28] = 0; blob[prefix + 29] = 0; blob[prefix + 30] = 0; blob[prefix + 31] = 32;
        blob[prefix + 32 + 0] = 0x80;
        blob[prefix + 32 + 1] = 0x10;
        blob[prefix + 32 + 8] = 0xFF;
        blob[prefix + 32 + 9] = 0x00;
        const auto bti = parseBti(blob, prefix, Endian::big);
        expect(bti.width == 2 && bti.height == 2, "embedded parseBti dimensions wrong");
        expect(bti.mipmaps.size() == 1, "embedded parseBti mip count wrong");
        expect(bti.mipmaps[0].rgba[0] == 0x80 && bti.mipmaps[0].rgba[3] == 255, "embedded parseBti (0,0) wrong");
        expect(bti.mipmaps[0].rgba[8] == 0xFF, "embedded parseBti (0,1) wrong");
        expect(bti.mipmaps[0].rgba[12] == 0x00 && bti.mipmaps[0].rgba[15] == 255, "embedded parseBti (1,1) wrong");
    }
}


void testJsonRoundTrip() {
    using whitehole::util::JsonArray;
    using whitehole::util::JsonObject;
    using whitehole::util::JsonValue;
    JsonObject root;
    root["name"] = JsonValue("Kinopio");
    root["count"] = JsonValue(3.0);
    root["ok"] = JsonValue(true);
    JsonArray items;
    items.emplace_back("a");
    items.emplace_back(1.0);
    root["items"] = JsonValue(std::move(items));
    const std::string text = whitehole::util::serializeJson(JsonValue(root));
    const JsonValue parsed = whitehole::util::parseJson(text);
    expect(parsed.strAt("name") == "Kinopio", "JSON string round trip failed");
    expect(parsed.at("count").asNumber() == 3.0, "JSON number round trip failed");
    expect(parsed.at("ok").asBool() == true, "JSON bool round trip failed");
    expect(parsed.at("items").asArray().size() == 2, "JSON array round trip failed");
    bool rejected = false;
    try {
        (void)whitehole::util::parseJson("{bad}");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "malformed JSON was not rejected");
}

void testSettingsRoundTrip() {
    whitehole::app::Settings settings;
    TemporaryDirectory temp;
    settings.setConfigPath(temp.path / "settings.json");
    settings.lastGameDir = "C:/games/smg2";
    settings.darkMode = false;
    settings.showPaths = false;
    settings.pushRecentMap("C:/games/map.arc");
    settings.pushRecentMap("C:/games/other.arc");
    settings.pushRecentMap("C:/games/map.arc");
    expect(settings.recentMaps.size() == 2, "recent maps should dedupe");
    expect(settings.recentMaps.front() == "C:/games/map.arc", "recent maps order wrong");
    settings.save();
    whitehole::app::Settings loaded;
    loaded.setConfigPath(temp.path / "settings.json");
    loaded.load();
    expect(loaded.lastGameDir == "C:/games/smg2", "settings lastGameDir mismatch");
    expect(loaded.darkMode == false, "settings darkMode mismatch");
    expect(loaded.showPaths == false, "settings showPaths mismatch");
    expect(loaded.recentMaps.size() == 2, "settings recentMaps mismatch");
}

void testObjectDatabase() {
    TemporaryDirectory temp;
    const auto path = temp.path / "objectdb.json";
    {
        std::ofstream out(path, std::ios::binary);
        out << "{\"Objects\":["
               "{\"InternalName\":\"Kinopio\",\"SimpleName\":\"Toad\",\"Category\":\"NPC\"},"
               "{\"InternalName\":\"Kuribo\",\"Category\":\"Enemy\"}]}";
    }
    whitehole::db::ObjectDatabase db;
    db.load(path);
    expect(db.size() == 2, "objectdb size wrong");
    expect(db.contains("Kinopio"), "objectdb missing Kinopio");
    expect(db.displayName("Kinopio") == "Toad", "objectdb display name wrong");
    expect(db.displayName("Missing") == "\"Missing\"", "objectdb fallback wrong");
}

void testDataHolderRoundTrip() {
    TemporaryDirectory temp;
    const auto base = temp.path / "base";
    std::filesystem::create_directories(base);
    {
        std::ofstream out(base / "test.json");
        out << "{\"Items\": [\"a\",\"b\"]}";
    }
    whitehole::db::DataHolderBase holder("test.json", "/hints.json", true);
    holder.setBaseGameRoot(base);
    holder.initBaseGame();
            expect(holder.dataPresent(), "base game data should be present");
    expect(holder.root().at("Items").asArray().size() == 2, "base root should parse");
}

void testDbHelpersRoundTrip() {
        TemporaryDirectory temp;
    const auto base = temp.path / "base";
    std::filesystem::create_directories(base / "data");

    // Hints: top-level "Hints" array with Game field for filtering.
    {
        std::ofstream out(base / "data" / "hints.json");
        out << "{\"Hints\":[{\"Game\":0,\"Hint\":\"hello\",\"Name\":\"Test\"},{\"Game\":1,\"Hint\":\"smg1only\"}]}";
    }
    whitehole::db::Hints hints;
    hints.setBaseGameRoot(base);
    hints.initBaseGame();
    hints.load(2);
    expect(hints.hints().size() == 1, "hints should load one entry for SMG2");
    expect(hints.hints()[0].hint == "hello", "hints hint wrong");
    hints.load(1);
    expect(hints.hints().size() == 2, "hints should load two entries for SMG1");

    // AreaManagerLimits: aliases + limits per game.
    {
        std::ofstream out(base / "data" / "areamanagerlimits.json");
        out << "{\"AreaManagerAliases\":{\"SMG1\":{\"Cube\":\"Area\"}},\"AreaManagers\":{\"SMG1\":{\"Area\":\"5\"}}}";
    }
    whitehole::db::AreaManagerLimits limits;
    limits.setBaseGameRoot(base);
    limits.initBaseGame();
    limits.load(1);
    expect(limits.resolveAlias("Cube") == "Area", "alias resolve wrong");
    expect(limits.resolveAlias("Missing") == "Missing", "alias passthrough wrong");
    expect(limits.limitFor("Cube") == "5", "limit via alias wrong");
    expect(limits.limitFor("Missing") == "", "limit missing empty");

    // Shortcuts: flat root.
    {
        std::ofstream out(base / "data" / "shortcuts.json");
        out << "{\"save\":\"Ctrl+S\"}";
    }
    whitehole::db::Shortcuts shortcuts;
    shortcuts.setBaseGameRoot(base);
    shortcuts.initBaseGame();
    shortcuts.load();
    expect(shortcuts.get("save") == "Ctrl+S", "shortcut get wrong");
    expect(shortcuts.get("missing") == "", "shortcut missing empty");

    // ModelSubstitutions: flat lowercase-keyed map.
    {
        std::ofstream out(base / "data" / "modelsubstitutions.json");
        out << "{\"dumptr\":\"Kinopio\"}";
    }
    whitehole::db::ModelSubstitutions subs;
    subs.setBaseGameRoot(base);
    subs.initBaseGame();
    subs.load();
    expect(subs.substitute("Dumptr") == "Kinopio", "case-insensitive substitute");
    expect(subs.substitute("missing") == "", "no substitute empty");

    // SpecialRenderers: array with ObjectName/ClassName + RendererType.
    {
        std::ofstream out(base / "data" / "specialrenderers.json");
        out << "{\"SpecialRenderers\":[{\"ObjectName\":\"GoalA\",\"RendererType\":\"Special\"}]}";
    }
    whitehole::db::SpecialRenderers special;
    special.setBaseGameRoot(base);
    special.initBaseGame();
    special.load();
    expect(special.lookup("GoalA") == "Special", "special renderer lookup");
    expect(special.lookup("Unknown") == "", "special renderer unknown empty");
}

void testObjectDatabaseV2() {
    TemporaryDirectory temp;
    const auto path = temp.path / "objectdb.json";
    {
        std::ofstream out(path, std::ios::binary);
        out << R"({
  "Timestamp": 1234567890,
  "Categories": [{"Key":"enemy","Description":"Enemies"},{"Key":"stagepart","Description":"Stage Parts"}],
  "Classes": [
    {"InternalName":"SampleObj","Name":"SampleObj","Notes":"A test class","Games":3,"Progress":1,
     "Parameters":{
        "Obj_arg0":{"Name":"Range","Type":"Float","Games":3,"Needed":true,"Description":"How far.","Values":[],"Exclusives":[]},
        "Obj_arg1":{"Name":"Mode","Type":"Integer","Games":3,"Needed":false,"Description":"Pick one.","Values":[{"Value":0,"Notes":"Off"},{"Value":1,"Notes":"On"}],"Exclusives":[]},
        "Obj_arg2":{"Name":"Only SMG2","Type":"Integer","Games":2,"Needed":false,"Description":"","Values":[],"Exclusives":[]},
        "Obj_arg3":{"Name":"Special","Type":"Boolean","Games":3,"Needed":false,"Description":"","Values":[],"Exclusives":["Kinopio"]},
        "SW_A":{"Games":3,"Needed":false,"Description":"Switch A.","Values":[],"Exclusives":[]}
     }}
  ],
  "Objects": [
    {"InternalName":"Kinopio","ClassNameSMG1":"SampleObj","ClassNameSMG2":"SampleObj","Name":"Toad",
     "Notes":"Friendly.","Category":"npc","ListSMG1":"ObjInfo","ListSMG2":"ObjInfo","File":"Map","Games":3,
     "Progress":1,"IsUnused":false,"IsLeftover":false},
    {"InternalName":"OldThing","ClassNameSMG1":"SampleObj","ClassNameSMG2":"SampleObj","Name":"Leftover",
     "Category":"stagepart","Games":1,"IsUnused":true,"IsLeftover":true}
  ]
})";
    }
    whitehole::db::ObjectDatabase db;
    db.load(path);

    expect(db.size() == 2, "objectdb v2 object count wrong");
    expect(db.classCount() == 1, "objectdb v2 class count wrong");
    expect(db.categoryCount() == 2, "objectdb v2 category count wrong");
    expect(db.timestamp() == 1234567890U, "objectdb v2 timestamp wrong");

    const auto* kinopio = db.find("Kinopio");
    expect(kinopio != nullptr, "objectdb v2 missing Kinopio");
    expect(kinopio->name == "Toad", "objectdb v2 display name wrong");
    expect(kinopio->description == "Friendly.", "objectdb v2 notes wrong");
    expect(kinopio->className(1) == "SampleObj", "objectdb v2 smg1 class wrong");
    expect(kinopio->className(2) == "SampleObj", "objectdb v2 smg2 class wrong");
    expect(kinopio->list(2) == "ObjInfo", "objectdb v2 list wrong");
    expect(!kinopio->unused, "objectdb v2 unused flag wrong");

    expect(db.findClass("SampleObj") != nullptr, "objectdb v2 class lookup failed");
    expect(db.classForObject("Kinopio", 2) != nullptr, "objectdb v2 class-for-object failed");

    // Labels, descriptions and kinds come from the class metadata.
    expect(db.propertyLabel("Kinopio", "Obj_arg0", 2) == "Range", "objectdb v2 label wrong");
    expect(db.propertyDescription("Kinopio", "Obj_arg0", 2) == "How far.", "objectdb v2 description wrong");
    const auto* range = db.propertyForObject("Kinopio", "Obj_arg0", 2);
    expect(range != nullptr, "objectdb v2 float property missing");
    expect(range->kind == whitehole::db::PropertyKind::Float, "objectdb v2 float kind wrong");
    expect(range->needed, "objectdb v2 needed flag wrong");

    const auto* mode = db.propertyForObject("Kinopio", "Obj_arg1", 2);
    expect(mode != nullptr, "objectdb v2 list property missing");
    expect(mode->kind == whitehole::db::PropertyKind::IntList, "objectdb v2 list kind wrong");
    expect(mode->values.size() == 2, "objectdb v2 values count wrong");
    expect(mode->values[1] == "1: On", "objectdb v2 value formatting wrong");

    // Per-game filtering: Obj_arg2 only exists in SMG2.
    expect(db.propertyUsed("Kinopio", "Obj_arg2", 2), "objectdb v2 smg2-only property hidden");
    expect(!db.propertyUsed("Kinopio", "Obj_arg2", 1), "objectdb v2 smg2-only property leaked to smg1");

    // Exclusives: Obj_arg3 is listed for Kinopio only.
    expect(db.propertyUsed("Kinopio", "Obj_arg3", 2), "objectdb v2 exclusive property hidden");
    expect(!db.propertyUsed("OldThing", "Obj_arg3", 2), "objectdb v2 exclusive property leaked");

    // A parameter with no declared "Type" still becomes an integer cell, and an
    // unknown parameter falls back to its raw identifier as the label.
    const auto* switchA = db.propertyForObject("Kinopio", "SW_A", 2);
    expect(switchA != nullptr, "objectdb v2 untyped property missing");
    expect(switchA->kind == whitehole::db::PropertyKind::Integer, "objectdb v2 untyped kind wrong");
    expect(db.propertyLabel("Kinopio", "SomethingElse", 2) == "SomethingElse",
           "objectdb v2 unknown property label wrong");

    // Alias table (Java getPropertyInfoForObject).
    expect(whitehole::db::ObjectDatabase::aliasField("CommonPath_ID") == "Rail", "alias Rail wrong");
    expect(whitehole::db::ObjectDatabase::aliasField("CameraSetId") == "Camera", "alias Camera wrong");
    expect(whitehole::db::ObjectDatabase::aliasField("GroupId") == "Group", "alias Group wrong");
    expect(whitehole::db::ObjectDatabase::aliasField("MessageId") == "Message", "alias Message wrong");
    expect(whitehole::db::ObjectDatabase::aliasField("Plain") == "Plain", "alias passthrough wrong");

    // Game availability (Java ObjectSelectForm filter).
    expect(db.objectAvailable("Kinopio", 1) && db.objectAvailable("Kinopio", 2),
           "objectdb v2 availability wrong");
    expect(db.objectAvailable("OldThing", 1), "objectdb v2 smg1-only availability wrong");
    expect(!db.objectAvailable("OldThing", 2), "objectdb v2 smg1-only leaked to smg2");

    // Search matches display name, internal name and class name.
    expect(db.search("toad", 2).size() == 1, "objectdb v2 search by display name");
    expect(db.search("kinop", 2).size() == 1, "objectdb v2 search by internal name");
    expect(db.search("sampleobj", 1).size() == 2, "objectdb v2 search by class name");
    expect(db.search("nothinghere", 2).empty(), "objectdb v2 search false positive");

    // Categories keep declaration order.
    expect(db.categories()[0].key == "enemy", "objectdb v2 category order wrong");
    expect(db.categories()[1].description == "Stage Parts", "objectdb v2 category description wrong");
}

void testObjectDatabaseCache() {
    TemporaryDirectory temp;
    const auto jsonPath = temp.path / "objectdb.json";
    const auto cachePath = temp.path / "cache" / "objectdb.cache";

    const auto writeJson = [&](std::string_view name) {
        std::ofstream out(jsonPath, std::ios::binary | std::ios::trunc);
        out << "{\"Timestamp\":7,\"Classes\":[{\"InternalName\":\"C\",\"Parameters\":{}}],"
               "\"Objects\":[{\"InternalName\":\"Obj\",\"Name\":\""
            << name << "\",\"ClassNameSMG1\":\"C\",\"ClassNameSMG2\":\"C\",\"Games\":3}]}";
    };

    writeJson("First");
    whitehole::db::ObjectDatabase db;
    db.load(jsonPath, cachePath);
    expect(!db.cacheLoaded(), "objectdb cache should not be used before it exists");
    expect(db.displayName("Obj") == "First", "objectdb cache source value wrong");
    expect(std::filesystem::exists(cachePath), "objectdb cache was not written");

    whitehole::db::ObjectDatabase cached;
    cached.load(jsonPath, cachePath);
    expect(cached.cacheLoaded(), "objectdb cache was not used on the second load");
    expect(cached.size() == 1, "objectdb cache object count wrong");
    expect(cached.classCount() == 1, "objectdb cache class count wrong");
    expect(cached.displayName("Obj") == "First", "objectdb cache display name wrong");
    expect(cached.timestamp() == 7U, "objectdb cache timestamp wrong");

    // Rewriting the JSON must invalidate the compiled cache. The source
    // timestamp is pushed into the future so the test does not depend on clock
    // granularity.
    writeJson("Second");
    std::error_code error;
    const auto bumped = std::filesystem::last_write_time(jsonPath, error) + std::chrono::seconds(10);
    std::filesystem::last_write_time(jsonPath, bumped, error);

    whitehole::db::ObjectDatabase reparsed;
    reparsed.load(jsonPath, cachePath);
    expect(!reparsed.cacheLoaded(), "objectdb cache was not invalidated");
    expect(reparsed.displayName("Obj") == "Second", "objectdb cache invalidation value wrong");
}

void testRealObjectDatabase() {
#ifdef WHITEHOLE_SOURCE_DIR
    const auto path = std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data" / "objectdb.json";
    if (!std::filesystem::exists(path)) return; // optional bulky data

    whitehole::db::ObjectDatabase db;
    db.load(path);
    expect(db.size() > 2000, "real objectdb object count unexpectedly small");
    expect(db.classCount() > 800, "real objectdb class count unexpectedly small");
    expect(db.categoryCount() >= 10, "real objectdb categories unexpectedly few");
    expect(db.timestamp() > 0, "real objectdb timestamp missing");

    // Whenever the database declares a class name for an object it must resolve,
    // otherwise the property grid would silently come up empty.
    std::size_t declared = 0;
    std::size_t resolved = 0;
    for (const auto& name : db.names()) {
        const auto* info = db.find(name);
        if (info == nullptr || info->classNameSmg2.empty()) continue;
        ++declared;
        if (db.findClass(info->classNameSmg2) != nullptr) ++resolved;
    }
    expect(declared > 2000, "real objectdb declares too few SMG2 class names");
    expect(resolved == declared, "real objectdb has unresolvable SMG2 class names");

    const auto* rail = db.findClass("RailMoveObj");
    expect(rail != nullptr, "real objectdb missing the RailMoveObj class");
    expect(!rail->properties.empty(), "real objectdb RailMoveObj has no parameters");
#endif
}

} // namespace

int main() {
    try {
        testBinaryData();
        testDirectoryFilesystem();
        testYaz0();
        testMath();
        testViewportCamera();
        testViewportScene();
        testObjectVisual();
        testHashes();
        testBcsvEndianness();
        testRarcEndianness();
        testProjectArchives();
        testArchiveTableEdit();
        testNameTables();
        testStageAndGameModels();
        testBtiDecoding();
        testJsonRoundTrip();
        testSettingsRoundTrip();
        testObjectDatabase();
        testObjectDatabaseV2();
        testObjectDatabaseCache();
        testRealObjectDatabase();
        testDataHolderRoundTrip();
        testDbHelpersRoundTrip();
        std::cout << "All Whitehole native core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}

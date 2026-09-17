#include "whitehole/db/name_table.hpp"
#include "whitehole/io/binary_file.hpp"
#include "whitehole/io/directory_filesystem.hpp"
#include "whitehole/io/rarc.hpp"
#include "whitehole/io/yaz0.hpp"
#include "whitehole/math/geometry.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/game_archive.hpp"
#include "whitehole/smg/hash.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
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
    const std::string pattern = "Whitehole Neo native archive support! ";
    for (int index = 0; index < 100; ++index) {
        input.insert(input.end(), pattern.begin(), pattern.end());
        input.push_back(static_cast<std::uint8_t>(index));
    }
    const auto compressed = whitehole::io::yaz0::compress(input);
    expect(whitehole::io::yaz0::isCompressed(compressed), "Yaz0 output has no header");
    expect(compressed.size() < input.size(), "Yaz0 did not compress repetitive data");
    expect(whitehole::io::yaz0::decompress(compressed) == input, "Yaz0 round trip failed");
    expect(whitehole::io::yaz0::compress(compressed) == compressed, "Yaz0 double compression changed data");
    expect(whitehole::io::yaz0::decompress(whitehole::io::yaz0::compress({})).empty(),
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

} // namespace

int main() {
    try {
        testBinaryData();
        testDirectoryFilesystem();
        testYaz0();
        testMath();
        testHashes();
        testBcsvEndianness();
        testRarcEndianness();
        testProjectArchives();
        testArchiveTableEdit();
        testNameTables();
        testStageAndGameModels();
        std::cout << "All Whitehole native core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}

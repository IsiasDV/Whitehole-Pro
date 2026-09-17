#include "whitehole/app/application.hpp"

#include "whitehole/io/binary_file.hpp"
#include "whitehole/io/rarc.hpp"
#include "whitehole/io/yaz0.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/field_hashes.hpp"
#include "whitehole/smg/game_archive.hpp"
#include "whitehole/smg/hash.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace whitehole::app {
namespace {

std::filesystem::path executableDirectory(const std::filesystem::path& executable) {
    auto path = executable;
    if (path.has_parent_path()) {
        return path.parent_path();
    }
    return std::filesystem::current_path();
}

void printUsage() {
    std::cout
        << "Whitehole Pro C++ 0.2.0\n"
        << "Native Super Mario Galaxy editor core\n\n"
        << "Usage:\n"
        << "  whitehole-neo\n"
        << "  whitehole-neo gui\n"
        << "  whitehole-neo game list <game-directory>\n"
        << "  whitehole-neo galaxy inspect <game-directory> <galaxy>\n"
        << "  whitehole-neo zone objects <game-directory> <zone>\n"
        << "  whitehole-neo map objects <archive.arc>\n"
        << "  whitehole-neo archive list <archive.arc>\n"
        << "  whitehole-neo archive extract <archive.arc> <directory>\n"
        << "  whitehole-neo archive replace <archive.arc> <entry> <input> <output.arc>\n"
        << "  whitehole-neo yaz0 compress <input> <output>\n"
        << "  whitehole-neo yaz0 decompress <input> <output>\n"
        << "  whitehole-neo bcsv inspect <table.bcsv> [--little]\n"
        << "  whitehole-neo bcsv roundtrip <input.bcsv> <output.bcsv> [--little]\n"
        << "  whitehole-neo hash <field-name>\n";
}

int archiveCommand(int argc, char** argv) {
    if (argc < 4) {
        throw std::runtime_error("archive requires an operation and input path");
    }
    const std::string operation = argv[2];
    auto archive = whitehole::io::RarcArchive::open(argv[3]);

    if (operation == "list") {
        std::cout << "Archive: " << archive.rootName()
                  << " (" << (archive.endian() == whitehole::io::Endian::big ? "big" : "little")
                  << "-endian, " << (archive.wasCompressed() ? "Yaz0 compressed" : "uncompressed") << ")\n";
        for (const auto& entry : archive.entries()) {
            std::cout << (entry.directory ? "d " : "f ") << std::setw(10) << entry.size << "  " << entry.path << '\n';
        }
        return 0;
    }
    if (operation == "extract") {
        if (argc != 5) {
            throw std::runtime_error("archive extract requires a destination directory");
        }
        archive.extractAll(argv[4]);
        std::cout << "Extracted " << archive.entries().size() << " entries to " << argv[4] << '\n';
        return 0;
    }
    if (operation == "replace") {
        if (argc != 7) {
            throw std::runtime_error("archive replace requires an entry, replacement file, and output archive");
        }
        archive.replace(argv[4], whitehole::io::readFile(argv[5]));
        whitehole::io::writeFile(argv[6], archive.serialize(archive.wasCompressed()));
        std::cout << "Replaced " << argv[4] << " and wrote " << argv[6] << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown archive operation: " + operation);
}

int yaz0Command(int argc, char** argv) {
    if (argc != 5) {
        throw std::runtime_error("yaz0 requires an operation, input path, and output path");
    }
    const std::string operation = argv[2];
    const auto input = whitehole::io::readFile(argv[3]);
    if (operation == "compress") {
        whitehole::io::writeFile(argv[4], whitehole::io::yaz0::compress(input));
    } else if (operation == "decompress") {
        whitehole::io::writeFile(argv[4], whitehole::io::yaz0::decompress(input));
    } else {
        throw std::runtime_error("Unknown Yaz0 operation: " + operation);
    }
    std::cout << operation << "ed " << argv[3] << " -> " << argv[4] << '\n';
    return 0;
}

int bcsvCommand(int argc, char** argv) {
    if (argc < 4) {
        throw std::runtime_error("bcsv requires an operation and input path");
    }
    const std::string operation = argv[2];
    const bool littleEndian = std::string(argv[argc - 1]) == "--little";
    const auto endian = littleEndian ? whitehole::io::Endian::little : whitehole::io::Endian::big;
    const auto table = whitehole::smg::BcsvTable::open(argv[3], endian);
    smg::FieldHashes hashes;
    hashes.loadFile(dataDirectory(argv[0]) / "hashlookup.txt");

    if (operation == "inspect") {
        if (argc != 4 && !(argc == 5 && littleEndian)) {
            throw std::runtime_error("bcsv inspect accepts only an optional --little flag");
        }
        std::cout << table.rows().size() << " rows, " << table.fields().size()
                  << " fields, " << table.entrySize() << " bytes per row\n";
        for (const auto& field : table.fields()) {
            std::cout << hashes.nameOf(field.hash) << '\t';
        }
        std::cout << '\n';
        for (const auto& row : table.rows()) {
            for (const auto& value : row.values) {
                std::cout << whitehole::smg::toString(value) << '\t';
            }
            std::cout << '\n';
        }
        return 0;
    }
    if (operation == "roundtrip") {
        const auto expectedArguments = littleEndian ? 6 : 5;
        if (argc != expectedArguments) {
            throw std::runtime_error("bcsv roundtrip requires an output path and optional --little flag");
        }
        whitehole::io::writeFile(argv[4], table.serialize());
        std::cout << "Rewrote " << table.rows().size() << " rows to " << argv[4] << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown BCSV operation: " + operation);
}

int gameCommand(int argc, char** argv) {
    if (argc != 4 || std::string(argv[2]) != "list") {
        throw std::runtime_error("game list requires a game directory");
    }
    smg::GameArchive game(argv[3]);
    if (game.gameType() == 0) {
        throw std::runtime_error("Directory is not an SMG1 or SMG2 workspace");
    }
    std::cout << "SMG" << game.gameType() << " workspace: " << argv[3] << '\n';
    std::cout << "Galaxies (" << game.galaxies().size() << "):\n";
    for (const auto& galaxy : game.galaxies()) {
        std::cout << "  " << galaxy << '\n';
    }
    std::cout << "Zones (" << game.zones().size() << "):\n";
    for (const auto& zone : game.zones()) {
        std::cout << "  " << zone << '\n';
    }
    return 0;
}

int galaxyCommand(int argc, char** argv) {
    if (argc != 5 || std::string(argv[2]) != "inspect") {
        throw std::runtime_error("galaxy inspect requires a game directory and galaxy name");
    }
    smg::GameArchive game(argv[3]);
    const auto galaxy = game.openGalaxy(argv[4]);
    std::cout << galaxy.name() << " zones:\n";
    for (const auto& zone : galaxy.zones()) {
        std::cout << "  " << zone << '\n';
    }
    return 0;
}

void printObjects(const smg::StageArchive& stage) {
    std::cout << stage.stageName() << ": " << stage.objects().size() << " objects\n";
    for (const auto& object : stage.objects()) {
        std::cout << "  [" << object.kind << "/" << object.layer << "] " << object.name
                  << "  (" << object.position.x << ", " << object.position.y << ", " << object.position.z << ")\n";
    }
}

int zoneCommand(int argc, char** argv) {
    if (argc != 5 || std::string(argv[2]) != "objects") {
        throw std::runtime_error("zone objects requires a game directory and zone name");
    }
    smg::GameArchive game(argv[3]);
    printObjects(smg::StageArchive::open(game.filesystem(), argv[4], game.gameType()));
    return 0;
}

int mapCommand(int argc, char** argv) {
    if (argc != 4 || std::string(argv[2]) != "objects") {
        throw std::runtime_error("map objects requires an archive path");
    }
    printObjects(smg::StageArchive::openMapFile(argv[3]));
    return 0;
}

} // namespace

std::filesystem::path dataDirectory(const std::filesystem::path& executable) {
    const auto current = std::filesystem::current_path() / "data";
    if (std::filesystem::exists(current)) {
        return current;
    }
    const auto beside = executableDirectory(executable) / "data";
    if (std::filesystem::exists(beside)) {
        return beside;
    }
#ifdef WHITEHOLE_SOURCE_DIR
    return std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data";
#else
    return current;
#endif
}

int runCli(int argc, char** argv) {
    try {
        if (argc == 1) {
#ifdef _WIN32
            return runGui(argv[0]);
#else
            printUsage();
            return 0;
#endif
        }
        const std::string command = argv[1];
        if (command == "--help" || command == "-h") {
            printUsage();
            return 0;
        }
        if (command == "gui") {
            return runGui(argv[0]);
        }
        if (command == "archive") {
            return archiveCommand(argc, argv);
        }
        if (command == "yaz0") {
            return yaz0Command(argc, argv);
        }
        if (command == "bcsv") {
            return bcsvCommand(argc, argv);
        }
        if (command == "game") {
            return gameCommand(argc, argv);
        }
        if (command == "galaxy") {
            return galaxyCommand(argc, argv);
        }
        if (command == "zone") {
            return zoneCommand(argc, argv);
        }
        if (command == "map") {
            return mapCommand(argc, argv);
        }
        if (command == "hash") {
            if (argc != 3) {
                throw std::runtime_error("hash requires one field name");
            }
            std::cout << "JMap:      0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                      << whitehole::smg::jmapHash(argv[2]) << '\n'
                      << "SuperFast: 0x" << std::setw(8) << whitehole::smg::superFastHash(argv[2]) << '\n';
            return 0;
        }
        throw std::runtime_error("Unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "whitehole-neo: " << error.what() << '\n';
        return 1;
    }
}

} // namespace whitehole::app

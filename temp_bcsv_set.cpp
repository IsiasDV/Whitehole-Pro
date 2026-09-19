int bcsvSetCommand(int argc, char** argv) {
    if (argc < 7) {
        throw std::runtime_error("bcsv set requires: <input> <row> <field> <value> [--little] [--in-place]");
    }
    const std::filesystem::path inputPath = argv[3];
    const std::size_t rowIndex = static_cast<std::size_t>(std::stol(argv[4]));
    const std::string fieldName = argv[5];
    const std::string valueText = argv[6];
    bool littleEndian = false;
    bool inPlace = false;
    for (int i = 7; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--little") { littleEndian = true; }
        else if (flag == "--in-place") { inPlace = true; }
        else if (flag == "--json") { g_json = true; }
        else { throw std::runtime_error("Unknown flag: " + flag); }
    }
    const auto endian = littleEndian ? whitehole::io::Endian::little : whitehole::io::Endian::big;
    auto table = whitehole::smg::BcsvTable::open(inputPath, endian);
    if (rowIndex >= table.rows().size()) {
        throw std::runtime_error("Row index " + std::to_string(rowIndex) + " out of range (" + std::to_string(table.rows().size()) + " rows)");
    }
    const auto fit = table.fieldIndex(fieldName);
    if (!fit) { throw std::runtime_error("Field not found: " + fieldName); }
    const auto& field = table.fields()[*fit];
    auto& row = table.rows()[rowIndex];
    if (field.type == whitehole::smg::BcsvType::floatingPoint) { table.setFloat(row, fieldName, std::stof(valueText)); }
    else if (field.type == whitehole::smg::BcsvType::integer || field.type == whitehole::smg::BcsvType::integer2) { table.setInt(row, fieldName, std::stol(valueText)); }
    else if (field.type == whitehole::smg::BcsvType::shortInteger) { table.setInt(row, fieldName, static_cast<std::int16_t>(std::stol(valueText))); }
    else if (field.type == whitehole::smg::BcsvType::byte) { table.setInt(row, fieldName, static_cast<std::int8_t>(std::stol(valueText))); }
    else if (field.type == whitehole::smg::BcsvType::fixedString || field.type == whitehole::smg::BcsvType::stringOffset) { table.setString(row, fieldName, valueText); }
    else { throw std::runtime_error("Unsupported BCSV field type"); }
    const std::filesystem::path outPath = inPlace ? inputPath : inputPath.replace_extension(inputPath.extension().string() + ".edited");
    whitehole::io::writeFile(outPath, table.serialize());
    if (g_json) {
        whitehole::util::JsonObject j;
        j["command"] = "bcsv set";
        j["input"] = inputPath.string();
        j["row"] = static_cast<int>(rowIndex);
        j["field"] = fieldName;
        j["value"] = valueText;
        j["inPlace"] = inPlace;
        j["rows"] = static_cast<int>(table.rows().size());
        std::cout << whitehole::util::serializeJson(j) << "\n";
    } else {
        std::cout << "Set " << fieldName << " = " << valueText << " on row " << rowIndex << " in " << inputPath << "\n";
        if (!inPlace) std::cout << "Output: " << outPath << "\n";
    }
    return 0;
}

int bcsvAddCommand(int argc, char** argv) {
    if (argc < 4) { throw std::runtime_error("bcsv add requires an input path"); }
    const std::filesystem::path inputPath = argv[3];
    bool littleEndian = false;
    bool inPlace = false;
    std::vector<std::pair<std::string, std::string>> fieldValues;
    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--little") { littleEndian = true; }
        else if (a == "--in-place") { inPlace = true; }
        else if (a == "--json") { g_json = true; }
            fieldValues.emplace_back(a.substr(2, eq - 2), a.substr(eq + 1));
        } else { throw std::runtime_error("Unknown argument: " + a); }
    }
    const auto endian = littleEndian ? whitehole::io::Endian::little : whitehole::io::Endian::big;
    auto table = whitehole::smg::BcsvTable::open(inputPath, endian);
    if (table.fields().empty()) { throw std::runtime_error("Cannot add row to a BCSV table with no fields"); }
    const std::size_t newRow = table.addRow();
    for (const auto& [fname, fval] : fieldValues) {
        const auto fi = table.fieldIndex(fname);
        if (!fi) { throw std::runtime_error("Field not found: " + fname); }
        const auto& fld = table.fields()[*fi];
        auto& rw = table.rows()[newRow];
        if (fld.type == whitehole::smg::BcsvType::floatingPoint) { table.setFloat(rw, fname, std::stof(fval)); }
        else if (fld.type == whitehole::smg::BcsvType::integer || fld.type == whitehole::smg::BcsvType::integer2) { table.setInt(rw, fname, std::stol(fval)); }
        else if (fld.type == whitehole::smg::BcsvType::shortInteger) { table.setInt(rw, fname, static_cast<std::int16_t>(std::stol(fval))); }
        else if (fld.type == whitehole::smg::BcsvType::byte) { table.setInt(rw, fname, static_cast<std::int8_t>(std::stol(fval))); }
        else if (fld.type == whitehole::smg::BcsvType::fixedString || fld.type == whitehole::smg::BcsvType::stringOffset) { table.setString(rw, fname, fval); }
    }
    const std::filesystem::path outPath = inPlace ? inputPath : inputPath.replace_extension(inputPath.extension().string() + ".edited");
    whitehole::io::writeFile(outPath, table.serialize());
    if (g_json) {
        whitehole::util::JsonObject j;
        j["command"] = "bcsv add";
        j["input"] = inputPath.string();
        whitehole::util::JsonObject flds;
        for (const auto& [n, v] : fieldValues) { flds[n] = v; }
        j["fields"] = flds;
        j["inPlace"] = inPlace;
        j["newRow"] = static_cast<int>(newRow);
        std::cout << whitehole::util::serializeJson(j) << "\n";
    } else {
        std::cout << "Added row " << newRow << " to " << inputPath.filename().string() << " with " << fieldValues.size() << " field(s)\n";
        if (!inPlace) std::cout << "Output: " << outPath << "\n";
    }
    return 0;
}

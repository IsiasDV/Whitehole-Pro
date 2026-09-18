#include "whitehole/util/json.hpp"
#include <cctype>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
namespace whitehole::util {
namespace {
struct JParser {
    std::string t;
    std::size_t p{0};
    [[noreturn]] void fail(const char* w) {
        throw std::runtime_error(std::string("JSON error at ") + std::to_string(p) + ": " + w);
    }
    void ws() {
        while (p < t.size() && (t[p] == ' ' || t[p] == '\t' || t[p] == '\n' || t[p] == '\r')) ++p;
    }
    char peek() {
        if (p >= t.size()) fail("end of input");
        return t[p];
    }
    void expect(char c) {
        if (p >= t.size() || t[p] != c) fail("expected char");
        ++p;
    }
    JsonValue run();
    JsonValue obj();
    JsonValue arr();
    std::string str();
    JsonValue num();
    void utf8(unsigned c, std::string& o) {
        if (c < 0x80) o.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            o.push_back(static_cast<char>(0xC0 | (c >> 6)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            o.push_back(static_cast<char>(0xE0 | (c >> 12)));
            o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
};
JsonValue JParser::num() {
    const std::size_t s = p;
    if (p < t.size() && (t[p] == '-' || t[p] == '+')) ++p;
    while (p < t.size() && (std::isdigit(static_cast<unsigned char>(t[p])) || t[p] == '.' ||
                            t[p] == 'e' || t[p] == 'E' || t[p] == '+' || t[p] == '-')) ++p;
    if (s == p) fail("expected value");
    try {
        return JsonValue(std::stod(std::string(t.substr(s, p - s))));
    } catch (...) { fail("bad number"); }
}
std::string JParser::str() {
    expect('"');
    std::string o;
    while (true) {
        if (p >= t.size()) fail("unterminated string");
        const char c = t[p++];
        if (c == '"') return o;
        if (c != '\\') { o.push_back(c); continue; }
        if (p >= t.size()) fail("bad escape");
        const char e = t[p++];
        switch (e) {
            case '"': o.push_back('"'); break;
            case '\\': o.push_back('\\'); break;
            case '/': o.push_back('/'); break;
            case 'n': o.push_back('\n'); break;
            case 'r': o.push_back('\r'); break;
            case 't': o.push_back('\t'); break;
            case 'b': o.push_back('\b'); break;
            case 'f': o.push_back('\f'); break;
            case 'u': {
                if (p + 4 > t.size()) fail("bad unicode");
                unsigned code = 0;
                for (int i = 0; i < 4; ++i) {
                    const char h = t[p++];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                    else fail("bad hex");
                }
                utf8(code, o);
                break;
            }
            default: fail("unknown escape");
        }
    }
}
JsonValue JParser::arr() {
    expect('[');
    JsonArray a;
    ws();
    if (peek() == ']') { ++p; return JsonValue(std::move(a)); }
    while (true) {
        a.push_back(run());
        ws();
        const char c = peek();
        if (c == ',') { ++p; continue; }
        if (c == ']') { ++p; break; }
        fail("expected , or ]");
    }
    return JsonValue(std::move(a));
}
JsonValue JParser::obj() {
    expect('{');
    JsonObject o;
    ws();
    if (peek() == '}') { ++p; return JsonValue(std::move(o)); }
    while (true) {
        ws();
        if (peek() != '"') fail("key must be string");
        std::string k = str();
        ws();
        expect(':');
        o.emplace(std::move(k), run());
        ws();
        const char c = peek();
        if (c == ',') { ++p; continue; }
        if (c == '}') { ++p; break; }
        fail("expected , or }");
    }
    return JsonValue(std::move(o));
}
JsonValue JParser::run() {
    ws();
    const char c = peek();
    if (c == '{') return obj();
    if (c == '[') return arr();
    if (c == '"') return JsonValue(str());
    if (c == 't') {
        if (t.substr(p, 4) == "true") { p += 4; return JsonValue(true); }
        fail("bad literal");
    }
    if (c == 'f') {
        if (t.substr(p, 5) == "false") { p += 5; return JsonValue(false); }
        fail("bad literal");
    }
    if (c == 'n') {
        if (t.substr(p, 4) == "null") { p += 4; return JsonValue(); }
        fail("bad literal");
    }
    return num();
}
void jesc(std::ostringstream& o, std::string_view s) {
    o.put('"');
    for (const char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[7];
                    std::snprintf(b, sizeof(b), "\\u%04x", c);
                    o << b;
                } else o.put(c);
        }
    }
    o.put('"');
}
void jwrite(std::ostringstream& o, const JsonValue& v, int in) {
    const std::string pad(static_cast<std::size_t>(in) * 2, ' ');
    const std::string kid(static_cast<std::size_t>(in + 1) * 2, ' ');
    if (v.isNull()) { o << "null"; return; }
    if (v.isBool()) { o << (v.asBool() ? "true" : "false"); return; }
    if (v.isNumber()) { o << v.asNumber(); return; }
    if (v.isString()) { jesc(o, v.asString()); return; }
    if (v.isArray()) {
        const auto& a = v.asArray();
        if (a.empty()) { o << "[]"; return; }
        o << "[\n";
        for (std::size_t i = 0; i < a.size(); ++i) {
            o << kid;
            jwrite(o, a[i], in + 1);
            if (i + 1 < a.size()) o.put(',');
            o.put('\n');
        }
        o << pad << "]";
        return;
    }
    const auto& e = v.asObject();
    if (e.empty()) { o << "{}"; return; }
    o << "{\n";
    std::size_t i = 0;
    for (const auto& kv : e) {
        o << kid;
        jesc(o, kv.first);
        o << ": ";
        jwrite(o, kv.second, in + 1);
        if (++i < e.size()) o.put(',');
        o.put('\n');
    }
    o << pad << "}";
}
} // namespace
JsonValue parseJson(std::string_view text) {
    JParser q;
    q.t = std::string(text);
    q.p = 0;
    JsonValue v = q.run();
    q.ws();
    if (q.p != q.t.size()) throw std::runtime_error("JSON error: trailing chars");
    return v;
}
std::string serializeJson(const JsonValue& v) {
    std::ostringstream o;
    jwrite(o, v, 0);
    o.put('\n');
    return o.str();
}
} // namespace whitehole::util

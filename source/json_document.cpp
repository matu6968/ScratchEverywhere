#include "json_document.hpp"

#include <fstream>
#include <json_dom.hpp>
#include <log.hpp>
#include <simdjson.h>
#include <sstream>

namespace {

std::string escapeJsonString(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

void dumpValue(const JsonValue &value, std::ostringstream &out, int indent, int depth) {
    const std::string lineBreak = indent > 0 ? "\n" : "";
    const std::string indentStep(indent > 0 ? static_cast<size_t>(indent) : 0, ' ');
    const std::string currentIndent = indent > 0 ? std::string(static_cast<size_t>(depth * indent), ' ') : "";

    switch (value.type) {
    case JsonValue::Type::Null:
        out << "null";
        break;
    case JsonValue::Type::Bool:
        out << (value.boolValue ? "true" : "false");
        break;
    case JsonValue::Type::Number: {
        std::ostringstream number;
        number << value.numberValue;
        std::string asString = number.str();
        if (asString.find('.') == std::string::npos && asString.find('e') == std::string::npos) {
            out << asString << ".0";
        } else {
            out << asString;
        }
        break;
    }
    case JsonValue::Type::String:
        out << '"' << escapeJsonString(value.stringValue) << '"';
        break;
    case JsonValue::Type::Array:
        out << '[' << lineBreak;
        for (size_t i = 0; i < value.arrayValue.size(); ++i) {
            if (i > 0) out << ',' << lineBreak;
            out << currentIndent << indentStep;
            dumpValue(value.arrayValue[i], out, indent, depth + 1);
        }
        if (!value.arrayValue.empty()) out << lineBreak << currentIndent;
        out << ']';
        break;
    case JsonValue::Type::Object:
        out << '{' << lineBreak;
        size_t index = 0;
        for (const auto &[key, child] : value.objectValue) {
            if (index++ > 0) out << ',' << lineBreak;
            out << currentIndent << indentStep << '"' << escapeJsonString(key) << "\": ";
            if (indent > 0) out << lineBreak << currentIndent << indentStep << indentStep;
            dumpValue(child, out, indent, depth + 1);
        }
        if (!value.objectValue.empty()) out << lineBreak << currentIndent;
        out << '}';
        break;
    }
}

} // namespace

bool JsonValue::is_null() const {
    return type == Type::Null;
}

bool JsonValue::is_bool() const {
    return type == Type::Bool;
}

bool JsonValue::is_number() const {
    return type == Type::Number;
}

bool JsonValue::is_string() const {
    return type == Type::String;
}

bool JsonValue::is_array() const {
    return type == Type::Array;
}

bool JsonValue::is_object() const {
    return type == Type::Object;
}

bool JsonValue::empty() const {
    switch (type) {
    case Type::Null:
        return true;
    case Type::String:
        return stringValue.empty();
    case Type::Array:
        return arrayValue.empty();
    case Type::Object:
        return objectValue.empty();
    default:
        return false;
    }
}

bool JsonValue::get_bool() const {
    return boolValue;
}

double JsonValue::get_number() const {
    return numberValue;
}

std::string JsonValue::get_string() const {
    return stringValue;
}

bool JsonValue::contains(const std::string &key) const {
    return type == Type::Object && objectValue.find(key) != objectValue.end();
}

JsonValue &JsonValue::operator[](const std::string &key) {
    if (type != Type::Object) {
        type = Type::Object;
        objectValue.clear();
    }
    return objectValue[key];
}

const JsonValue &JsonValue::operator[](const std::string &key) const {
    static const JsonValue nullValue;
    if (type != Type::Object) return nullValue;
    auto it = objectValue.find(key);
    if (it == objectValue.end()) return nullValue;
    return it->second;
}

bool JsonValue::valueBool(const std::string &key, bool defaultValue) const {
    const JsonValue &child = (*this)[key];
    if (child.is_bool()) return child.get_bool();
    return defaultValue;
}

std::string JsonValue::valueString(const std::string &key, const std::string &defaultValue) const {
    const JsonValue &child = (*this)[key];
    if (child.is_string()) return child.get_string();
    return defaultValue;
}

JsonValue JsonValue::makeBool(bool value) {
    JsonValue json;
    json.type = Type::Bool;
    json.boolValue = value;
    return json;
}

JsonValue JsonValue::makeNumber(double value) {
    JsonValue json;
    json.type = Type::Number;
    json.numberValue = value;
    return json;
}

JsonValue JsonValue::makeString(std::string value) {
    JsonValue json;
    json.type = Type::String;
    json.stringValue = std::move(value);
    return json;
}

JsonValue JsonValue::makeObject() {
    JsonValue json;
    json.type = Type::Object;
    return json;
}

JsonValue JsonValue::makeArray() {
    JsonValue json;
    json.type = Type::Array;
    return json;
}

void JsonValue::push_back(const JsonValue &value) {
    if (type != Type::Array) {
        type = Type::Array;
        arrayValue.clear();
    }
    arrayValue.push_back(value);
}

void JsonValue::clear() {
    type = Type::Null;
    boolValue = false;
    numberValue = 0;
    stringValue.clear();
    arrayValue.clear();
    objectValue.clear();
}

std::string JsonValue::dump(int indent) const {
    std::ostringstream out;
    dumpValue(*this, out, indent, 0);
    return out.str();
}

JsonDocument JsonDocument::object() {
    JsonDocument document;
    document.root = JsonValue::makeObject();
    return document;
}

JsonDocument JsonDocument::parseContent(const std::string &content, bool &ok) {
    JsonDocument document;
    ok = false;
    if (content.empty()) return document;

    simdjson::dom::parser parser;
    simdjson::dom::element root;
    simdjson::padded_string padded(content);
    if (parser.parse(padded).get(root)) return document;

    document.root = fromSimdjsonElement(root);
    ok = true;
    return document;
}

JsonDocument JsonDocument::parseFile(const std::string &path, bool &ok, bool logWarning) {
    std::ifstream file(path);
    if (!file.good()) {
        ok = false;
        if (logWarning) Log::logWarning("Failed to open JSON file: " + path);
        return object();
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return parseContent(content, ok);
}

bool JsonDocument::is_null() const {
    return root.is_null();
}

bool JsonDocument::empty() const {
    return root.empty();
}

void JsonDocument::clear() {
    root.clear();
}

bool JsonDocument::contains(const std::string &key) const {
    return root.contains(key);
}

JsonValue &JsonDocument::operator[](const std::string &key) {
    return root[key];
}

const JsonValue &JsonDocument::operator[](const std::string &key) const {
    return root[key];
}

bool JsonDocument::valueBool(const std::string &key, bool defaultValue) const {
    return root.valueBool(key, defaultValue);
}

std::string JsonDocument::valueString(const std::string &key, const std::string &defaultValue) const {
    return root.valueString(key, defaultValue);
}

std::string JsonDocument::dump(int indent) const {
    return root.dump(indent);
}

JsonValue fromSimdjsonElement(simdjson::dom::element elem) {
    if (elem.is_null()) return JsonValue();

    if (elem.is_bool()) {
        bool value = false;
        if (elem.get_bool().get(value)) return JsonValue();
        return JsonValue::makeBool(value);
    }

    if (elem.is_number()) {
        double value = 0;
        if (elem.get_double().get(value)) return JsonValue();
        return JsonValue::makeNumber(value);
    }

    if (elem.is_string()) {
        std::string_view value;
        if (elem.get_string().get(value)) return JsonValue();
        return JsonValue::makeString(std::string(value));
    }

    if (elem.is_array()) {
        JsonValue json = JsonValue::makeArray();
        simdjson::dom::array array;
        if (elem.get_array().get(array)) return json;
        for (simdjson::dom::element item : array) {
            json.push_back(fromSimdjsonElement(item));
        }
        return json;
    }

    if (elem.is_object()) {
        JsonValue json = JsonValue::makeObject();
        simdjson::dom::object object;
        if (elem.get_object().get(object)) return json;
        for (auto [key, value] : object) {
            json.objectValue.emplace(std::string(key), fromSimdjsonElement(value));
        }
        return json;
    }

    return JsonValue();
}

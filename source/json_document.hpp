#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace simdjson {
namespace dom {
class element;
}
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::unordered_map<std::string, JsonValue> objectValue;

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;
    bool empty() const;

    bool get_bool() const;
    double get_number() const;
    std::string get_string() const;

    bool contains(const std::string &key) const;
    JsonValue &operator[](const std::string &key);
    const JsonValue &operator[](const std::string &key) const;

    bool valueBool(const std::string &key, bool defaultValue) const;
    std::string valueString(const std::string &key, const std::string &defaultValue) const;

    static JsonValue makeBool(bool value);
    static JsonValue makeNumber(double value);
    static JsonValue makeString(std::string value);
    static JsonValue makeObject();
    static JsonValue makeArray();

    void push_back(const JsonValue &value);
    void clear();

    std::string dump(int indent = 0) const;
};

struct JsonDocument {
    JsonValue root;

    static JsonDocument object();
    static JsonDocument parseContent(const std::string &content, bool &ok);
    static JsonDocument parseFile(const std::string &path, bool &ok, bool logWarning = true);

    bool is_null() const;
    bool empty() const;
    void clear();

    bool contains(const std::string &key) const;
    JsonValue &operator[](const std::string &key);
    const JsonValue &operator[](const std::string &key) const;

    bool valueBool(const std::string &key, bool defaultValue) const;
    std::string valueString(const std::string &key, const std::string &defaultValue) const;

    std::string dump(int indent = 0) const;
};

JsonValue fromSimdjsonElement(simdjson::dom::element elem);

#pragma once

#include <optional>
#include <simdjson.h>
#include <string>
#include <string_view>
#include <vector>

namespace JsonDom {

inline bool hasKey(simdjson::dom::element elem, std::string_view key) {
    if (!elem.is_object()) return false;
    simdjson::dom::element child;
    return !elem[key].get(child);
}

inline bool isEmptyObject(simdjson::dom::element elem) {
    simdjson::dom::object obj;
    if (elem.get_object().get(obj)) return true;
    return obj.size() == 0;
}

inline bool isEmptyArray(simdjson::dom::element elem) {
    simdjson::dom::array arr;
    if (elem.get_array().get(arr)) return true;
    return arr.size() == 0;
}

inline size_t arraySize(simdjson::dom::element elem) {
    simdjson::dom::array arr;
    if (elem.get_array().get(arr)) return 0;
    return arr.size();
}

inline simdjson::dom::element arrayAt(simdjson::dom::element elem, size_t index) {
    simdjson::dom::array arr;
    if (elem.get_array().get(arr) || index >= arr.size()) return simdjson::dom::element();
    return arr.at(index);
}

inline std::optional<std::string> getStringValue(simdjson::dom::element elem) {
    if (elem.is_string()) {
        std::string_view sv;
        if (elem.get_string().get(sv)) return std::nullopt;
        return std::string(sv);
    }
    if (elem.is_number()) {
        double value = 0;
        if (elem.get_double().get(value)) return std::nullopt;
        return std::to_string(value);
    }
    if (elem.is_bool()) {
        bool value = false;
        if (elem.get_bool().get(value)) return std::nullopt;
        return value ? "true" : "false";
    }
    return std::nullopt;
}

inline std::optional<double> getNumberValue(simdjson::dom::element elem) {
    if (!elem.is_number()) return std::nullopt;
    double value = 0;
    if (elem.get_double().get(value)) return std::nullopt;
    return value;
}

inline int getIntValue(simdjson::dom::element elem, int defaultValue = 0) {
    if (auto value = getNumberValue(elem)) return static_cast<int>(*value);
    return defaultValue;
}

inline std::optional<std::string> getString(simdjson::dom::element elem, std::string_view key) {
    simdjson::dom::element child;
    if (elem[key].get(child)) return std::nullopt;
    return getStringValue(child);
}

inline std::optional<double> getDouble(simdjson::dom::element elem, std::string_view key) {
    simdjson::dom::element child;
    if (elem[key].get(child)) return std::nullopt;
    double value = 0;
    if (child.get_double().get(value)) return std::nullopt;
    return value;
}

inline std::optional<bool> getBool(simdjson::dom::element elem, std::string_view key) {
    simdjson::dom::element child;
    if (elem[key].get(child)) return std::nullopt;
    bool value = false;
    if (child.get_bool().get(value)) return std::nullopt;
    return value;
}

inline std::optional<int64_t> getInt64(simdjson::dom::element elem, std::string_view key) {
    simdjson::dom::element child;
    if (elem[key].get(child)) return std::nullopt;
    int64_t value = 0;
    if (child.get_int64().get(value)) return std::nullopt;
    return value;
}

inline int getIntOr(simdjson::dom::element elem, std::string_view key, int defaultValue) {
    if (auto value = getInt64(elem, key)) return static_cast<int>(*value);
    if (auto value = getDouble(elem, key)) return static_cast<int>(*value);
    return defaultValue;
}

inline double getDoubleOr(simdjson::dom::element elem, std::string_view key, double defaultValue) {
    if (auto value = getDouble(elem, key)) return *value;
    return defaultValue;
}

inline bool getBoolOr(simdjson::dom::element elem, std::string_view key, bool defaultValue) {
    if (auto value = getBool(elem, key)) return *value;
    return defaultValue;
}

inline std::string toJsonString(simdjson::dom::element elem) {
    return std::string(simdjson::minify(elem));
}

inline bool parseNested(simdjson::dom::parser &parser, std::string_view jsonStr, simdjson::dom::element &out) {
    simdjson::padded_string padded(jsonStr);
    return !parser.parse(padded).get(out);
}

inline std::vector<std::string> parseStringArray(simdjson::dom::parser &parser, std::string_view jsonStr) {
    std::vector<std::string> values;
    simdjson::dom::element parsed;
    if (!parseNested(parser, jsonStr, parsed) || !parsed.is_array()) return values;

    simdjson::dom::array arr;
    if (parsed.get_array().get(arr)) return values;

    for (simdjson::dom::element item : arr) {
        if (auto value = getStringValue(item)) values.push_back(*value);
    }
    return values;
}

} // namespace JsonDom

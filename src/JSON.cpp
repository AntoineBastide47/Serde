//
// JsonValue.cpp
// Author: Antoine Bastide
// Date: 05.07.2025
//

#include <cmath>
#include <sstream>

#include "JSON.hpp"
#include "JsonParser.hpp"
#include "Log.hpp"

namespace Serde {
  JSON::JSON()
    : type(Null), data(null) {}

  auto JSONNull = JSON();

  JSON::JSON(const bool value)
    : type(Boolean), data(value) {}

  JSON::JSON(const std::initializer_list<JSON> &values) {
    const bool isObject = std::ranges::all_of(
      values, [](const JSON &j) {
        return j.IsArray() && j.Size() == 2 && j[0].IsString();
      }
    );

    type = isObject ? object : array;

    if (isObject) {
      auto obj = std::make_unique<JSONObject>();
      for (const auto &pair: values)
        obj->emplace_back(pair[0].GetString(), pair[1]);
      data = std::move(obj);
    } else
      data = std::make_unique<JSONArray>(values);
  }

  JSON::JSON(const JSON &other) : type(other.type) {
    switch (type) {
      case Null:    data = null; break;
      case Boolean: data = std::get<bool>(other.data); break;
      case Number:  data = std::get<double>(other.data); break;
      case String:  data = std::get<std::string>(other.data); break;
      case array:   data = std::make_unique<JSONArray>(*std::get<std::unique_ptr<JSONArray>>(other.data)); break;
      case object:  data = std::make_unique<JSONObject>(*std::get<std::unique_ptr<JSONObject>>(other.data)); break;
    }
  }

  JSON &JSON::operator=(const JSON &other) {
    if (this == &other)
      return *this;
    type = other.type;
    switch (type) {
      case Null:    data = null; break;
      case Boolean: data = std::get<bool>(other.data); break;
      case Number:  data = std::get<double>(other.data); break;
      case String:  data = std::get<std::string>(other.data); break;
      case array:   data = std::make_unique<JSONArray>(*std::get<std::unique_ptr<JSONArray>>(other.data)); break;
      case object:  data = std::make_unique<JSONObject>(*std::get<std::unique_ptr<JSONObject>>(other.data)); break;
    }
    return *this;
  }

  bool JSON::operator==(const JSON &other) const {
    if (type != other.type) return false;
    switch (type) {
      case Null:    return true;
      case Boolean: return std::get<bool>(data) == std::get<bool>(other.data);
      case Number:  return std::get<double>(data) == std::get<double>(other.data);
      case String:  return std::get<std::string>(data) == std::get<std::string>(other.data);
      case array:   return GetArray() == other.GetArray();
      case object:  return GetObject() == other.GetObject();
    }
    return false;
  }

  bool JSON::operator!=(const JSON &other) const {
    return !(*this == other);
  }

  bool JSON::operator<(const JSON &other) const {
    return type < other.type;
  }

  bool JSON::operator<=(const JSON &other) const {
    return type <= other.type;
  }

  bool JSON::operator>(const JSON &other) const {
    return type > other.type;
  }

  bool JSON::operator>=(const JSON &other) const {
    return type >= other.type;
  }

  std::ostream &operator<<(std::ostream &os, const JSON &value) {
    return os << value.Dump();
  }

  std::istream &operator>>(std::istream &is, JSON &value) {
    value = JSONParser(is).Parse();
    return is;
  }

  JSON &JSON::operator[](const size_t index) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    auto &arr = GetArray();
    if (index >= arr.size())
      arr.resize(index + 1);
    return arr[index];
  }

  const JSON &JSON::operator[](const size_t index) const {
    if (type == array) {
      if (const auto &arr = GetArray(); index < arr.size())
        return arr[index];
      Log::Error("JSON::operator[]");
      throw std::out_of_range("");
    }
    Log::Error("JSON::operator[] on a non array-type");
    throw std::logic_error("");
  }

  JSON &JSON::operator[](const std::string &key) {
    if (type != object) {
      type = object;
      data = std::make_unique<JSONObject>();
    }
    auto &obj = GetObject();
    for (auto &[k, v]: obj)
      if (k == key) return v;
    obj.emplace_back(key, JSON{});
    return obj.back().second;
  }

  const JSON &JSON::operator[](const std::string &key) const {
    if (type == object) {
      for (const auto &[k, v]: GetObject())
        if (k == key) return v;
      Log::Error("JSON::operator[] key: \"" + key + "\" not found");
      throw std::out_of_range("");
    }
    Log::Error("JSON::operator[] on a non object-type");
    throw std::logic_error("");
  }

  null_t &JSON::GetNull() {
    if (type == Null)
      return std::get<null_t>(data);
    Log::Error("JSON::GetNull called on a non-null type");
    throw std::logic_error("");
  }

  bool &JSON::GetBool() {
    if (type == Boolean)
      return std::get<bool>(data);
    Log::Error("JSON::GetBool called on a non-boolean type");
    throw std::logic_error("");
  }

  double &JSON::GetNumber() {
    if (type == Number)
      return std::get<double>(data);
    Log::Error("JSON::GetNumber called on a non-number type");
    throw std::logic_error("");
  }

  std::string &JSON::GetString() {
    if (type == String)
      return std::get<std::string>(data);
    Log::Error("JSON::GetString called on a non-string type");
    throw std::logic_error("");
  }

  JSON::JSONArray &JSON::GetArray() {
    if (type == array)
      return *std::get<std::unique_ptr<JSONArray>>(data);
    Log::Error("JSON::GetArray called on a non-array type");
    throw std::logic_error("");
  }

  JSON::JSONObject &JSON::GetObject() {
    if (type == object)
      return *std::get<std::unique_ptr<JSONObject>>(data);
    Log::Error("JSON::GetObject called on a non-object type");
    throw std::logic_error("");
  }

  const null_t &JSON::GetNull() const {
    if (type == Null)
      return std::get<null_t>(data);
    Log::Error("JSON::GetNull called on a non-null type");
    throw std::logic_error("");
  }

  const bool &JSON::GetBool() const {
    if (type == Boolean)
      return std::get<bool>(data);
    Log::Error("JSON::GetBool called on a non-boolean type");
    throw std::logic_error("");
  }

  const double &JSON::GetNumber() const {
    if (type == Number)
      return std::get<double>(data);
    Log::Error("JSON::GetNumber called on a non-number type");
    throw std::logic_error("");
  }

  const std::string &JSON::GetString() const {
    if (type == String)
      return std::get<std::string>(data);
    Log::Error("JSON::GetString called on a non-string type");
    throw std::logic_error("");
  }

  const JSON::JSONArray &JSON::GetArray() const {
    if (type == array)
      return *std::get<std::unique_ptr<JSONArray>>(data);
    Log::Error("JSON::GetArray called on a non-array type");
    throw std::logic_error("");
  }

  const JSON::JSONObject &JSON::GetObject() const {
    if (type == object)
      return *std::get<std::unique_ptr<JSONObject>>(data);
    Log::Error("JSON::GetObject called on a non-object type");
    throw std::logic_error("");
  }

  void JSON::PushBack(const JSON &value) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    GetArray().push_back(value);
  }

  void JSON::PushBack(JSON &&value) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    GetArray().push_back(std::move(value));
  }

  void JSON::PushBackAll(const std::initializer_list<JSON> &values) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    auto &arr = GetArray();
    arr.insert(arr.end(), values.begin(), values.end());
  }

  void JSON::Erase(const size_t index) {
    if (type == array) {
      auto &arr = GetArray();
      if (index >= arr.size()) {
        Log::Error("JSON::Erase");
        throw std::out_of_range("");
      }
      arr.erase(arr.begin() + static_cast<long>(index));
      return;
    }
    Log::Error("JSON::Erase on a non-array type");
    throw std::logic_error("");
  }

  void JSON::Resize(const size_t size) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    GetArray().resize(size);
  }

  void JSON::Reserve(const size_t size) {
    if (type == array)
      GetArray().reserve(size);
    else if (type == object)
      GetObject().reserve(size);
    else
      Log::Error("JSON::Reserve called on non-array and non-object type");
    throw std::logic_error("");
  }

  void JSON::ReserveAndResize(const size_t size) {
    if (type != array) {
      type = array;
      data = std::make_unique<JSONArray>();
    }
    auto &arr = GetArray();
    arr.reserve(size);
    arr.resize(size);
  }

  JSON &JSON::Front() {
    if (type == array)
      return GetArray().front();
    Log::Error("JSON::Front() called on non-array type");
    throw std::logic_error("");
  }

  const JSON &JSON::Front() const {
    if (type == array)
      return GetArray().front();
    Log::Error("JSON::Front() called on non-array type");
    throw std::logic_error("");
  }

  JSON &JSON::Back() {
    if (type == array)
      return GetArray().back();
    Log::Error("JSON::Back called on non-array type");
    throw std::logic_error("");
  }

  const JSON &JSON::Back() const {
    if (type == array)
      return GetArray().back();
    Log::Error("JSON::Back called on non-array type");
    throw std::logic_error("");
  }

  JSON &JSON::At(const size_t index) {
    if (type == array)
      return GetArray().at(index);
    Log::Error("JSON::At() on a non array-type");
    throw std::out_of_range("");
  }

  const JSON &JSON::At(const size_t index) const {
    if (type == array)
      return GetArray().at(index);
    Log::Error("JSON::At() on a non array-type");
    throw std::out_of_range("");
  }

  JSON &JSON::Value(const size_t index, JSON &defaultValue) {
    if (type == array) {
      auto &arr = GetArray();
      return arr[index] == JSON{} ? defaultValue : arr[index];
    }
    return defaultValue;
  }

  const JSON &JSON::Value(const size_t index, JSON &defaultValue) const {
    if (type == array) {
      const auto &arr = GetArray();
      return arr[index] == JSON{} ? defaultValue : arr[index];
    }
    return defaultValue;
  }

  void JSON::Insert(const size_t index, const JSON &value) {
    if (type == array) {
      auto &arr = GetArray();
      if (index >= arr.size()) {
        Log::Error("JSON::Insert()");
        throw std::out_of_range("");
      }
      arr.at(index) = value;
      return;
    }
    Log::Error("JSON::Insert() called on non-array type");
    throw std::logic_error("");
  }

  void JSON::Emplace(const std::string &key, const JSON &value) {
    if (type != object) {
      type = object;
      data = std::make_unique<JSONObject>();
    }
    auto &obj = GetObject();
    for (auto &[k, v]: obj) {
      if (k == key) {
        v = value;
        return;
      }
    }
    obj.emplace_back(key, value);
  }

  void JSON::EmplaceAll(const std::initializer_list<std::pair<std::string, JSON>> &pairs) {
    for (const auto &[k, v]: pairs)
      Emplace(k, v);
  }

  void JSON::Erase(const std::string &key) {
    if (type == object) {
      auto &obj = GetObject();
      const auto it = std::ranges::find_if(obj, [&](const auto &p) { return p.first == key; });
      if (it != obj.end())
        obj.erase(it);
    }
  }

  void JSON::EraseAll(const std::initializer_list<std::string> &keys) {
    if (type == object)
      for (const auto &key: keys)
        Erase(key);
  }

  JSON &JSON::At(const std::string &key) {
    if (type == object) {
      for (auto &[k, v]: GetObject())
        if (k == key) return v;
      Log::Error("JSON::At() key not found: \"" + key + "\"");
      throw std::out_of_range("");
    }
    Log::Error("JSON::At() on a non object-type");
    throw std::out_of_range("");
  }

  const JSON &JSON::At(const std::string &key) const {
    if (type == object) {
      for (const auto &[k, v]: GetObject())
        if (k == key) return v;
      Log::Error("JSON::At() key not found: \"" + key + "\"");
      throw std::out_of_range("");
    }
    Log::Error("JSON::At() on a non object-type");
    throw std::out_of_range("");
  }

  JSON &JSON::Value(const std::string &key, JSON &defaultValue) {
    if (type == object) {
      for (auto &[k, v]: GetObject())
        if (k == key) return v == JSON{} ? defaultValue : v;
    }
    return defaultValue;
  }

  const JSON &JSON::Value(const std::string &key, JSON &defaultValue) const {
    if (type == object) {
      for (const auto &[k, v]: GetObject())
        if (k == key) return v == JSON{} ? defaultValue : v;
    }
    return defaultValue;
  }

  bool JSON::Contains(const JSON &value) const {
    if (type == object) {
      const auto &obj = GetObject();
      return std::ranges::find_if(
               obj, [&](const auto &pair) {
                 return pair.second == value;
               }
             ) != obj.end();
    }

    if (type == array) {
      const auto &arr = GetArray();
      return std::ranges::find(arr, value) != arr.end();
    }

    Log::Error("JSON::Contains() called on non-array or non-object type");
    throw std::out_of_range("");
  }

  bool JSON::Contains(const std::string &key) const {
    if (type == object) {
      const auto &obj = GetObject();
      return std::ranges::any_of(obj, [&](const auto &p) { return p.first == key; });
    }
    Log::Error("JSON::Contains() called on non-object type");
    throw std::out_of_range("");
  }

  bool JSON::Contains(const char *key) const {
    return Contains(std::string(key));
  }

  size_t JSON::Size() const {
    if (type == array)
      return GetArray().size();
    if (type == object)
      return GetObject().size();
    Log::Error("JSON::Size() called on non-array or non-object type");
    throw std::out_of_range("");
  }

  bool JSON::Empty() const {
    if (type == array)
      return GetArray().empty();
    if (type == object)
      return GetObject().empty();
    Log::Error("JSON::Empty() called on non-array or non-object type");
    throw std::out_of_range("");
  }

  void JSON::Clear() {
    if (type == array)
      GetArray().clear();
    else if (type == object)
      GetObject().clear();
  }

  bool JSON::IsNull() const {
    return type == Null;
  }

  bool JSON::IsBool() const {
    return type == Boolean;
  }

  bool JSON::IsNumber() const {
    return type == Number;
  }

  bool JSON::IsString() const {
    return type == String;
  }

  bool JSON::IsArray() const {
    return type == array;
  }

  bool JSON::IsObject() const {
    return type == object;
  }

  std::string JSON::Dump(const bool prettyPrint, const char indentChar) const {
    return dump(prettyPrint, 0, indentChar);
  }

  JSON JSON::Array(const std::initializer_list<JSON> &values) {
    JSON json;
    json.type = array;
    json.data = std::make_unique<JSONArray>(values);
    return json;
  }

  JSON JSON::Object(const std::initializer_list<std::pair<std::string, JSON>> &values) {
    JSON json;
    json.type = object;
    json.data = std::make_unique<JSONObject>(values.begin(), values.end());
    return json;
  }

  bool JSON::isComplexType() const {
    return type == object || type == array;
  }

  std::string JSON::dump(const bool prettyPrint, const int indentSize, const char indentChar) const {
    std::ostringstream oss;
    const int newIndent = prettyPrint ? indentSize + 1 : 0;
    const int endCharIndent = indentSize > 0 ? indentSize : 0;

    switch (type) {
      case Null:
        oss.write("null", 4);
        break;
      case Boolean: {
        const auto &value = GetBool();
        oss.write(value ? "true" : "false", value ? 4 : 5);
        break;
      }
      case Number: {
        if (const auto value = GetNumber(); std::isnan(value))
          oss.write("NaN", 3);
        else if (std::isinf(value))
          oss.write("Inf", 3);
        else
          oss << value << (static_cast<int>(value) == value ? ".0" : "");
        break;
      }
      case String: {
        const auto &value = GetString();
        oss.put('"');
        oss.write(value.c_str(), static_cast<long>(value.size()));
        oss.put('"');
        break;
      }
      case array: {
        const auto &arr = GetArray();
        std::string dump;
        oss.put('[');
        for (const auto &item: arr) {
          if (prettyPrint) {
            oss.put('\n');
            oss.write(std::string(newIndent * 2, indentChar).c_str(), newIndent * 2);
          }
          dump = item.dump(prettyPrint, newIndent, indentChar);
          oss.write(dump.c_str(), static_cast<long>(dump.size()));
          oss.put(',');
        }
        if (!arr.empty()) {
          oss.seekp(-1, std::ios_base::end);
          if (prettyPrint) {
            oss.put('\n');
            oss.write(std::string(endCharIndent * 2, indentChar).c_str(), endCharIndent * 2);
          }
        }
        oss.put(']');
        break;
      }
      case object: {
        const auto &obj = GetObject();
        std::string dump;
        oss.put('{');
        for (const auto &[key, value]: obj) {
          if (prettyPrint) {
            oss.put('\n');
            oss.write(std::string(newIndent * 2, indentChar).c_str(), newIndent * 2);
          }
          oss.put('"');
          oss.write(key.c_str(), static_cast<long>(key.size()));
          oss.put('"');
          oss.put(':');
          if (prettyPrint)
            oss.put(' ');
          dump = value.dump(prettyPrint, newIndent, indentChar);
          oss.write(dump.c_str(), static_cast<long>(dump.size()));
          oss.put(',');
        }
        if (!obj.empty()) {
          oss.seekp(-1, std::ios_base::end);
          if (prettyPrint) {
            oss.put('\n');
            oss.write(std::string(endCharIndent * 2, indentChar).c_str(), endCharIndent * 2);
          }
        }
        oss.put('}');
        break;
      }
    }

    return oss.str();
  }
}

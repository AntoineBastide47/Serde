// Tests for Serde::JSON, Serializer, and Deserializer.
// No LLVM/Clang dependency required.

#include <array>
#include <deque>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../include/JsonParser.hpp"
#include "../include/Load.hpp"
#include "../include/Reflectable.hpp"
#include "../include/Serializer.hpp"

// ---- minimal Reflectable test type ---------------------------------

struct Vec2 : Serde::Reflectable {
  int x{}, y{};

  Vec2() = default;
  Vec2(int x, int y) : x(x), y(y) {}

  [[nodiscard]] std::string_view ClassNameQualified() const override { return "Vec2"; }
  [[nodiscard]] std::string_view ClassName() const override { return "Vec2"; }

  void _e_save(Serde::Format fmt, Serde::JSON &json) const override {
    json = Serde::JSON::Object();
    Serde::_e_saveImpl(x, fmt, json["x"]);
    Serde::_e_saveImpl(y, fmt, json["y"]);
  }

  void _e_load(Serde::Format fmt, const Serde::JSON &json) override {
    Serde::_e_loadImpl(x, fmt, json.At("x"));
    Serde::_e_loadImpl(y, fmt, json.At("y"));
  }

  bool operator==(const Vec2 &o) const { return x == o.x && y == o.y; }
};

enum class Color : int { Red = 0, Green = 1, Blue = 2 };

// ---- test framework ------------------------------------------------

static int passed = 0, failed = 0;
static std::vector<std::string> failures;

static void check(const bool condition, const std::string &name) {
  if (condition) {
    std::cout << "  " << name << " ... ok\n" << std::flush;
    ++passed;
  } else {
    std::cout << "  " << name << " ... FAIL\n" << std::flush;
    failures.push_back(name);
    ++failed;
  }
}

template<typename E = std::exception>
static bool throws_as(const std::function<void()> &fn) {
  try { fn(); return false; }
  catch (const E &) { return true; }
  catch (...) { return false; }
}

// ---- 1. Constructors -----------------------------------------------

static void testConstructors() {
  std::cout << "--- Constructors ---\n";
  using namespace Serde;

  {
    JSON j;
    check(j.IsNull() && !j.IsBool() && !j.IsNumber() && !j.IsString() && !j.IsArray() && !j.IsObject(),
          "default ctor → null");
  }
  {
    JSON t(true), f(false);
    check(t.IsBool() && t.GetBool() == true,  "bool true ctor");
    check(f.IsBool() && f.GetBool() == false, "bool false ctor");
  }
  {
    JSON j("hello");
    check(j.IsString() && j.GetString() == "hello", "const char* ctor");
  }
  {
    check(JSON(42).IsNumber() && JSON(42).GetNumber() == 42.0,     "int ctor");
    check(JSON(3.14).IsNumber() && JSON(3.14).GetNumber() == 3.14, "double ctor");
    check(JSON(1.5f).IsNumber(),                                   "float ctor");
    check(JSON(100LL).GetNumber() == 100.0,                        "long long ctor");
  }
  {
    std::string s = "world";
    check(JSON(s).IsString() && JSON(s).GetString() == "world", "std::string ctor");
  }
  {
    std::string_view sv = "view";
    check(JSON(sv).IsString() && JSON(sv).GetString() == "view", "string_view ctor");
  }
  {
    check(JSON('A').IsString() && JSON('A').GetString() == "A", "char ctor");
  }
  {
    JSON j({JSON(1), JSON(2), JSON(3)});
    check(j.IsArray() && j.Size() == 3 && j[0].GetNumber() == 1.0, "init-list → array");
  }
  {
    JSON j({JSON({"k1", JSON(1)}), JSON({"k2", JSON(2)})});
    check(j.IsObject() && j.Size() == 2, "init-list → object");
  }
  {
    std::vector<JSON> vec = {JSON(1), JSON(2)};
    JSON j(vec);
    check(j.IsArray() && j.Size() == 2, "vector<JSON> ctor");
  }
  {
    std::map<std::string, JSON> m = {{"a", JSON(1)}, {"b", JSON(2)}};
    JSON j(m);
    check(j.IsObject() && j.Size() == 2, "map<string,JSON> ctor");
  }
  {
    JSON j(std::make_tuple(1, true, std::string("x")));
    check(j.IsArray() && j.Size() == 3, "tuple ctor size");
    check(j[0].GetNumber() == 1.0 && j[1].GetBool() && j[2].GetString() == "x", "tuple ctor values");
  }
  {
    JSON arr = JSON::Array({JSON(10), JSON(20)});
    check(arr.IsArray() && arr.Size() == 2, "Array() factory");

    JSON obj = JSON::Object({{"x", JSON(1)}, {"y", JSON(2)}});
    check(obj.IsObject() && obj.Size() == 2, "Object() factory");
  }
  {
    JSON orig = JSON::Array({JSON(1), JSON(2)});
    JSON copy(orig);
    check(copy.IsArray() && copy.Size() == 2, "copy ctor type/size");
    copy[0] = JSON(99);
    check(orig[0].GetNumber() == 1.0, "copy ctor is deep");
  }
  {
    JSON a(42);
    JSON b("hello");
    b = a;
    check(b.IsNumber() && b.GetNumber() == 42.0, "copy assignment");
  }
}

// ---- 2. Getters and wrong-type throws ------------------------------

static void testGetters() {
  std::cout << "\n--- Getters ---\n";
  using namespace Serde;

  JSON jnull;
  JSON jbool(true);
  JSON jnum(3.14);
  JSON jstr("hi");
  JSON jarr = JSON::Array({JSON(1)});
  JSON jobj = JSON::Object({{"k", JSON(1)}});

  check(!throws_as<std::logic_error>([&]{ jnull.GetNull(); }),  "GetNull on null ok");
  check(!throws_as<std::logic_error>([&]{ jbool.GetBool(); }),  "GetBool on bool ok");
  check(!throws_as<std::logic_error>([&]{ jnum.GetNumber(); }), "GetNumber on number ok");
  check(!throws_as<std::logic_error>([&]{ jstr.GetString(); }), "GetString on string ok");
  check(!throws_as<std::logic_error>([&]{ jarr.GetArray(); }),  "GetArray on array ok");
  check(!throws_as<std::logic_error>([&]{ jobj.GetObject(); }), "GetObject on object ok");

  check(throws_as<std::logic_error>([&]{ jbool.GetNull(); }),   "GetNull on bool throws");
  check(throws_as<std::logic_error>([&]{ jnull.GetBool(); }),   "GetBool on null throws");
  check(throws_as<std::logic_error>([&]{ jstr.GetNumber(); }),  "GetNumber on string throws");
  check(throws_as<std::logic_error>([&]{ jnum.GetString(); }),  "GetString on number throws");
  check(throws_as<std::logic_error>([&]{ jnull.GetArray(); }),  "GetArray on null throws");
  check(throws_as<std::logic_error>([&]{ jarr.GetObject(); }),  "GetObject on array throws");

  const JSON cn(99);
  const JSON cs("test");
  check(cn.GetNumber() == 99.0, "const GetNumber");
  check(cs.GetString() == "test", "const GetString");
  check(throws_as<std::logic_error>([&]{ cn.GetString(); }), "const wrong-type throws");
}

// ---- 3. Comparison -------------------------------------------------

static void testComparison() {
  std::cout << "\n--- Comparison ---\n";
  using namespace Serde;

  check(JSON() == JSON(),             "null == null");
  check(JSON(true) == JSON(true),     "true == true");
  check(JSON(true) != JSON(false),    "true != false");
  check(JSON(42.0) == JSON(42.0),     "number ==");
  check(JSON(42.0) != JSON(43.0),     "number !=");
  check(JSON("abc") == JSON("abc"),   "string ==");
  check(JSON("abc") != JSON("xyz"),   "string !=");
  check(JSON() != JSON(true),         "null != bool");
  check(JSON() != JSON(0),            "null != number");

  JSON a = JSON::Array({JSON(1), JSON(2)});
  JSON b = JSON::Array({JSON(1), JSON(2)});
  JSON c = JSON::Array({JSON(1), JSON(3)});
  check(a == b, "equal arrays ==");
  check(a != c, "different arrays !=");

  check(JSON::Object({{"k", JSON(1)}}) == JSON::Object({{"k", JSON(1)}}), "equal objects ==");

  check(JSON() < JSON(true),        "null < bool");
  check(JSON(true) <= JSON(42.0),   "bool <= number");
  check(JSON("x") > JSON(42.0),    "string > number");
  check(JSON(42.0) >= JSON(true),   "number >= bool");
}

// ---- 4. Array API --------------------------------------------------

static void testArrayAPI() {
  std::cout << "\n--- Array API ---\n";
  using namespace Serde;

  {
    JSON j;
    j[0] = JSON(10);
    j[2] = JSON(30);
    check(j.IsArray() && j.Size() == 3, "operator[] grows array");
    check(j[0].GetNumber() == 10.0 && j[2].GetNumber() == 30.0, "operator[] values");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2)});
    check(j.At(0).GetNumber() == 1.0, "At(0)");
    check(j.At(1).GetNumber() == 2.0, "At(1)");
    check(throws_as<std::exception>([&]{ j.At(99); }),     "At OOB throws");
    check(throws_as<std::exception>([&]{ JSON().At(0); }), "At non-array throws");
  }
  {
    const JSON j = JSON::Array({JSON(5), JSON(6)});
    check(j[0].GetNumber() == 5.0, "const operator[](0)");
    check(throws_as<std::exception>([&]{ (void)j[99]; }),        "const operator[] OOB throws");
    { const JSON cn42(42); check(throws_as<std::exception>([&]{ (void)cn42[0]; }), "const operator[] non-array throws"); }
    check(j.At(1).GetNumber() == 6.0, "const At(1)");
  }
  {
    JSON j = JSON::Array({JSON(10), JSON(20), JSON(30)});
    check(j.Front().GetNumber() == 10.0, "Front");
    check(j.Back().GetNumber() == 30.0,  "Back");
    check(throws_as<std::logic_error>([&]{ JSON().Front(); }), "Front non-array throws");
    check(throws_as<std::logic_error>([&]{ JSON().Back(); }),  "Back non-array throws");
  }
  {
    JSON j = JSON::Array();
    j.PushBack(JSON(1));
    j.PushBack(JSON(2));
    check(j.Size() == 2 && j.Back().GetNumber() == 2.0, "PushBack");
    JSON tmp(42);
    j.PushBack(std::move(tmp));
    check(j.Size() == 3, "PushBack(move)");
  }
  {
    JSON j = JSON::Array();
    j.PushBackAll({JSON(1), JSON(2), JSON(3)});
    check(j.Size() == 3, "PushBackAll");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2), JSON(3)});
    j.Erase(1);
    check(j.Size() == 2 && j[0].GetNumber() == 1.0 && j[1].GetNumber() == 3.0, "Erase(index)");
    check(throws_as<std::out_of_range>([&]{ j.Erase(99); }),          "Erase OOB throws");
    check(throws_as<std::logic_error>([&]{ JSON("x").Erase(0); }),    "Erase non-array throws");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2)});
    j.Resize(4);
    check(j.Size() == 4, "Resize grow");
    j.Resize(1);
    check(j.Size() == 1, "Resize shrink");
  }
  {
    JSON jarr = JSON::Array();
    check(!throws_as<std::exception>([&]{ jarr.Reserve(10); }),      "Reserve on array ok");
    check(throws_as<std::logic_error>([&]{ JSON(42).Reserve(10); }), "Reserve on number throws");
  }
  {
    JSON j;
    j.ReserveAndResize(5);
    check(j.IsArray() && j.Size() == 5, "ReserveAndResize");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2)});
    j.Insert(0, JSON(99));
    check(j.At(0).GetNumber() == 99.0, "Insert replaces at index");
    check(throws_as<std::out_of_range>([&]{ j.Insert(99, JSON(0)); }),       "Insert OOB throws");
    check(throws_as<std::logic_error>([&]{ JSON("x").Insert(0, JSON(0)); }), "Insert non-array throws");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON()});
    JSON def(999);
    check(j.Value(0, def).GetNumber() == 1.0,   "Value(idx) non-null");
    check(j.Value(1, def).GetNumber() == 999.0, "Value(idx) null → default");
    check(JSON().Value(0, def).GetNumber() == 999.0, "Value(idx) non-array → default");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2)});
    check(j.Size() == 2, "array Size");
    check(!j.Empty(), "array not Empty");
    j.Clear();
    check(j.Size() == 0 && j.Empty(), "Clear empties array");
    check(throws_as<std::exception>([&]{ JSON(42).Size(); }),  "Size non-array throws");
    check(throws_as<std::exception>([&]{ JSON(42).Empty(); }), "Empty non-array throws");
  }
  {
    JSON j = JSON::Array({JSON(1), JSON(2), JSON("x")});
    check(j.Contains(JSON(1)),   "Contains(JSON) found");
    check(!j.Contains(JSON(99)), "Contains(JSON) not found");
    check(throws_as<std::exception>([&]{ JSON(42).Contains(JSON(1)); }), "Contains(JSON) non-array/obj throws");
  }
}

// ---- 5. Object API -------------------------------------------------

static void testObjectAPI() {
  std::cout << "\n--- Object API ---\n";
  using namespace Serde;

  {
    JSON j;
    j["x"] = JSON(10);
    j["y"] = JSON(20);
    check(j.IsObject() && j.Size() == 2, "operator[](key) creates object");
    check(j["x"].GetNumber() == 10.0,    "operator[](key) value");
  }
  {
    const JSON j = JSON::Object({{"a", JSON(1)}, {"b", JSON(2)}});
    check(j["a"].GetNumber() == 1.0, "const operator[](key)");
    check(throws_as<std::out_of_range>([&]{ (void)j["missing"]; }), "const operator[] missing throws");
    { const JSON cn42(42); check(throws_as<std::logic_error>([&]{ (void)cn42["k"]; }), "const operator[] non-object throws"); }
    check(j.At("b").GetNumber() == 2.0, "const At(key)");
    check(throws_as<std::out_of_range>([&]{ (void)j.At("missing"); }), "const At missing throws");
  }
  {
    JSON j = JSON::Object({{"k", JSON(5)}});
    j.At("k") = JSON(50);
    check(j.At("k").GetNumber() == 50.0, "mutable At update");
    check(throws_as<std::out_of_range>([&]{ j.At("nope"); }), "mutable At missing throws");
  }
  {
    JSON j;
    j.Emplace("x", JSON(1));
    j.Emplace("y", JSON(2));
    check(j.IsObject() && j.Size() == 2, "Emplace creates entries");
    j.Emplace("x", JSON(99));
    check(j.Size() == 2 && j.At("x").GetNumber() == 99.0, "Emplace updates, no duplicate");
    j.EmplaceAll({{"a", JSON(10)}, {"b", JSON(20)}});
    check(j.Size() == 4, "EmplaceAll");
  }
  {
    JSON j = JSON::Object({{"a", JSON(1)}, {"b", JSON(2)}, {"c", JSON(3)}});
    j.Erase("b");
    check(j.Size() == 2 && !j.Contains("b"), "Erase(key)");
    j.EraseAll({"a", "c"});
    check(j.Size() == 0, "EraseAll");
  }
  {
    JSON j = JSON::Object({{"x", JSON(1)}});
    check(j.Contains("x"),  "Contains(key) found");
    check(!j.Contains("z"), "Contains(key) not found");
    check(j.Contains("x"),  "Contains(const char*) found");
    check(throws_as<std::exception>([&]{ JSON::Array().Contains("x"); }), "Contains(key) on array throws");
  }
  {
    JSON j = JSON::Object({{"a", JSON(5)}, {"b", JSON()}});
    JSON def(999);
    check(j.Value("a", def).GetNumber() == 5.0,   "Value(key) non-null");
    check(j.Value("b", def).GetNumber() == 999.0, "Value(key) null → default");
    check(j.Value("missing", def).GetNumber() == 999.0, "Value(key) missing → default");
    check(JSON().Value("k", def).GetNumber() == 999.0,  "Value(key) non-object → default");
  }
  {
    JSON j = JSON::Object();
    check(!throws_as<std::exception>([&]{ j.Reserve(10); }), "Reserve on object ok");
  }
  {
    JSON j = JSON::Object({{"a", JSON(1)}});
    check(j.Size() == 1 && !j.Empty(), "object Size/Empty");
    j.Clear();
    check(j.Empty(), "Clear empties object");
  }
  {
    JSON j = JSON::Object({{"k", JSON(42)}});
    check(j.Contains(JSON(42)),  "Contains(JSON) in object values found");
    check(!j.Contains(JSON(99)), "Contains(JSON) in object values not found");
  }
}

// ---- 6. Dump / IO --------------------------------------------------

static void testDump() {
  std::cout << "\n--- Dump / IO ---\n";
  using namespace Serde;

  check(JSON().Dump()        == "null",    "dump null");
  check(JSON(true).Dump()    == "true",    "dump true");
  check(JSON(false).Dump()   == "false",   "dump false");
  check(JSON("hello").Dump() == "\"hello\"", "dump string");
  check(JSON(42).Dump()      == "42.0",    "dump integer");
  check(JSON(3.5).Dump()     == "3.5",     "dump non-integer");

  check(JSON::Array({JSON(1), JSON(2)}).Dump() == "[1.0,2.0]", "dump array");
  check(JSON::Object({{"x", JSON(1)}}).Dump()  == "{\"x\":1.0}", "dump object");

  {
    const std::string pretty = JSON::Array({JSON(1), JSON(2)}).Dump(true);
    check(pretty.find('\n') != std::string::npos, "pretty dump has newlines");
  }
  {
    std::ostringstream oss;
    oss << JSON(42);
    check(oss.str() == "42.0", "operator<< number");
  }
  {
    std::istringstream iss("{\"a\":1}");
    JSON j;
    iss >> j;
    check(j.IsObject() && j.Contains("a") && j["a"].GetNumber() == 1.0, "operator>> parses object");
  }
}

// ---- 7. TypeToString -----------------------------------------------

static void testTypeToString() {
  std::cout << "\n--- TypeToString ---\n";
  using namespace Serde;

  check(JSON::TypeToString(JSON())          == "null",    "TypeToString null");
  check(JSON::TypeToString(JSON(true))      == "boolean", "TypeToString boolean");
  check(JSON::TypeToString(JSON(42))        == "number",  "TypeToString number");
  check(JSON::TypeToString(JSON("x"))       == "string",  "TypeToString string");
  check(JSON::TypeToString(JSON::Array())   == "array",   "TypeToString array");
  check(JSON::TypeToString(JSON::Object())  == "object",  "TypeToString object");
}

// ---- 8. Save -------------------------------------------------------

static void testSave() {
  std::cout << "\n--- Save ---\n";
  using namespace Serde;

  // primitives
  check(Serializer::ToJson(42).IsNumber() && Serializer::ToJson(42).GetNumber() == 42.0,    "save int");
  check(Serializer::ToJson(3.14).GetNumber() == 3.14,                                        "save double");
  check(Serializer::ToJson(true).IsBool() && Serializer::ToJson(true).GetBool(),             "save true");
  check(!Serializer::ToJson(false).GetBool(),                                                 "save false");
  check(Serializer::ToJson(std::string("hi")).GetString() == "hi",                           "save string");
  check(Serializer::ToJson("cstr").GetString() == "cstr",                                    "save const char*");

  // containers
  {
    std::vector<int> v = {1, 2, 3};
    JSON j = Serializer::ToJson(v);
    check(j.IsArray() && j.Size() == 3 && j[0].GetNumber() == 1.0, "save vector");
  }
  {
    std::list<int> l = {4, 5};
    check(Serializer::ToJson(l).IsArray() && Serializer::ToJson(l).Size() == 2, "save list");
  }
  {
    std::deque<int> d = {7, 8};
    check(Serializer::ToJson(d).IsArray(), "save deque");
  }
  {
    std::set<int> s = {10, 20};
    check(Serializer::ToJson(s).IsArray() && Serializer::ToJson(s).Size() == 2, "save set");
  }
  {
    std::forward_list<int> fl = {1, 2, 3};
    JSON j = Serializer::ToJson(fl);
    check(j.IsArray() && j.Size() == 3, "save forward_list");
  }

  // maps
  {
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
    JSON j = Serializer::ToJson(m);
    check(j.IsObject() && j.Size() == 2, "save map");
  }
  {
    std::unordered_map<std::string, int> um = {{"x", 10}};
    check(Serializer::ToJson(um).IsObject(), "save unordered_map");
  }

  // pair
  {
    auto p = std::make_pair(1, std::string("one"));
    JSON j = Serializer::ToJson(p);
    check(j.IsArray() && j.Size() == 2, "save pair size");
    check(j[0].GetNumber() == 1.0 && j[1].GetString() == "one", "save pair values");
  }

  // tuple
  {
    auto t = std::make_tuple(1, true, std::string("z"));
    JSON j = Serializer::ToJson(t);
    check(j.IsArray() && j.Size() == 3, "save tuple size");
    check(j[0].GetNumber() == 1.0 && j[1].GetBool() && j[2].GetString() == "z", "save tuple values");
  }

  // fixed C array
  {
    int arr[3] = {10, 20, 30};
    JSON j = Serializer::ToJson(arr);
    check(j.IsArray() && j.Size() == 3 && j[2].GetNumber() == 30.0, "save fixed array");
  }

  // variant
  {
    std::variant<int, std::string> vi = 42;
    JSON j = Serializer::ToJson(vi);
    check(j.IsObject() && j.Contains("index") && j.Contains("value"), "save variant structure");
    check(j["index"].GetNumber() == 0.0 && j["value"].GetNumber() == 42.0, "save variant<int>");

    std::variant<int, std::string> vs = std::string("hello");
    JSON js = Serializer::ToJson(vs);
    check(js["index"].GetNumber() == 1.0 && js["value"].GetString() == "hello", "save variant<string>");
  }

  // smart pointers (non-Reflectable)
  {
    auto up = std::make_unique<int>(7);
    JSON j = Serializer::ToJson(up);
    check(j.IsNumber() && j.GetNumber() == 7.0, "save unique_ptr<int>");

    std::unique_ptr<int> nullUp;
    check(Serializer::ToJson(nullUp).IsNull(), "save null unique_ptr");

    auto sp = std::make_shared<int>(8);
    check(Serializer::ToJson(sp).GetNumber() == 8.0, "save shared_ptr<int>");

    std::shared_ptr<int> nullSp;
    check(Serializer::ToJson(nullSp).IsNull(), "save null shared_ptr");
  }

  // enum class
  {
    JSON j = Serializer::ToJson(Color::Green);
    check(j.IsNumber() && j.GetNumber() == 1.0, "save enum → Number");
  }

  // Reflectable
  {
    Vec2 v(3, 4);
    JSON j = Serializer::ToJson(v);
    check(j.IsObject(), "save Reflectable → Object");
    check(j.Contains("x") && j["x"].GetNumber() == 3.0, "save Reflectable x");
    check(j.Contains("y") && j["y"].GetNumber() == 4.0, "save Reflectable y");
  }

  // helpers
  check(Serializer::ToJsonString(42)                == "42.0",   "ToJsonString int");
  check(Serializer::ToJsonString(true)              == "true",   "ToJsonString bool");
  check(Serializer::ToJsonString(std::string("hi")) == "\"hi\"", "ToJsonString string");
}

// ---- 9. Load -------------------------------------------------------

static void testLoad() {
  std::cout << "\n--- Load ---\n";
  using namespace Serde;

  // primitives
  check(Deserializer::FromJson<int>(JSON(42))          == 42,   "load int");
  check(Deserializer::FromJson<double>(JSON(3.14))     == 3.14, "load double");
  check(Deserializer::FromJson<bool>(JSON(true))       == true, "load bool true");
  check(Deserializer::FromJson<bool>(JSON(false))      == false,"load bool false");
  check(Deserializer::FromJson<std::string>(JSON("hi"))== "hi", "load string");

  // vector round-trip
  {
    std::vector<int> v = {1, 2, 3};
    check(Deserializer::FromJson<std::vector<int>>(Serializer::ToJson(v)) == v, "vector round-trip");
  }
  // list round-trip
  {
    std::list<int> l = {4, 5, 6};
    check(Deserializer::FromJson<std::list<int>>(Serializer::ToJson(l)) == l, "list round-trip");
  }
  // deque round-trip
  {
    std::deque<int> d = {7, 8};
    check(Deserializer::FromJson<std::deque<int>>(Serializer::ToJson(d)) == d, "deque round-trip");
  }
  // set round-trip
  {
    std::set<int> s = {1, 2, 3};
    check(Deserializer::FromJson<std::set<int>>(Serializer::ToJson(s)) == s, "set round-trip");
  }
  // forward_list round-trip
  {
    std::forward_list<int> fl = {1, 2, 3};
    check(Deserializer::FromJson<std::forward_list<int>>(Serializer::ToJson(fl)) == fl,
          "forward_list round-trip");
  }
  // map round-trip
  {
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
    check(Deserializer::FromJson<std::map<std::string, int>>(Serializer::ToJson(m)) == m, "map round-trip");
  }
  // pair round-trip
  {
    auto p = std::make_pair(42, std::string("hello"));
    check(Deserializer::FromJson<std::pair<int, std::string>>(Serializer::ToJson(p)) == p, "pair round-trip");
  }
  // tuple round-trip
  {
    auto t = std::make_tuple(1, true, std::string("z"));
    check(Deserializer::FromJson<std::tuple<int, bool, std::string>>(Serializer::ToJson(t)) == t,
          "tuple round-trip");
  }
  // fixed C array
  {
    int arr[3] = {10, 20, 30};
    auto j = Serializer::ToJson(arr);
    int arr2[3] = {};
    _e_loadImpl(arr2, Format::JSON, j);
    check(arr2[0] == 10 && arr2[1] == 20 && arr2[2] == 30, "fixed array round-trip");

    JSON bad = JSON::Array({JSON(1), JSON(2)});
    check(throws_as<std::runtime_error>([&]{ _e_loadImpl(arr2, Format::JSON, bad); }),
          "fixed array size mismatch throws");
  }
  // variant round-trips
  {
    std::variant<int, std::string> vi = 99;
    auto v2 = Deserializer::FromJson<std::variant<int, std::string>>(Serializer::ToJson(vi));
    check(std::holds_alternative<int>(v2) && std::get<int>(v2) == 99, "variant<int> round-trip");
  }
  {
    std::variant<int, std::string> vs = std::string("world");
    auto v2 = Deserializer::FromJson<std::variant<int, std::string>>(Serializer::ToJson(vs));
    check(std::holds_alternative<std::string>(v2) && std::get<std::string>(v2) == "world",
          "variant<string> round-trip");
  }
  {
    JSON bad = JSON::Array({JSON(1)});
    check(throws_as<std::runtime_error>([&]{
      (void)Deserializer::FromJson<std::variant<int, std::string>>(bad);
    }), "variant from non-object throws");
  }
  // smart pointers round-trips
  {
    auto j = Serializer::ToJson(std::make_unique<int>(55));
    auto up = Deserializer::FromJson<std::unique_ptr<int>>(j);
    check(up != nullptr && *up == 55, "unique_ptr<int> round-trip");

    auto jNull = Serializer::ToJson(std::unique_ptr<int>{});
    check(Deserializer::FromJson<std::unique_ptr<int>>(jNull) == nullptr, "null unique_ptr round-trip");

    auto sp = Deserializer::FromJson<std::shared_ptr<int>>(j);
    check(sp != nullptr && *sp == 55, "shared_ptr<int> round-trip");

    check(Deserializer::FromJson<std::shared_ptr<int>>(jNull) == nullptr, "null shared_ptr round-trip");
  }
  // enum round-trip
  {
    auto j = Serializer::ToJson(Color::Blue);
    check(Deserializer::FromJson<Color>(j) == Color::Blue, "enum round-trip");

    JSON bad(3.5);
    check(throws_as<std::runtime_error>([&]{ (void)Deserializer::FromJson<Color>(bad); }),
          "enum non-integral throws");
  }
  // Reflectable round-trip
  {
    Vec2 original(7, 13);
    auto j = Serializer::ToJson(original);
    Vec2 loaded;
    _e_loadImpl(loaded, Format::JSON, j);
    check(loaded == original, "Reflectable round-trip");
  }
  // Deserializer helpers
  {
    check(Deserializer::FromJsonString<int>("42")            == 42,    "FromJsonString int");
    check(Deserializer::FromJsonString<bool>("true")         == true,  "FromJsonString bool");
    check(Deserializer::FromJsonString<std::string>("\"hi\"")== "hi",  "FromJsonString string");
    auto v = Deserializer::FromJsonString<std::vector<int>>("[1,2,3]");
    check(v.size() == 3 && v[0] == 1 && v[2] == 3, "FromJsonString vector");
  }
}

// ---- main ----------------------------------------------------------

int main() {
  testConstructors();
  testGetters();
  testComparison();
  testArrayAPI();
  testObjectAPI();
  testDump();
  testTypeToString();
  testSave();
  testLoad();

  std::cout << "\n";
  for (const auto &f : failures)
    std::cout << "FAIL: " << f << "\n";
  std::cout << "\n" << passed << " passed, " << failed << " failed\n";
  return failed > 0 ? 1 : 0;
}

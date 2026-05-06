// Tests for HeaderForge Generator — no Clang dependency required.
// Parser (AST traversal) is exercised end-to-end by main.cpp / CI; Generator
// is pure string manipulation and can be unit-tested directly.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Generator.hpp"
#include "Parser.hpp"

using namespace HeaderForge;
namespace fs = std::filesystem;

// ---- minimal test framework ------------------------------------------------

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

static bool has(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

static std::string readFile(const std::string &path) {
  std::ifstream f(path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static int tmpCounter = 0;

static std::string writeTmp(const std::string &content) {
  const std::string path =
      (fs::temp_directory_path() / ("hf_test_" + std::to_string(++tmpCounter) + ".hpp")).string();
  std::ofstream f(path);
  f << content;
  return path;
}

// ---- GenerateReflectionFactoryCode -----------------------------------------

static void testGenerateReflectionFactoryCode() {
  std::cout << "--- GenerateReflectionFactoryCode ---\n";

  std::ostringstream out;
  Generator::GenerateReflectionFactoryCode(Record{false, "MyStruct", "f.hpp", {}, {}}, out);
  const std::string s = out.str();

  check(has(s, "RegisterType<MyStruct>"), "registers correct type");
  check(has(s, "\"MyStruct\""), "uses correct type name string");
  check(has(s, "return true;"), "lambda returns true");
}

// ---- GenerateSaveFunction --------------------------------------------------

static void testGenerateSaveFunction() {
  std::cout << "\n--- GenerateSaveFunction ---\n";

  // with fields
  {
    std::ostringstream out;
    Generator::GenerateSaveFunction(Record{false, "S", "f.hpp", {"x", "y"}, {}}, out);
    const std::string s = out.str();
    check(has(s, "_e_save"), "save signature present");
    check(has(s, "json = Serde::JSON::Object()"), "initialises json object");
    check(has(s, "_e_saveImpl(x, format, json[\"x\"])"), "saves field x");
    check(has(s, "_e_saveImpl(y, format, json[\"y\"])"), "saves field y");
  }

  // no fields
  {
    std::ostringstream out;
    Generator::GenerateSaveFunction(Record{false, "Empty", "f.hpp", {}, {}}, out);
    const std::string s = out.str();
    check(has(s, "json = Serde::JSON::Object()"), "empty struct still initialises object");
    check(!has(s, "_e_saveImpl"), "empty struct has no save calls");
  }
}

// ---- GenerateLoadFunction --------------------------------------------------

static void testGenerateLoadFunction() {
  std::cout << "\n--- GenerateLoadFunction ---\n";

  // with fields
  {
    std::ostringstream out;
    Generator::GenerateLoadFunction(Record{false, "S", "f.hpp", {"a", "b"}, {}}, out);
    const std::string s = out.str();
    check(has(s, "_e_load"), "load signature present");
    check(has(s, "_e_loadImpl(a, format, json.At(\"a\"))"), "loads field a");
    check(has(s, "_e_loadImpl(b, format, json.At(\"b\"))"), "loads field b");
  }

  // no fields — body closes without any loadImpl calls
  {
    std::ostringstream out;
    Generator::GenerateLoadFunction(Record{false, "Empty", "f.hpp", {}, {}}, out);
    const std::string s = out.str();
    check(!has(s, "_e_loadImpl"), "empty struct has no load calls");
  }
}

// ---- GenerateRecordMacro ---------------------------------------------------

static void testGenerateRecordMacro() {
  std::cout << "\n--- GenerateRecordMacro ---\n";

  // struct — no private section
  {
    std::ostringstream out;
    Generator::GenerateRecordMacro(Record{false, "MyStruct", "f.hpp", {"v"}, {}}, out);
    const std::string s = out.str();
    check(s.rfind("#define SERIALIZE_MYSTRUCT", 0) == 0, "struct macro name correct");
    check(!has(s, "private:"), "struct has no private section");
  }

  // class — ends with private:
  {
    std::ostringstream out;
    Generator::GenerateRecordMacro(Record{true, "MyClass", "f.hpp", {"v"}, {}}, out);
    const std::string s = out.str();
    check(s.rfind("#define SERIALIZE_MYCLASS", 0) == 0, "class macro name correct");
    check(has(s, "private:"), "class ends with private section");
  }

  // qualified name — macro uses simple name, body uses fully-qualified name
  {
    std::ostringstream out;
    Generator::GenerateRecordMacro(Record{true, "NS::MyClass", "f.hpp", {"x"}, {}}, out);
    const std::string s = out.str();
    check(s.rfind("#define SERIALIZE_MYCLASS", 0) == 0, "qualified name strips namespace from macro");
    check(has(s, "RegisterType<NS::MyClass>"), "fully-qualified name used in factory registration");
  }

  // no fields — load body closes immediately
  {
    std::ostringstream out;
    Generator::GenerateRecordMacro(Record{false, "Bare", "f.hpp", {}, {}}, out);
    const std::string s = out.str();
    check(!has(s, "_e_saveImpl"), "no-field struct has no save calls");
    check(!has(s, "_e_loadImpl"), "no-field struct has no load calls");
  }
}

// ---- GenerateEnumReflectionMacro -------------------------------------------

static void testGenerateEnumReflectionMacro() {
  std::cout << "\n--- GenerateEnumReflectionMacro ---\n";

  const Enum e{"MyEnum", {"Val1", "Val2"}};

  // last enum — ends with newline, no trailing backslash
  {
    std::ostringstream out;
    Generator::GenerateEnumReflectionMacro(e, out, true);
    const std::string s = out.str();
    check(has(s, "REFLECT_MYENUM"), "macro name correct");
    check(has(s, "RegisterEnum<MyEnum>"), "registers correct enum type");
    check(has(s, "\"Val1\", MyEnum::Val1"), "includes Val1 with correct syntax");
    check(has(s, "\"Val2\", MyEnum::Val2"), "includes Val2 with correct syntax");
    check(
      !s.empty() && s.back() == '\n' &&
      s[s.size() - 2] != '\\', "last enum ends without continuation backslash"
    );
  }

  // non-last enum — ends with backslash then newline
  {
    std::ostringstream out;
    Generator::GenerateEnumReflectionMacro(e, out, false);
    const std::string s = out.str();
    const auto bs = s.rfind('\\');
    check(
      bs != std::string::npos && bs == s.size() - 2,
      "non-last enum ends with continuation backslash"
    );
  }

  // qualified enum name
  {
    std::ostringstream out;
    Generator::GenerateEnumReflectionMacro(Enum{"NS::MyEnum", {"A"}}, out, true);
    const std::string s = out.str();
    check(has(s, "REFLECT_MYENUM"), "qualified enum strips namespace from macro");
    check(has(s, "RegisterEnum<NS::MyEnum>"), "fully-qualified name used in registration");
  }
}

// ---- GenerateRecordContent -------------------------------------------------

static void testGenerateRecordContent() {
  std::cout << "\n--- GenerateRecordContent ---\n";

  // single struct
  {
    const std::string s = Generator::GenerateRecordContent({Record{false, "S", "f.hpp", {"f"}, {}}});
    check(s.rfind("// Auto-generated", 0) == 0, "starts with auto-generated comment");
    check(has(s, "#pragma once"), "contains pragma once");
    check(has(s, "#define SERIALIZE_S"), "contains struct macro");
  }

  // multiple records — order preserved, both macros present
  {
    const std::string s = Generator::GenerateRecordContent(
      {
        Record{false, "A", "f.hpp", {}, {}},
        Record{true, "B", "f.hpp", {}, {}},
      }
    );
    check(has(s, "#define SERIALIZE_A"), "first record macro present");
    check(has(s, "#define SERIALIZE_B"), "second record macro present");
    check(
      s.find("#define SERIALIZE_A") < s.find("#define SERIALIZE_B"),
      "records appear in declaration order"
    );
  }
}

// ---- WriteFileIfChanged ----------------------------------------------------

static void testWriteFileIfChanged() {
  std::cout << "\n--- WriteFileIfChanged ---\n";

  const std::string path = (fs::temp_directory_path() / "hf_wfic_test.txt").string();
  const std::string content = "hello\n";
  fs::remove(path); {
    const bool wrote = Generator::WriteFileIfChanged(path, content, false);
    check(wrote, "writes new file");
    check(fs::exists(path), "file exists after write");
    check(readFile(path) == content, "file content matches");
  } {
    const bool wrote = Generator::WriteFileIfChanged(path, content, false);
    check(!wrote, "skips write when content identical");
  } {
    const std::string changed = "changed\n";
    const bool wrote = Generator::WriteFileIfChanged(path, changed, false);
    check(wrote, "writes when content differs");
    check(readFile(path) == changed, "updated content stored");
  } {
    const std::string cur = readFile(path);
    const bool wrote = Generator::WriteFileIfChanged(path, cur, true);
    check(wrote, "override forces write even when content identical");
  }

  fs::remove(path);
}

// ---- AddGeneratedInclude ---------------------------------------------------

static void testAddGeneratedInclude() {
  std::cout << "\n--- AddGeneratedInclude ---\n";

  // inserts after the last existing include
  {
    const std::string path = writeTmp(
      "#pragma once\n"
      "#include <string>\n"
      "#include <vector>\n"
      "class Foo {};\n"
    );
    Generator::AddGeneratedInclude(path, "Foo.gen.hpp");
    const std::string result = readFile(path);
    const auto vecPos = result.find("#include <vector>");
    const auto genPos = result.find("#include \"Foo.gen.hpp\"");
    check(genPos != std::string::npos, "include added");
    check(genPos > vecPos, "inserted after last existing include");
    fs::remove(path);
  }

  // inserts at the top when no includes exist
  {
    const std::string path = writeTmp("class Foo {};\n");
    Generator::AddGeneratedInclude(path, "Foo.gen.hpp");
    const std::string result = readFile(path);
    check(
      result.rfind("#include \"Foo.gen.hpp\"", 0) == 0,
      "inserted at top when no includes present"
    );
    fs::remove(path);
  }

  // idempotent — second call does not duplicate the include
  {
    const std::string path = writeTmp(
      "#include \"Bar.gen.hpp\"\n"
      "struct Bar {};\n"
    );
    Generator::AddGeneratedInclude(path, "Bar.gen.hpp");
    const std::string result = readFile(path);
    size_t count = 0, pos = 0;
    while ((pos = result.find("Bar.gen.hpp", pos)) != std::string::npos) {
      ++count;
      ++pos;
    }
    check(count == 1, "include not duplicated on second call");
    fs::remove(path);
  }
}

// ---- InjectSerializeMacro --------------------------------------------------

static void testInjectSerializeMacro() {
  std::cout << "\n--- InjectSerializeMacro ---\n";

  // injects macro on the line after the opening brace
  {
    const std::string path = writeTmp(
      "struct MyStruct {\n"
      "  int x;\n"
      "};\n"
    );
    Generator::InjectSerializeMacro(path, {Record{false, "MyStruct", path, {}, {}}});
    const std::string result = readFile(path);
    check(has(result, "SERIALIZE_MYSTRUCT"), "macro injected");
    check(
      result.find("SERIALIZE_MYSTRUCT") > result.find('{'),
      "macro appears after opening brace"
    );
    fs::remove(path);
  }

  // works for classes too
  {
    const std::string path = writeTmp(
      "class MyClass {\n"
      "public:\n"
      "  int val;\n"
      "};\n"
    );
    Generator::InjectSerializeMacro(path, {Record{true, "MyClass", path, {}, {}}});
    const std::string result = readFile(path);
    check(has(result, "SERIALIZE_MYCLASS"), "class macro injected");
    fs::remove(path);
  }

  // idempotent
  {
    const std::string path = writeTmp(
      "struct S {\n"
      "  SERIALIZE_S\n"
      "  int x;\n"
      "};\n"
    );
    Generator::InjectSerializeMacro(path, {Record{false, "S", path, {}, {}}});
    const std::string result = readFile(path);
    size_t count = 0, pos = 0;
    while ((pos = result.find("SERIALIZE_S", pos)) != std::string::npos) {
      ++count;
      ++pos;
    }
    check(count == 1, "macro not duplicated on second call");
    fs::remove(path);
  }

  // qualified name — macro uses simple name only
  {
    const std::string path = writeTmp(
      "class MyClass {\n"
      "  int x;\n"
      "};\n"
    );
    Generator::InjectSerializeMacro(path, {Record{true, "NS::MyClass", path, {}, {}}});
    const std::string result = readFile(path);
    check(has(result, "SERIALIZE_MYCLASS"), "qualified name uses simple name in macro");
    check(!has(result, "SERIALIZE_NS"), "namespace not included in macro name");
    fs::remove(path);
  }
}

// ---- InjectReflectMacro ----------------------------------------------------

static void testInjectReflectMacro() {
  std::cout << "\n--- InjectReflectMacro ---\n";

  // injects macro on the line after the closing brace
  // (needs at least one line after the enum's }; for the insertion point check)
  {
    const std::string path = writeTmp(
      "enum MyEnum {\n"
      "  Val1,\n"
      "  Val2\n"
      "};\n"
      "// end\n"
    );
    Generator::InjectReflectMacro(path, {Enum{"MyEnum", {"Val1", "Val2"}}});
    const std::string result = readFile(path);
    check(has(result, "REFLECT_MYENUM"), "reflect macro injected");
    check(result.find("REFLECT_MYENUM") > result.find("};"), "macro appears after closing brace");
    fs::remove(path);
  }

  // idempotent
  {
    const std::string path = writeTmp(
      "enum E { A };\n"
      "REFLECT_E\n"
    );
    Generator::InjectReflectMacro(path, {Enum{"E", {"A"}}});
    const std::string result = readFile(path);
    size_t count = 0, pos = 0;
    while ((pos = result.find("REFLECT_E", pos)) != std::string::npos) {
      ++count;
      ++pos;
    }
    check(count == 1, "reflect macro not duplicated on second call");
    fs::remove(path);
  }
}

// ---- main ------------------------------------------------------------------

int main() {
  testGenerateReflectionFactoryCode();
  testGenerateSaveFunction();
  testGenerateLoadFunction();
  testGenerateRecordMacro();
  testGenerateEnumReflectionMacro();
  testGenerateRecordContent();
  testWriteFileIfChanged();
  testAddGeneratedInclude();
  testInjectSerializeMacro();
  testInjectReflectMacro();

  std::cout << "\n";
  for (const auto &f: failures)
    std::cout << "FAIL: " << f << "\n";
  std::cout << "\n" << passed << " passed, " << failed << " failed\n";
  return failed > 0 ? 1 : 0;
}

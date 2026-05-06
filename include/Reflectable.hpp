//
// Serializable.hpp
// Author: Antoine Bastide
// Date: 19.06.2025
//

#ifndef REFLECTABLE_HPP
#define REFLECTABLE_HPP

#include <string_view>

#include "Save.hpp"
#include "Utils.hpp"

namespace Serde {
  class ReflectionFactory;
}

#define _e_SERIALIZE_RECORD \
  friend class Editor::EntityInspector; \
  friend class Engine::Reflection::ReflectionFactory; \
  template<typename T> friend bool _e_renderInEditor(T &, const std::string &, bool, const std::string &); \
  public: \
    [[nodiscard]] std::string_view ClassNameQualified() const override { return CLASS_NAME_FULLY_QUALIFIED; } \
    [[nodiscard]] std::string_view ClassName() const override { return CLASS_NAME; } \
    [[nodiscard]] static std::string_view ClassNameStatic() { return CLASS_NAME; }
#define _e_SERIALIZE_STRING "serialize"
#define _e_NON_SERIALIZABLE_STRING "non_serializable"

// Since only the tool itself uses the annotations, only make the macros add them when the tool is run
// This should prevent compilation errors on compilers that do not support custom annotation (ex: MSVC)
// This allows the engine to be compiled with clang, gcc or MSVC but enforces the Header Forge to be compiled with clang
#ifdef HEADER_FORGE_ENABLE_ANNOTATIONS
#define SERIALIZE __attribute__((annotate(_e_SERIALIZE_STRING))) SHOW_IN_INSPECTOR
#define NON_SERIALIZABLE __attribute__((annotate(_e_NON_SERIALIZABLE_STRING)))
#else
#define SERIALIZE
#define NON_SERIALIZABLE
#endif

namespace Serde {
  /// Class used to accurately save classes
  struct Reflectable {
    virtual ~Reflectable() = default;
    /// @returns The name of the current class with its namespace and parent classes as a string
    [[nodiscard]] virtual std::string_view ClassNameQualified() const = 0;
    /// @returns The name of the current class without its namespace and parent classes as a string
    [[nodiscard]] virtual std::string_view ClassName() const = 0;

    /// Serializes the current class instance
    virtual void _e_save(Format format, JSON &json) const = 0;
    /// Called when this class instance is serialized/saved
    /// @param format The format in which to save the data
    /// @param json The JSON representation of this class instance
    virtual void OnSerialize(Format format, JSON &json) const {}

    /// Loads a class instance
    virtual void _e_load(Format format, const JSON &json) = 0;
    /// Called when this class instance is deserialized/loaded
    /// @param format The format in which to load the data
    /// @param json The JSON representation of this class instance
    virtual void OnDeserialize(Format format, const JSON &json) {}
  };
}

#endif //REFLECTABLE_HPP

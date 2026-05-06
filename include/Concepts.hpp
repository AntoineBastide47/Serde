//
// Concepts.hpp
// Author: Antoine Bastide
// Date: 18.06.2025
//

#ifndef CONCEPTS_HPP
#define CONCEPTS_HPP

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Serde {
  class JSON;
  struct Reflectable;

  template<typename T> concept IsConst = std::is_const_v<std::remove_reference_t<T>>;

  template<typename T> concept IsNonOwningString =
      std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
      std::is_same_v<std::remove_cvref_t<T>, const char *> ||
      std::is_same_v<std::remove_cvref_t<T>, char *>;

  template<typename T> concept IsString = std::is_same_v<std::remove_cvref_t<T>, std::string> || IsNonOwningString<T>;

  template<typename T> concept IsNumber =
      std::is_arithmetic_v<std::remove_cvref_t<T>> &&
      !std::is_same_v<std::remove_cvref_t<T>, bool> &&
      !IsString<T>;

  template<typename T> concept IsTuple = requires {
    std::tuple_size_v<std::remove_cvref_t<T>>;
  };

  template<typename T> concept IsPair =
      requires {
        typename std::remove_cvref_t<T>;
      } && std::is_same_v<
        std::remove_cvref_t<T>,
        std::pair<typename std::remove_cvref_t<T>::first_type, typename std::remove_cvref_t<T>::second_type>
      >;

  template<typename T> concept IsMap =
      requires {
        typename T::value_type;
        typename T::key_type;
        typename T::mapped_type;
        std::begin(std::declval<T &>());
        std::end(std::declval<T &>());
      } && IsPair<std::remove_cvref_t<typename T::value_type>>;

  template<typename T> concept IsContainer =
      requires {
        typename T::value_type;
        std::begin(std::declval<T &>());
        std::end(std::declval<T &>());
      } && !IsMap<T> && !IsString<T>;

  template<typename T> concept IsSharedPtr = requires(T ptr) {
    typename T::element_type;
    requires std::is_same_v<std::remove_cvref_t<T>, std::shared_ptr<typename T::element_type>>;
  };

  template<typename T> concept IsUniquePtr = requires(T ptr) {
    typename T::element_type;
    typename T::deleter_type;
    requires std::is_same_v<std::remove_cvref_t<T>,
      std::unique_ptr<typename T::element_type, typename T::deleter_type>>;
  };

  template<typename T> concept IsSmartPtr = IsSharedPtr<T> || IsUniquePtr<T>;

  template<typename T> concept IsEnum = std::is_enum_v<T>;

  template<typename T> concept IsReflectable = std::is_base_of_v<Reflectable, std::remove_cvref_t<T>>;
  template<typename T> concept IsNotReflectable = !IsReflectable<T>;

  enum class Format {
    JSON, TEXT, BINARY
  };

  template<typename> static constexpr bool _e_f = false;

  template<typename T, typename Format>
  concept HasFreeSave = requires(T &data, Format fmt, JSON &json) {
    { _e_save(data, fmt, json) } -> std::same_as<void>;
  };

  template<typename T, typename Format>
  concept HasMemberSave = requires(const T &obj, Format fmt, JSON &json) {
    { obj._e_save(fmt, json) } -> std::same_as<void>;
  };

  template<typename T, typename Format>
  concept HasSaveFunction = HasFreeSave<T, Format> || HasMemberSave<T, Format>;

  template<typename T, typename Format>
  concept HasFreeLoad = requires(T &t, Format fmt, const JSON &j) {
    { _e_load(t, fmt, j) } -> std::same_as<void>;
  };

  template<typename T, typename Format>
  concept HasMemberLoad = requires(T &obj, Format fmt, const JSON &j) {
    { obj._e_load(fmt, j) } -> std::same_as<void>;
  };

  template<typename T, typename Format>
  concept HasLoadFunction = HasFreeLoad<T, Format> || HasMemberLoad<T, Format>;
}

#endif //CONCEPTS_HPP

//
// Save.hpp
// Author: Antoine Bastide
// Date: 22.06.2025
//

#ifndef SAVE_HPP
#define SAVE_HPP

#include "Concepts.hpp"
#include "JSON.hpp"

namespace Serde {
  template<typename T> static void _e_saveImpl(const T &data, const Format format, JSON &json) {
    if constexpr (HasSaveFunction<T, Format>)
      _e_save(data, format, json);
    else
      static_assert(
        _e_f<T>, R"(
No save overloads were found for the requested type.
 - if the type is a part of the STL, convert it to a supported STL type before the save call
 - if the type is not a part of the STL, make sure the type publicly inherits from Serde::Reflectable
)"
      );
  }

  template<IsNumber T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON)
      json = data;
  }

  template<IsString T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON)
      json = data;
  }

  template<typename T> requires std::is_same_v<std::decay_t<T>, bool>
  static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON)
      json = data;
  }

  template<typename T, size_t N> static void _e_save(const T (&data)[N], const Format format, JSON &json) {
    if (format == Format::JSON) {
      json = JSON::Array();
      json.ReserveAndResize(N);

      for (size_t i = 0; i < N; ++i) {
        JSON value;
        _e_saveImpl(data[i], format, value);
        json.At(i) = value;
      }
    }
  }

  template<IsContainer T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON) {
      json = JSON::Array();
      json.ReserveAndResize(std::distance(data.begin(), data.end()));

      size_t i = 0;
      for (const auto &item: data) {
        JSON value;
        _e_saveImpl(item, format, value);
        json.At(i++) = value;
      }
    }
  }

  template<IsMap T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON) {
      json = JSON::Object();
      if constexpr (requires { data.reserve(); })
        json.Reserve(data.size());

      for (const auto &[k, v]: data) {
        JSON key;
        _e_saveImpl(k, format, key);
        _e_saveImpl(v, format, json[key.Dump()]);
      }
    }
  }

  template<IsPair T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (format == Format::JSON) {
      json.ReserveAndResize(2);
      _e_saveImpl(data.first, format, json.At(0));
      _e_saveImpl(data.second, format, json.At(1));
    }
  }

  template<typename... Ts> static void _e_save(const std::tuple<Ts...> &t, const Format format, JSON &json) {
    if (format == Format::JSON) {
      json.ReserveAndResize(std::tuple_size_v<std::tuple<Ts...>>);

      // ReSharper disable once CppDFAUnreadVariable
      size_t i = 0;
      std::apply(
        [&](const Ts &... elems) {
          (([&] {
            JSON value;
            _e_saveImpl(elems, format, value);
            json.At(i++) = std::move(value);
          }()), ...);
        }, t
      );
    }
  }

  template<IsSmartPtr T> static void _e_save(const T &data, const Format format, JSON &json) {
    if (data) {
      if constexpr (IsNotReflectable<typename T::element_type>) {
        _e_saveImpl(*data, format, json);
      } else {
        json = JSON::Object();
        _e_saveImpl(data->ClassNameQualified(), format, json["type"]);
        data->_e_save(format, json["data"]);
        data->OnSerialize(format, json["data"]);
      }
    } else
      json = {};
  }

  template<IsReflectable T> static void _e_save(const T &data, const Format format, JSON &json) {
    if constexpr (requires {
      data._e_save(format, json);
    }) {
      data._e_save(format, json);
      data.OnSerialize(format, json);
    } else
      static_assert(
        _e_f<T>,
        "Missing save function. Ensure the type uses it's SERIALIZE_* macro."
      );
  }

  template<typename T> requires std::is_enum_v<T>
  static void _e_save(const T &data, const Format format, JSON &json) {
    _e_saveImpl(static_cast<std::underlying_type_t<T>>(data), format, json);
  }

  template<typename... Ts> static void _e_save(const std::variant<Ts...> &v, const Format format, JSON &json) {
    if (format == Format::JSON) {
      json = JSON::Object();
      json["index"] = v.index();

      JSON value;
      std::visit(
        [&](const auto &elem) {
          _e_saveImpl(elem, format, value);
        }, v
      );
      json["value"] = value;
    }
  }
}

#endif //SAVE_HPP

//
// Utils.hpp
// Author: Antoine Bastide
// Date: 08/03/2025
//

#ifndef UTILS_H
#define UTILS_H

#include <string_view>

#ifdef _MSC_VER
#define FUNC_SIG __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define FUNC_SIG __PRETTY_FUNCTION__
#else
#error "Unknown Compiler"
#endif

constexpr std::string_view className(const std::string_view prettyFunction, const bool fullyQualified) {
  const size_t colons = prettyFunction.rfind("::");
  if (colons == std::string_view::npos)
    return "::";

  if (!fullyQualified) {
    const size_t end = colons;
    size_t start = end;
    int angle_depth = 0;

    while (start > 0) {
      --start;
      if (const char c = prettyFunction[start]; c == '>')
        angle_depth++;
      else if (c == '<')
        angle_depth--;
      else if (angle_depth == 0 && (c == ':' || c == ' '))
        break;
    }
    ++start;
    return prettyFunction.substr(start, end - start);
  }

  const size_t end = colons;
  size_t start = end;
  int angle_depth = 0;

  while (start > 0) {
    --start;
    if (const char c = prettyFunction[start]; c == '>')
      angle_depth++;
    else if (c == '<')
      angle_depth--;
    else if (angle_depth == 0 &&
             !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':'))
      break;
  }
  ++start;
  return prettyFunction.substr(start, end - start);
}

#define CLASS_NAME className(FUNC_SIG, false)
#define CLASS_NAME_FULLY_QUALIFIED className(FUNC_SIG, true)

#endif //UTILS_H

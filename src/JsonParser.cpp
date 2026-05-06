//
// JsonParser.cpp
// Author: Antoine Bastide
// Date: 07.07.2025
//

#include <cmath>
#include <cstring>

#include "JsonParser.hpp"
#include "JSON.hpp"

#if DEBUG
#define THROW_JSON_ERROR(msg) throw std::logic_error(std::string(msg) + " at line " + std::to_string(line) + ", column " + std::to_string(column))
#else
#define THROW_JSON_ERROR(msg) throw std::logic_error(msg)
#endif

namespace Serde {
  JSONParser::JSONParser(std::istream &stream)
    : stream(&stream), streamView(""), svData(nullptr), svSize(0),
      ptr(nullptr), end(nullptr), candidateKeyIsSet(false), pendingKeyIsSet(),
      commaDetected(true), foundData(false), depth(0), line(1), column(1) {}

  JSONParser::JSONParser(const std::string_view json)
    : stream(nullptr), streamView(""), svData(json.data()), svSize(json.size()),
      ptr(nullptr), end(nullptr), candidateKeyIsSet(false), pendingKeyIsSet(),
      commaDetected(true), foundData(false), depth(0), line(1), column(1) {}

  static constexpr auto validNumberChar = []() noexcept {
    std::array<bool, 256> t{};
    for (const uint8_t c : {
      '0','1','2','3','4','5','6','7','8','9','e','E','-','+','.'
    }) t[c] = true;
    return t;
  }();

  static constexpr auto escapedMap = []() noexcept {
    std::array<char, 256> t{};
    t[static_cast<uint8_t>('"')]  = '"';
    t[static_cast<uint8_t>('\\')] = '\\';
    t[static_cast<uint8_t>('/')]  = '/';
    t[static_cast<uint8_t>('b')]  = '\b';
    t[static_cast<uint8_t>('f')]  = '\f';
    t[static_cast<uint8_t>('n')]  = '\n';
    t[static_cast<uint8_t>('r')]  = '\r';
    t[static_cast<uint8_t>('t')]  = '\t';
    return t;
  }();

  // Marks characters that terminate a plain string run: control chars (0x00-0x1F), '"', '\\'
  static constexpr auto stringStopChar = []() noexcept {
    std::array<bool, 256> t{};
    for (uint8_t i = 0; i <= 0x1F; ++i) t[i] = true;
    t[static_cast<uint8_t>('"')]  = true;
    t[static_cast<uint8_t>('\\')] = true;
    return t;
  }();

  JSON JSONParser::Parse() {
    JSON json;
    stack.push_back(&json);

    // Fast path: string_view input is already in memory — no buffering needed.
    if (stream == nullptr) {
      const auto consumed = static_cast<size_t>(parseBuffer(svData, svSize, true));

      if (!foundData)
        THROW_JSON_ERROR("Empty input is not valid JSON");
      if (!stack.empty()) {
        if (stack.back()->IsObject())
          THROW_JSON_ERROR("Missing closing '}' for object");
        if (stack.back()->IsArray())
          THROW_JSON_ERROR("Missing closing ']' for array");
      }

      size_t tail = consumed;
      while (tail < svSize && (svData[tail] == ' ' || svData[tail] == '\t' || svData[tail] == '\r' || svData[tail] == '\n'))
        ++tail;
      if (tail < svSize) {
        ++column;
        eof(svData[tail]);
      }
      return json;
    }

    // Buffered path: read istream in chunks.
    constexpr size_t blockSize = 1024 * 64;
    size_t bufferSize = blockSize;
    std::vector<char> buffer(bufferSize);
    size_t buffered = 0;
    size_t consumed = 0;

    std::istream &iss = *stream;
    while (!stack.empty() && (iss || buffered - consumed > 0)) {
      if (const size_t unconsumed = buffered - consumed; unconsumed < bufferSize) {
        std::memmove(buffer.data(), buffer.data() + consumed, unconsumed);
        buffered = unconsumed;
        consumed = 0;
      } else {
        bufferSize *= 2;
        buffer.resize(bufferSize);
        buffer.reserve(bufferSize);
      }

      if (iss && buffered < bufferSize) {
        iss.read(buffer.data() + buffered, static_cast<std::streamsize>(bufferSize - buffered));
        const auto readCount = static_cast<size_t>(iss.gcount());
        if (readCount == 0 && iss.eof())
          break;
        buffered += readCount;
      }

      consumed = static_cast<size_t>(parseBuffer(buffer.data(), buffered, iss.eof()));
    }

    if (!foundData)
      THROW_JSON_ERROR("Empty input is not valid JSON");
    if (!stack.empty()) {
      if (stack.back()->IsObject())
        THROW_JSON_ERROR("Missing closing '}' for object");
      if (stack.back()->IsArray())
        THROW_JSON_ERROR("Missing closing ']' for array");
    }

    size_t tail = consumed;
    while (tail < buffered && (buffer[tail] == ' ' || buffer[tail] == '\t' || buffer[tail] == '\r' || buffer[tail] == '\n'))
      ++tail;
    if (tail < buffered) {
      ++column;
      eof(buffer[tail]);
    } else if (iss) {
      int c;
      while ((c = iss.peek()) != EOF && (c == ' ' || c == '\t' || c == '\r' || c == '\n'))
        iss.get();
      if (iss.peek() != EOF) {
        ++column;
        eof(static_cast<char>(iss.peek()));
      }
    }

    return json;
  }

  int JSONParser::parseBuffer(const char *buffer, const size_t buffered, const bool isLastBuffer) {
    const char *p = buffer;
    const char *bufferEnd = buffer + buffered;

    while (p < bufferEnd) {
      #if DEBUG
      ++column;
      #endif

      switch (const char c = *p; tokenTable[static_cast<unsigned char>(c)]) {
        #if DEBUG
        case TOKEN_WHITESPACE:
          break;
        case TOKEN_NEWLINE:
          ++line;
          column = 1;
          break;
          #else
        case TOKEN_WHITESPACE:
        case TOKEN_NEWLINE:
          break;
        #endif
        case TOKEN_OBJECT_START:
          if (++depth > MAX_NESTING_DEPTH)
            THROW_JSON_ERROR("Nesting depth limit exceeded");
          if (stack.back()->IsObject())
            stack.push_back(setObjectValue(JSON::Object()));
          else if (stack.back()->IsArray()) {
            setArrayValue(JSON::Object());
            stack.push_back(&stack.back()->Back());
          } else if (stack.size() == 1)
            *stack.back() = JSON::Object();
          foundData = true;
          break;
        case TOKEN_OBJECT_END:
          if (stack.back()->IsObject()) {
            if (pendingKeyIsSet)
              THROW_JSON_ERROR("Missing value for key '" + pendingKey + "' in object");
            if (commaDetected && !stack.back()->Empty())
              THROW_JSON_ERROR("Trailing ',' before closing '}'");
            --depth;
            stack.pop_back();
            if (stack.empty())
              return static_cast<int>(p - buffer + 1);
          } else if (stack.back()->IsArray())
            THROW_JSON_ERROR("Expected ']' but found '}'");
          else
            THROW_JSON_ERROR("Unexpected '}'");
          foundData = true;
          break;
        case TOKEN_ARRAY_START:
          if (++depth > MAX_NESTING_DEPTH)
            THROW_JSON_ERROR("Nesting depth limit exceeded");
          if (stack.back()->IsObject())
            stack.push_back(setObjectValue(JSON::Array()));
          else if (stack.back()->IsArray()) {
            setArrayValue(JSON::Array());
            stack.push_back(&stack.back()->Back());
          } else if (stack.size() == 1)
            *stack.back() = JSON::Array();
          foundData = true;
          break;
        case TOKEN_ARRAY_END:
          if (stack.back()->IsArray()) {
            if (commaDetected && !stack.back()->Empty())
              THROW_JSON_ERROR("Trailing ',' before closing ']'");
            --depth;
            stack.pop_back();
            if (stack.empty())
              return static_cast<int>(p - buffer + 1);
          } else if (stack.back()->IsObject())
            THROW_JSON_ERROR("Expected '}' but found ']'");
          else
            THROW_JSON_ERROR("Unexpected ']'");
          foundData = true;
          break;
        case TOKEN_COLON:
          if (stack.back()->IsArray())
            THROW_JSON_ERROR("Unexpected ':' in an array, did you mean ','?");
          if (!candidateKeyIsSet)
            THROW_JSON_ERROR("Expected a string key before ':'");
          pendingKey = std::move(candidateKey);
          pendingKeyIsSet = true;
          foundData = true;
          candidateKeyIsSet = false;
          break;
        case TOKEN_COMMA:
          if (commaDetected)
            THROW_JSON_ERROR("Duplicate ','");
          commaDetected = true;
          pendingKeyIsSet = false;
          pendingKey.clear();
          candidateKey.clear();
          candidateKeyIsSet = false;
          foundData = true;
          break;
        case TOKEN_STRING: {
          ptr = p;
          const char *q = p + 1;
          bool done = false;
          while (q < bufferEnd) {
            if (*q == '"' && q[-1] != '\\') {
              end = q;
              JSON json = parseString();
              foundData = true;

              if (stack.back()->IsObject()) {
                if (!pendingKeyIsSet) {
                  if (!candidateKeyIsSet) {
                    candidateKey = json.GetString();
                    candidateKeyIsSet = true;
                  } else
                    THROW_JSON_ERROR("Missing a colon after a key");
                } else
                  setObjectValue(std::move(json));
              } else if (stack.back()->IsArray())
                setArrayValue(std::move(json));
              else if (stack.size() == 1 && !stack.back()->isComplexType()) {
                *stack.back() = std::move(json);
                stack.pop_back();
                return static_cast<int>(q - buffer + 1);
              }

              p = q;
              done = true;
              break;
            }
            ++q;
          }
          if (!done) {
            if (isLastBuffer)
              THROW_JSON_ERROR("Unterminated string");
            return static_cast<int>(p - buffer);
          }
          break;
        }
        case TOKEN_NUMBER_START: {
          if (stack.back()->IsObject() && !pendingKeyIsSet)
            THROW_JSON_ERROR("Expected a key before adding a number");

          ptr = p;
          const char *q = p + 1;
          bool done = false;
          while (q < bufferEnd) {
            if (!validNumberChar[static_cast<unsigned char>(*q)]) {
              end = q;
              JSON json = parseNumber();
              if (ptr != end)
                THROW_JSON_ERROR("Invalid character in number");
              foundData = true;

              if (stack.back()->IsObject())
                setObjectValue(std::move(json));
              else if (stack.back()->IsArray())
                setArrayValue(std::move(json));
              else if (stack.size() == 1 && !stack.back()->isComplexType()) {
                *stack.back() = std::move(json);
                stack.pop_back();
                return static_cast<int>(q - buffer);
              }

              p = q - 1;
              done = true;
              break;
            }
            ++q;
          }
          if (!done) {
            if (isLastBuffer) {
              end = q;
              JSON json = parseNumber();
              if (ptr != end)
                THROW_JSON_ERROR("Invalid character in number");
              foundData = true;
              if (stack.back()->IsArray())
                setArrayValue(std::move(json));
              else if (stack.back()->IsObject() && pendingKeyIsSet)
                setObjectValue(std::move(json));
              else {
                *stack.back() = std::move(json);
                stack.pop_back();
              }
              return static_cast<int>(q - buffer);
            }
            return static_cast<int>(p - buffer);
          }
          break;
        }
        case TOKEN_LITERAL_START: {
          if (stack.back()->IsObject() && !pendingKeyIsSet)
            THROW_JSON_ERROR("Expected a key before adding a boolean or null value");

          constexpr std::string_view true_lit = "true";
          constexpr std::string_view false_lit = "false";
          constexpr std::string_view null_lit = "null";

          const std::string_view lit = c == 't' ? true_lit : c == 'f' ? false_lit : null_lit;
          const JSON val = c == 't' ? true : c == 'f' ? false : JSON{};

          if (p + lit.size() > bufferEnd)
            return static_cast<int>(p - buffer);

          for (size_t i = 0; i < lit.size(); ++i)
            if (p[i] != lit[i])
              unexpected(p[i]);

          if (stack.back()->IsObject())
            setObjectValue(val);
          else if (stack.back()->IsArray())
            setArrayValue(val);
          else if (stack.size() == 1 && !stack.back()->isComplexType()) {
            *stack.back() = val;
            foundData = true;
            return static_cast<int>(p - buffer + lit.size());
          }

          p += lit.size() - 1;
          foundData = true;
          break;
        }
        default:
          unexpected(c);
      }

      ++p;
    }

    return static_cast<int>(p - buffer);
  }

  JSON *JSONParser::setObjectValue(JSON json) {
    JSON *stackBack = stack.back();

    if (!commaDetected && !stackBack->Empty())
      THROW_JSON_ERROR("Missing ',' between object members");

    if (!pendingKeyIsSet)
      THROW_JSON_ERROR("Expected a key before adding an inner value");

    auto &entries = stackBack->GetObject();
    for (auto &[k, v]: entries) {
      if (k == pendingKey) {
        v = std::move(json);
        pendingKey.clear();
        candidateKey.clear();
        pendingKeyIsSet = false;
        commaDetected = false;
        return &v;
      }
    }

    entries.emplace_back(std::move(pendingKey), std::move(json));
    pendingKey.clear();
    candidateKey.clear();
    pendingKeyIsSet = false;
    commaDetected = false;

    return &entries.back().second;
  }

  void JSONParser::setArrayValue(JSON json) {
    if (!commaDetected && !stack.back()->Empty())
      THROW_JSON_ERROR("Missing ',' between array members");
    stack.back()->PushBack(std::move(json));
    commaDetected = false;
  }

  JSON JSONParser::parseNumber() {
    // Optional minus
    bool neg = false;
    #if DEBUG
    const char *start = ptr;
    #endif
    if (*ptr == '-') {
      neg = true;
      ++ptr;
    }

    if (ptr == end)
      THROW_JSON_ERROR("Invalid number: digit expected after '-'");

    uint64_t intPart = 0;
    if (*ptr == '0') {
      ++ptr;
      if (isdigit(*ptr))
        THROW_JSON_ERROR("Invalid number: Leading zeros are not allowed");
    } else if (isdigit(*ptr))
      while (ptr < end && isdigit(*ptr)) {
        intPart = intPart * 10 + (*ptr - '0');
        ++ptr;
      }
    else
      THROW_JSON_ERROR("Invalid number: digit expected after '-'");

    // Optional fraction
    double fracPart = 0.0;
    if (ptr < end && *ptr == '.') {
      ++ptr;
      if (ptr == end || !isdigit(*ptr))
        THROW_JSON_ERROR("Invalid number: digit expected after '.'");
      double factor = 0.1;
      while (ptr < end && isdigit(*ptr)) {
        fracPart += (*ptr - '0') * factor;
        factor *= 0.1;
        ++ptr;
      }
    }
    double value = static_cast<double>(intPart) + fracPart;

    // Optional exponent
    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
      ++ptr;
      bool expNeg = false;
      if (ptr < end && (*ptr == '+' || *ptr == '-')) {
        expNeg = *ptr == '-';
        ++ptr;
      }
      if (ptr == end || !isdigit(*ptr))
        THROW_JSON_ERROR("Invalid number: digit expected after exponent");
      int exponent = 0;
      while (ptr < end && isdigit(*ptr)) {
        exponent = exponent * 10 + (*ptr - '0');
        ++ptr;
      }
      value *= std::pow(10.0, exponent * (1 - 2 * expNeg));
    }

    #if DEBUG
    column += ptr - start;
    #endif
    return JSON(value * (1 - 2 * neg));
  }

  static std::string hexString(const uint16_t value) {
    static constexpr char hexDigits[] = "0123456789ABCDEF";
    std::string str(4, '\0');
    str[0] = hexDigits[value >> 12 & 0xF];
    str[1] = hexDigits[value >> 8 & 0xF];
    str[2] = hexDigits[value >> 4 & 0xF];
    str[3] = hexDigits[value & 0xF];
    return str;
  }

  thread_local std::string result;
  JSON JSONParser::parseString() {
    #if DEBUG
    const char *start = ptr;
    #endif

    // ptr points to the opening '"'; skip it so the content loop starts at the first real character.
    ++ptr;

    result.clear();
    if (const auto approxLen = static_cast<size_t>(end - ptr); approxLen > 0)
      result.reserve(approxLen);

    while (ptr < end) {
      if (*ptr == '\\') {
        ++ptr;
        if (ptr == end)
          THROW_JSON_ERROR("Unterminated escape sequence");

        if (const char escaped = escapedMap[static_cast<unsigned char>(*ptr)]) {
          result.push_back(escaped);
          ++ptr;
        } else if (*ptr == 'u') {
          ++ptr;
          if (end - ptr < 4)
            THROW_JSON_ERROR("Incomplete unicode escape");

          const uint16_t first = parseHex4();
          uint32_t code = first;

          if (0xD800 <= first && first <= 0xDBFF) {
            if (end - ptr < 6)
              THROW_JSON_ERROR("Unexpected end of input: missing low surrogate after high surrogate (\\uXXXX)");

            if (ptr[0] != '\\' || ptr[1] != 'u')
              THROW_JSON_ERROR("Expected '\\u' after high surrogate, found '" + std::string(ptr, ptr + 2) + "'");

            ptr += 2;
            const uint16_t second = parseHex4();

            if (second < 0xDC00 || second > 0xDFFF)
              THROW_JSON_ERROR(
              "Invalid low surrogate: expected value in range \\uDC00..\\uDFFF, got \\u" + hexString(second)
            );

            code = 0x10000 + ((first - 0xD800) << 10 | second - 0xDC00);
          } else if (0xDC00 <= first && first <= 0xDFFF)
            THROW_JSON_ERROR("Unexpected low surrogate \\u" + hexString(first) + " without preceding high surrogate");

          if (code < 0x80)
            result.push_back(static_cast<char>(code));
          else if (code < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (code >> 6)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else if (code < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (code >> 12)));
            result.push_back(static_cast<char>(0x80 | (code >> 6 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else {
            result.push_back(static_cast<char>(0xF0 | (code >> 18)));
            result.push_back(static_cast<char>(0x80 | (code >> 12 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code >> 6 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
        } else
          THROW_JSON_ERROR("Invalid escape sequence");
      } else {
        const char *runStart = ptr;
        while (ptr < end && !stringStopChar[static_cast<unsigned char>(*ptr)])
          ++ptr;
        if (ptr < end && *ptr != '"' && *ptr != '\\')
          THROW_JSON_ERROR("Control character in string");
        result.append(runStart, ptr - runStart);
      }
    }

    #if DEBUG
    column += ptr - start;
    #endif
    return JSON(result);
  }

  uint16_t JSONParser::parseHex4() {
    uint16_t code = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = *ptr++;
      code <<= 4;
      if (c >= '0' && c <= '9')
        code |= c - '0';
      else if (c >= 'a' && c <= 'f')
        code |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        code |= c - 'A' + 10;
      else
        THROW_JSON_ERROR("Invalid hex digit in \\uXXXX");
    }
    return code;
  }

  void JSONParser::unexpected(const char c) const {
    if (static_cast<int>(c) == 0)
      THROW_JSON_ERROR("Unexpected character '\\0'");
    THROW_JSON_ERROR(
      static_cast<std::string>("Unexpected character '") + c + '\''
    );
  }

  void JSONParser::eof(const char c) const {
    if (c == 0)
      THROW_JSON_ERROR("Unexpected null byte after JSON end");
    THROW_JSON_ERROR(static_cast<std::string>("Unexpected character '") + c + "' after JSON end");
  }
}

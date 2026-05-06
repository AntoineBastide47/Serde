//
// Serde.hpp
// Author: Antoine Bastide
// Date: 07.07.2025
//

#ifndef JSON_PARSER_HPP
#define JSON_PARSER_HPP

#include <iosfwd>
#include <istream>
#include <string_view>
#include <vector>

namespace Serde {
  class JSON;

  class JSONParser final {
    public:
      /// Creates a parser from the given std::istream
      explicit JSONParser(std::istream &stream);
      /// Creates a parser from the given std::string_view
      explicit JSONParser(std::string_view json);

      /// @returns The parsed data stored in the parser
      JSON Parse();
    private:
      class StringViewBuf : public std::streambuf {
        public:
          explicit StringViewBuf(std::string_view view) {
            const auto data = const_cast<char *>(view.data());
            setg(data, data, data + view.size());
          }
      };

      class StringViewIStream : public std::istream {
        public:
          explicit StringViewIStream(const std::string_view view)
            : std::istream(&buf), buf(view) {}
        private:
          StringViewBuf buf;
      };

      enum TokenType : uint8_t {
        TOKEN_OTHER,
        TOKEN_OBJECT_START,
        TOKEN_OBJECT_END,
        TOKEN_ARRAY_START,
        TOKEN_ARRAY_END,
        TOKEN_COLON,
        TOKEN_COMMA,
        TOKEN_STRING,
        TOKEN_NUMBER_START,
        TOKEN_LITERAL_START,
        TOKEN_WHITESPACE,
        TOKEN_NEWLINE,
      };

      static constexpr TokenType tokenTable[256] = {
        ['{'] = TOKEN_OBJECT_START,
        ['}'] = TOKEN_OBJECT_END,
        ['['] = TOKEN_ARRAY_START,
        [']'] = TOKEN_ARRAY_END,
        [':'] = TOKEN_COLON,
        [','] = TOKEN_COMMA,
        ['"'] = TOKEN_STRING,
        ['-'] = TOKEN_NUMBER_START,
        ['0'] = TOKEN_NUMBER_START,
        ['1'] = TOKEN_NUMBER_START,
        ['2'] = TOKEN_NUMBER_START,
        ['3'] = TOKEN_NUMBER_START,
        ['4'] = TOKEN_NUMBER_START,
        ['5'] = TOKEN_NUMBER_START,
        ['6'] = TOKEN_NUMBER_START,
        ['7'] = TOKEN_NUMBER_START,
        ['8'] = TOKEN_NUMBER_START,
        ['9'] = TOKEN_NUMBER_START,
        ['t'] = TOKEN_LITERAL_START,
        ['f'] = TOKEN_LITERAL_START,
        ['n'] = TOKEN_LITERAL_START,
        [' '] = TOKEN_WHITESPACE,
        ['\t'] = TOKEN_WHITESPACE,
        ['\r'] = TOKEN_WHITESPACE,
        ['\n'] = TOKEN_NEWLINE,
      };

      /// The current istream being parsed
      std::istream *stream;
      /// The current StringViewIStream being parsed (istream path only)
      StringViewIStream streamView;
      /// Raw pointer into the string_view data (string_view path only)
      const char *svData;
      /// Size of the string_view data (string_view path only)
      size_t svSize;
      /// The current character in the string_view being parsed
      const char *ptr;
      /// The last character in the string_view being parsed
      const char *end;

      static constexpr size_t MAX_NESTING_DEPTH = 1024;

      /// The current JSON object/array tree
      std::vector<JSON *> stack;
      /// The key waiting to be assigned
      std::string pendingKey;
      /// The string value that could be turned into a key
      std::string candidateKey;
      /// Whether candidateKey was set by actually parsing a string token
      bool candidateKeyIsSet;
      /// Whether the current pending key has been assigned
      bool pendingKeyIsSet;
      /// Whether a comma has been detected by the parser
      bool commaDetected;
      /// Whether JSON data has been detected in the istream
      bool foundData;
      /// Current nesting depth (objects + arrays)
      size_t depth;

      /// The current line number the parsed character is located on
      size_t line;
      /// The current column number the parsed character is located on
      size_t column;

      /// Parses the given buffer; isLastBuffer signals no more data will follow
      int parseBuffer(const char *buffer, size_t buffered, bool isLastBuffer);
      /// Assigns the given JSON value to the object at the back of the JSON stack and to the pending key
      JSON *setObjectValue(JSON json);
      /// Assigns the given JSON value to the array at the back of the JSON
      void setArrayValue(JSON json);
      /// Parses the current number in the string_view
      JSON parseNumber();
      /// Parses the current string in the string_view
      JSON parseString();

      /// Converts a hex of size 4 into a uint16_t
      uint16_t parseHex4();
      /// Throws an unexpected error for the given char if it shouldn't be their
      void unexpected(char c) const;
      /// Throws an unexpected error for the given char if it comes after the end of the main JSON value
      void eof(char c) const;
  };
}

#endif //JSON_PARSER_HPP

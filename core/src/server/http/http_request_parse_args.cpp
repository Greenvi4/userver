#include "http_request_parse_args.hpp"

#include <cstring>

#include <userver/utils/encoding/hex.hpp>

namespace server::http::parser {

void ParseArgs(
    std::string_view args,
    std::unordered_map<std::string, std::vector<std::string>>& result) {
  const char* end = args.data() + args.size();
  const char* key_begin = args.data();
  const char* key_end = args.data();
  bool parse_key = true;
  for (const char* ptr = args.data(); ptr <= end; ++ptr) {
    if (ptr == end || *ptr == '&') {
      if (!parse_key) {
        const char* value_begin = key_end + 1;
        const char* value_end = ptr;
        if (key_begin < key_end && value_begin <= value_end) {
          std::vector<std::string>& arg_values = result[UrlDecode(
              std::string_view(key_begin, key_end - key_begin))];
          arg_values.emplace_back(UrlDecode(
              std::string_view(value_begin, value_end - value_begin)));
        }
      }
      parse_key = true;
      key_begin = ptr + 1;
      continue;
    }
    if (*ptr == '=' && parse_key) {
      parse_key = false;
      key_end = ptr;
      continue;
    }
  }
}

std::string UrlDecode(std::string_view url) {
  const auto* data = url.data();
  const auto* data_end = url.data() + url.size();
  // Fast path: no %, just id
  if (!memchr(data, '%', data_end - data) &&
      !memchr(data, '+', data_end - data)) {
    return std::string(data, data_end);
  }

  std::string res;
  for (const char* ptr = data; ptr < data_end; ++ptr) {
    if (*ptr == '%') {
      if (ptr + 2 < data_end &&
          utils::encoding::FromHex({ptr + 1, 2}, res) == 2) {
        ptr += 2;
      } else {
        static constexpr std::size_t kMaxOutputLength = 100;
        std::string data_short(data, data_end - data);
        if (data_short.size() > kMaxOutputLength) {
          data_short = data_short.substr(0, kMaxOutputLength);
          data_short += "<...>";
        }
        const auto percent_encoded_len =
            std::min(static_cast<std::size_t>(data_end - ptr), std::size_t{3});

        throw std::runtime_error("invalid percent-encoding sequence '" +
                                 std::string(ptr, percent_encoded_len) +
                                 "\' in input '" + std::move(data_short) +
                                 '\'');
      }
    } else if (*ptr == '+') {
      res += ' ';
    } else {
      res += *ptr;
    }
  }
  return res;
}

}  // namespace server::http::parser

// cookie_parser.hpp
#ifndef COOKIE_PARSER_HPP
#define COOKIE_PARSER_HPP

#include <string>
#include <unordered_map>
#include "get_parser.hpp"   // 提供 url_decode

inline std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.length() && s[start] == ' ') ++start;
    size_t end = s.length();
    while (end > start && s[end - 1] == ' ') --end;
    return s.substr(start, end - start);
}

inline std::unordered_map<std::string, std::string> parse_cookies(const std::string& cookie_header) {
    std::unordered_map<std::string, std::string> result;
    if (cookie_header.empty()) return result;

    size_t pos = 0;
    while (pos < cookie_header.length()) {
        size_t eq = cookie_header.find('=', pos);
        size_t semi = cookie_header.find(';', pos);
        if (semi == std::string::npos) semi = cookie_header.length();

        if (eq != std::string::npos && eq < semi) {
            std::string key = trim(cookie_header.substr(pos, eq - pos));
            std::string val = trim(cookie_header.substr(eq + 1, semi - eq - 1));
            result[key] = url_decode(val);
        }
        pos = semi + 1;
    }
    return result;
}

#endif
// get_parser.hpp
#ifndef GET_PARSER_HPP
#define GET_PARSER_HPP

#include <string>
#include <unordered_map>
#include <cstdlib>

// URL解码
inline std::string url_decode(const std::string& src) {
    std::string result;
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length()) {
            int c = 0;
            sscanf(src.substr(i + 1, 2).c_str(), "%x", &c);
            result += static_cast<char>(c);
            i += 2;
        } else if (src[i] == '+') {
            result += ' ';
        } else {
            result += src[i];
        }
    }
    return result;
}

// 解析查询字符串
inline std::unordered_map<std::string, std::string> parse_query_string(const std::string& qs) {
    std::unordered_map<std::string, std::string> result;
    if (qs.empty()) return result;

    size_t pos = 0;
    while (pos < qs.length()) {
        size_t eq = qs.find('=', pos);
        size_t amp = qs.find('&', pos);
        if (amp == std::string::npos) amp = qs.length();

        if (eq != std::string::npos && eq < amp) {
            std::string key = url_decode(qs.substr(pos, eq - pos));
            std::string val = url_decode(qs.substr(eq + 1, amp - eq - 1));
            result[key] = val;
        }
        pos = amp + 1;
    }
    return result;
}

#endif
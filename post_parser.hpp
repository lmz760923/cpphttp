// post_parser.hpp
#ifndef POST_PARSER_HPP
#define POST_PARSER_HPP

#include <string>
#include <unordered_map>
#include "get_parser.hpp"   // 复用 parse_query_string

// application/x-www-form-urlencoded 格式与GET相同
inline std::unordered_map<std::string, std::string> parse_post_form(const std::string& body) {
    return parse_query_string(body);
}

#endif
// http_parser.hpp
#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <unordered_map>
#include <sstream>
#include "request.hpp"
#include "get_parser.hpp"
#include "post_parser.hpp"
#include "cookie_parser.hpp"
#include "file_upload_parser.hpp"

class HttpParser {
public:
    static bool parse_request(const std::string& raw, Request& req) {
        // 1. 分离请求行、头部、请求体
        size_t headers_end = raw.find("\r\n\r\n");
        if (headers_end == std::string::npos) {
            headers_end = raw.find("\n\n");
            if (headers_end == std::string::npos) return false;
        }
        
        std::string headers_part = raw.substr(0, headers_end);
        req.body = raw.substr(headers_end + 4);
        
        // 2. 解析请求行
        size_t first_nl = headers_part.find('\n');
        std::string request_line = headers_part.substr(0, first_nl);
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }
        
        std::istringstream iss(request_line);
        iss >> req.method >> req.uri >> req.version;
        
        // 3. 解析URI，分离路径和查询字符串
        size_t qm = req.uri.find('?');
        if (qm != std::string::npos) {
            req.path = req.uri.substr(0, qm);
            req.query_string = req.uri.substr(qm + 1);
        } else {
            req.path = req.uri;
            req.query_string = "";
        }
        
        // 4. 解析GET参数
        req.GET = parse_query_string(req.query_string);
        
        // 5. 解析请求头
        std::string headers_str = headers_part.substr(first_nl + 1);
        size_t h_pos = 0;
        while (h_pos < headers_str.length()) {
            size_t h_end = headers_str.find('\n', h_pos);
            std::string line = headers_str.substr(h_pos, h_end - h_pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                // 去除开头空格
                while (!val.empty() && val[0] == ' ') val.erase(0, 1);
                req.headers[key] = val;
            }
            
            if (h_end == std::string::npos) break;
            h_pos = h_end + 1;
        }
        
        // 6. 解析Cookie
        req.COOKIE = parse_cookies(req.get_header("Cookie"));
        
        // 7. 解析POST请求体
        std::string content_type = req.get_header("Content-Type");
        if (req.method == "POST" && !req.body.empty()) {
            if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
                req.POST = parse_post_form(req.body);
            } else if (content_type.find("multipart/form-data") != std::string::npos) {
                // 提取boundary
                size_t bd_pos = content_type.find("boundary=");
                if (bd_pos != std::string::npos) {
                    std::string boundary = content_type.substr(bd_pos + 9);
                    auto result = parse_multipart(req.body, boundary);
                    if (!result.success) {
                        // 解析失败，记录错误
                        req.POST["__upload_error__"] = result.error;
                    } else {
                        req.POST = result.fields;
                        req.FILES = result.files;
                    }
                } else {
                    req.POST["__upload_error__"] = "未找到 multipart boundary";
                }
            }
        }
        
        return true;
    }
};

#endif  // HTTP_PARSER_HPP
// file_upload_parser.hpp（修正版片段）
#ifndef FILE_UPLOAD_PARSER_HPP
#define FILE_UPLOAD_PARSER_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include "request.hpp"

// 安全文件名过滤
inline std::string sanitize_filename(const std::string& filename) {
    std::string safe;
    for (char c : filename) {
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            safe += c;
        } else {
            safe += '_';
        }
    }
    // 避免空文件名
    if (safe.empty()) safe = "uploaded_file";
    return safe;
}

// 确保目录存在
inline bool ensure_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path.c_str(), 0755) == 0;
}

struct MultipartResult {
    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, Request::FileInfo> files;
    bool success = true;
    std::string error;
};

inline MultipartResult parse_multipart(const std::string& body, const std::string& boundary) {
    MultipartResult result;
    std::string full_boundary = "--" + boundary;
    std::string end_boundary = "--" + boundary + "--";
    // 调试输出 boundary 和 body 前200字节
    std::cerr << "[MULTIPART_DEBUG] boundary=" << boundary << std::endl;
    std::cerr << "[MULTIPART_DEBUG] body preview: " << body.substr(0, 200) << std::endl;
    // 确保上传目录存在
    std::string upload_dir = "./www/uploads";
    if (!ensure_directory(upload_dir)) {
        result.success = false;
        result.error = "Cannot create upload directory: " + upload_dir;
        return result;
    }

    size_t pos = 0;
    while (pos < body.length()) {
        // 查找下一个边界起始位置
        size_t bound_pos = body.find(full_boundary, pos);
        if (bound_pos == std::string::npos) break;

        // 边界后紧跟 \r\n 或 \n
        size_t part_start = bound_pos + full_boundary.length();
        if (part_start >= body.length()) break;

        // 检查是否是结束边界（后面紧跟 "--"）
        if (body.compare(part_start, 2, "--") == 0) {
            break;  // 结束边界，停止解析
        }

        // 跳过边界行末尾的换行符
        if (body[part_start] == '\r' && body[part_start + 1] == '\n')
            part_start += 2;
        else if (body[part_start] == '\n')
            part_start += 1;

        // 查找下一个边界的位置（作为当前 part 的结束标志）
        size_t next_bound = body.find(full_boundary, part_start);
        if (next_bound == std::string::npos) break;

        // 当前 part 的数据结束位置：下一个边界前的 \r\n
        size_t part_end = next_bound;
        if (part_end >= 2 && body[part_end - 2] == '\r' && body[part_end - 1] == '\n')
            part_end -= 2;
        else if (part_end >= 1 && body[part_end - 1] == '\n')
            part_end -= 1;

        std::string part_data = body.substr(part_start, part_end - part_start);

        // 分离头部与内容：查找第一个 "\r\n\r\n"
        size_t header_end = part_data.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            pos = next_bound;
            continue;
        }
        std::string headers = part_data.substr(0, header_end);
        std::string content = part_data.substr(header_end + 4);

        // 解析 Content-Disposition 获取 name 和 filename
        std::string name, filename;
        size_t cd_pos = headers.find("Content-Disposition:");
        if (cd_pos != std::string::npos) {
            size_t name_pos = headers.find("name=\"", cd_pos);
            if (name_pos != std::string::npos) {
                name_pos += 6;
                size_t name_end = headers.find('"', name_pos);
                if (name_end != std::string::npos)
                    name = headers.substr(name_pos, name_end - name_pos);
            }

            size_t fn_pos = headers.find("filename=\"", cd_pos);
            if (fn_pos != std::string::npos) {
                fn_pos += 10;
                size_t fn_end = headers.find('"', fn_pos);
                if (fn_end != std::string::npos)
                    filename = headers.substr(fn_pos, fn_end - fn_pos);
            }
        }

        // 解析 Content-Type
        std::string content_type;
        size_t ct_pos = headers.find("Content-Type:");
        if (ct_pos != std::string::npos) {
            ct_pos += 13;
            while (ct_pos < headers.length() && headers[ct_pos] == ' ') ct_pos++;
            size_t ct_end = headers.find('\r', ct_pos);
            if (ct_end == std::string::npos) ct_end = headers.length();
            content_type = headers.substr(ct_pos, ct_end - ct_pos);
        }

        if (!filename.empty()) {
            // 安全处理文件名
            std::string safe_filename = sanitize_filename(filename);
            std::string unique_name = std::to_string(time(NULL)) + "_" + safe_filename;
            std::string file_path = upload_dir + "/" + unique_name;

            std::ofstream ofs(file_path, std::ios::binary);
            if (ofs.is_open()) {
                ofs.write(content.c_str(), content.length());
                ofs.close();
                Request::FileInfo finfo;
                finfo.name = filename;
                finfo.type = content_type.empty() ? "application/octet-stream" : content_type;
                finfo.tmp_name = file_path;
                finfo.error = "0";
                finfo.size = content.length();
                result.files[name] = finfo;
            } else {
                Request::FileInfo finfo;
                finfo.name = filename;
                finfo.type = content_type;
                finfo.tmp_name = "";
                finfo.error = "1";
                finfo.size = 0;
                result.files[name] = finfo;
            }
        } else if (!name.empty()) {
            result.fields[name] = content;
        }

        pos = next_bound;
    }
    return result;
}

#endif
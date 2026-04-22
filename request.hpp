// request.hpp
#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>

class Request {
public:
	std::string session_id;
    // ==================== 超全局数组 ====================
    
    // $_SERVER - 服务器和执行环境信息
    std::unordered_map<std::string, std::string> SERVER;
    
    // $_GET - URL查询参数
    std::unordered_map<std::string, std::string> GET;
    
    // $_POST - POST表单数据
    std::unordered_map<std::string, std::string> POST;
    
    // $_COOKIE - Cookie数据
    std::unordered_map<std::string, std::string> COOKIE;
    
    // $_FILES - 上传文件信息
    struct FileInfo {
        std::string name;        // 原始文件名
        std::string type;        // MIME类型
        std::string tmp_name;    // 临时文件路径
        std::string error;       // 错误码
        size_t size;             // 文件大小
    };
    std::unordered_map<std::string, FileInfo> FILES;
    
    // ==================== 请求元数据 ====================
    
    std::string method;          // GET, POST, etc.
    std::string uri;             // 完整URI
    std::string path;            // 路径部分
    std::string query_string;    // 查询字符串
    std::string body;            // 请求体原始数据
    std::string version;         // HTTP版本
    std::unordered_map<std::string, std::string> headers;  // 原始请求头
    std::unordered_map<std::string, std::string> COOKIE_RAW; // 原始Cookie
    
    // ==================== 方法 ====================
    
    // 初始化SERVER数组
    void init_server_vars(const std::string& client_ip, int client_port,
                          const std::string& server_ip, int server_port,
                          const std::string& script_filename) {
        SERVER["REMOTE_ADDR"] = client_ip;
        SERVER["REMOTE_PORT"] = std::to_string(client_port);
        SERVER["SERVER_ADDR"] = server_ip;
        SERVER["SERVER_PORT"] = std::to_string(server_port);
        SERVER["REQUEST_METHOD"] = method;
        SERVER["REQUEST_URI"] = uri;
        SERVER["QUERY_STRING"] = query_string;
        SERVER["SCRIPT_NAME"] = path;
        SERVER["SCRIPT_FILENAME"] = script_filename;
        SERVER["DOCUMENT_ROOT"] = get_document_root();
        SERVER["HTTP_HOST"] = get_header("Host");
        SERVER["HTTP_USER_AGENT"] = get_header("User-Agent");
        SERVER["HTTP_ACCEPT"] = get_header("Accept");
        SERVER["HTTP_ACCEPT_LANGUAGE"] = get_header("Accept-Language");
        SERVER["HTTP_ACCEPT_ENCODING"] = get_header("Accept-Encoding");
        SERVER["HTTP_CONNECTION"] = get_header("Connection");
        SERVER["SERVER_SOFTWARE"] = "CPPServ/1.0 (类PHP C++ Web服务器)";
        SERVER["GATEWAY_INTERFACE"] = "CGI/1.1";
        SERVER["SERVER_PROTOCOL"] = version;
        
        char buf[64];
        time_t now = time(NULL);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        SERVER["REQUEST_TIME"] = std::to_string(now);
        SERVER["REQUEST_TIME_FMT"] = buf;
    }
    
    // 辅助方法：获取请求头
    std::string get_header(const std::string& name) const {
        auto it = headers.find(name);
        if (it != headers.end()) return it->second;
        // 尝试大小写不敏感查找
        for (const auto& h : headers) {
            if (strcasecmp(h.first.c_str(), name.c_str()) == 0) {
                return h.second;
            }
        }
        return "";
    }
    
    // 获取文档根目录
    std::string get_document_root() const {
        const char* pwd = getenv("PWD");
        return pwd ? std::string(pwd) + "/www" : "./www";
    }

private:
    // 请求头查找（大小写不敏感）
    struct CaseInsensitiveCompare {
        bool operator()(const std::string& a, const std::string& b) const {
            return strcasecmp(a.c_str(), b.c_str()) < 0;
        }
    };
};
#endif
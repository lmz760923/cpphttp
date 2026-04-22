// session.hpp
#ifndef SESSION_HPP
#define SESSION_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

class SessionManager {
private:
    std::string session_dir;
    int session_expire;  // 过期时间（秒）
    
    // 生成随机Session ID（32位十六进制字符串，类似PHP）
    std::string generate_session_id() {
        static const char hex_chars[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::string id;
        for (int i = 0; i < 32; ++i) {
            id += hex_chars[dis(gen)];
        }
        return id;
    }
    
    // 获取Session文件路径
    std::string get_session_file(const std::string& session_id) const {
        return session_dir + "/sess_" + session_id;
    }
    
public:
    SessionManager(const std::string& dir = "/tmp/php_sessions", int expire = 1440) 
        : session_dir(dir), session_expire(expire) {
        // 创建Session目录
        if (!fs::exists(session_dir)) {
            fs::create_directories(session_dir);
        }
    }
    
    // 读取Session数据
    std::unordered_map<std::string, std::string> load(const std::string& session_id) {
        std::unordered_map<std::string, std::string> data;
        std::string file_path = get_session_file(session_id);
        
        std::ifstream ifs(file_path);
        if (!ifs.is_open()) return data;
        
        // 检查是否过期
        auto ftime = fs::last_write_time(file_path);
        auto now = fs::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - ftime).count();
        if (age > session_expire) {
            fs::remove(file_path);
            return data;
        }
        
        // 解析Session文件（格式：key|s:value;）
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        
        size_t pos = 0;
        while (pos < content.length()) {
            size_t bar = content.find('|', pos);
            if (bar == std::string::npos) break;
            
            std::string key = content.substr(pos, bar - pos);
            pos = bar + 1;
            
            // 解析PHP风格序列化格式
            if (pos >= content.length()) break;
            char type = content[pos];
            pos++;
            
            if (type == 's') {
                size_t colon = content.find(':', pos);
                if (colon == std::string::npos) break;
                int len = std::stoi(content.substr(pos, colon - pos));
                pos = colon + 1;
                
                size_t quote = content.find('"', pos);
                if (quote == std::string::npos) break;
                pos = quote + 1;
                
                std::string value = content.substr(pos, len);
                data[key] = value;
                pos += len + 2;  // 跳过引号和分号
            } else if (type == 'i') {
                size_t colon = content.find(':', pos);
                size_t semi = content.find(';', pos);
                if (colon == std::string::npos || semi == std::string::npos) break;
                std::string value = content.substr(colon + 1, semi - colon - 1);
                data[key] = value;
                pos = semi + 1;
            }
        }
        
        return data;
    }
    
    // 保存Session数据
    bool save(const std::string& session_id, 
              const std::unordered_map<std::string, std::string>& data) {
        std::string file_path = get_session_file(session_id);
        std::ofstream ofs(file_path, std::ios::trunc);
        if (!ofs.is_open()) return false;
        
        for (const auto& kv : data) {
            // 格式：key|s:length:"value";
            ofs << kv.first << "|s:" << kv.second.length() 
                << ":\"" << kv.second << "\";";
        }
        
        return true;
    }
    
    // 创建新Session
    std::string create() {
        std::string sid = generate_session_id();
        std::string file_path = get_session_file(sid);
        std::ofstream ofs(file_path);
        ofs.close();
        return sid;
    }
    
    // 销毁Session
    bool destroy(const std::string& session_id) {
        std::string file_path = get_session_file(session_id);
        return fs::remove(file_path);
    }
    
    // 清理过期Session
    void gc() {
        auto now = fs::file_time_type::clock::now();
        for (const auto& entry : fs::directory_iterator(session_dir)) {
            if (entry.path().filename().string().find("sess_") == 0) {
                auto ftime = entry.last_write_time();
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - ftime).count();
                if (age > session_expire) {
                    fs::remove(entry.path());
                }
            }
        }
    }
};

#endif
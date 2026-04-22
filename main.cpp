// main.cpp
#include <filesystem>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <ctime>

#include "request.hpp"
#include "http_parser.hpp"
#include "session.hpp"
#include "router.hpp"

namespace fs=std::filesystem;

const int PORT = 80;
const int BUFFER_SIZE = 4096; // 调小缓冲区便于调试

SessionManager session_mgr("/tmp/php_sessions", 1440);
Router router;


// 响应构建函数（支持额外头部）
std::string build_response(Request& req, const std::string& body,
                           const std::string& content_type = "text/html",
                           bool session_changed = false,
                           const std::unordered_map<std::string, std::string>& extra_headers = {}) {
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    // 仅 text 类型加 charset，图片/二进制类型不加
    if (content_type.find("text/") == 0) {
        resp << "Content-Type: " << content_type << "; charset=utf-8\r\n";
    } else {
        resp << "Content-Type: " << content_type << "\r\n";
    }
    resp << "Content-Length: " << body.length() << "\r\n";

    if (session_changed && !req.session_id.empty()) {
        resp << "Set-Cookie: PHPSESSID=" << req.session_id << "; Path=/\r\n";
    }

    // 添加额外头部（如 Content-Disposition）
    for (const auto& h : extra_headers) {
        resp << h.first << ": " << h.second << "\r\n";
    }

    resp << "Connection: close\r\n";
    resp << "Server: CPPServ/1.0\r\n";
    resp << "\r\n";   // 头部结束空行
    resp << body;
    return resp.str();
}
// ---------- 路由处理函数（新签名）----------
std::string handle_index(Request& req, const std::vector<std::string>& params) {
    auto SESSION = session_mgr.load(req.session_id);
    
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>CPPServ</title></head><body>";
    html << "<h1>Welcome to CPPServ (类PHP C++ Web服务器)</h1>";
    
    html << "<h2>Session 数据:</h2><pre>";
    for (const auto& kv : SESSION) {
        html << kv.first << " = " << kv.second << "\n";
    }
    html << "</pre>";
    
    html << "<h2>$_GET:</h2><pre>";
    for (const auto& kv : req.GET) {
        html << kv.first << " = " << kv.second << "\n";
    }
    html << "</pre>";
    
    html << "<h2>$_POST:</h2><pre>";
    for (const auto& kv : req.POST) {
        html << kv.first << " = " << kv.second << "\n";
    }
    html << "</pre>";
    
    html << "<h2>$_COOKIE:</h2><pre>";
    for (const auto& kv : req.COOKIE) {
        html << kv.first << " = " << kv.second << "\n";
    }
    html << "</pre>";
    
    html << R"(
        <h2>POST表单测试:</h2>
        <form method="POST" action="/info">
            <input type="text" name="username" placeholder="用户名">
            <input type="password" name="password" placeholder="密码">
            <button type="submit">提交</button>
        </form>
        
        <h2>文件上传测试:</h2>
        <form method="POST" action="/upload" enctype="multipart/form-data">
            <input type="file" name="file">
            <button type="submit">上传</button>
        </form>
        
        <h2>正则路由测试:</h2>
        <p><a href="/user/123">/user/123</a> (显示用户ID 123)</p >
        <p><a href="/user/456">/user/456</a> (显示用户ID 456)</p >
    )";
    
    html << "</body></html>";
    return html.str();
}

std::string handle_login(Request& req, const std::vector<std::string>& params) {
    auto SESSION = session_mgr.load(req.session_id);
    
    if (req.method == "POST") {
        std::string username = req.POST["username"];
        std::string password = req.POST["password"];
        
        if (username == "admin" && password == "123456") {
            SESSION["user"] = username;
            SESSION["login_time"] = std::to_string(time(NULL));
            session_mgr.save(req.session_id, SESSION);
            return "<h1>登录成功！</h1><a href='/'>返回首页</a >";
        }
        return "<h1>登录失败！</h1><a href='/'>返回首页</a >";
    }
    
    return R"(
        <h1>登录</h1>
        <form method='POST'>
            <input type='text' name='username' placeholder='用户名'><br>
            <input type='password' name='password' placeholder='密码'><br>
            <button type='submit'>登录</button>
        </form>
    )";
}

std::string handle_phpinfo(Request& req, const std::vector<std::string>& params) {
    auto SESSION = session_mgr.load(req.session_id);
    
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>PHPInfo</title>";
    html << "<style>table{border-collapse:collapse;width:100%}";
    html << "th{background:#999;color:#fff}";
    html << "td,th{border:1px solid #ccc;padding:5px}</style>";
    html << "</head><body>";
    html << "<h1>CPPServ 信息 (类似 phpinfo)</h1>";
    
    // $_SERVER
    html << "<h2>$_SERVER</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : req.SERVER) {
        html << "<tr><td>" << kv.first << "</td><td>" << kv.second << "</td></tr>";
    }
    html << "</table>";
    
    // $_GET
    html << "<h2>$_GET</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : req.GET) {
        html << "<tr><td>" << kv.first << "</td><td>" << kv.second << "</td></tr>";
    }
    html << "</table>";
    
    // $_POST
    html << "<h2>$_POST</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : req.POST) {
        html << "<tr><td>" << kv.first << "</td><td>" << kv.second << "</td></tr>";
    }
    html << "</table>";
    
    // $_COOKIE
    html << "<h2>$_COOKIE</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : req.COOKIE) {
        html << "<tr><td>" << kv.first << "</td><td>" << kv.second << "</td></tr>";
    }
    html << "</table>";
    
    // $_FILES
    html << "<h2>$_FILES</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : req.FILES) {
        html << "<tr><td>" << kv.first << ".name</td><td>" << kv.second.name << "</td></tr>";
        html << "<tr><td>" << kv.first << ".type</td><td>" << kv.second.type << "</td></tr>";
        html << "<tr><td>" << kv.first << ".tmp_name</td><td>" << kv.second.tmp_name << "</td></tr>";
        html << "<tr><td>" << kv.first << ".size</td><td>" << kv.second.size << "</td></tr>";
    }
    html << "</table>";
    
    // $_SESSION
    html << "<h2>$_SESSION</h2><table><tr><th>变量</th><th>值</th></tr>";
    for (const auto& kv : SESSION) {
        html << "<tr><td>" << kv.first << "</td><td>" << kv.second << "</td></tr>";
    }
    html << "</table>";
    
    html << "<p><a href='/'>返回首页</a ></p >";
    html << "</body></html>";
    return html.str();
}

std::string handle_upload(Request& req, const std::vector<std::string>& params) {
    auto SESSION = session_mgr.load(req.session_id);
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>文件上传</title></head><body>";

    // 调试：输出 body 长度和 content-length
    size_t content_length = 0;
    if (req.headers.count("Content-Length")) {
        try {
            content_length = std::stoul(req.headers.at("Content-Length"));
        } catch (...) {}
    }
    std::cerr << "[UPLOAD_DEBUG] req.body.length=" << req.body.length() << ", Content-Length=" << content_length << std::endl;
    // 上传成功
    if (req.method == "POST" && !req.FILES.empty()) {
        html << "<h1>文件上传成功！</h1>";
        std::string host;
        auto it = req.headers.find("Host");
        if (it != req.headers.end()) {
            host = it->second;
        } else {
            host = "127.0.0.1"; // 可根据实际情况修改为服务器公网IP
        }
        for (const auto& kv : req.FILES) {
            html << "<p>字段名: " << kv.first << "</p >";
            html << "<p>原始文件名: " << kv.second.name << "</p >";
            html << "<p>MIME类型: " << kv.second.type << "</p >";
            html << "<p>保存路径: " << kv.second.tmp_name << "</p >";
            html << "<p>文件大小: " << kv.second.size << " 字节</p >";
            std::string web_path = "/uploads/" + kv.second.tmp_name.substr(kv.second.tmp_name.find_last_of('/') + 1);
            std::string abs_url = "http://" + host + web_path;
            html << "<p><a href='" << abs_url << "' target='_blank'>查看/下载文件</a ></p >";
            html << "<hr>";
        }
    } else if (req.method == "POST") {
        // POST但FILES为空，显示详细错误并记录日志
        html << "<h1 style='color:red'>文件上传失败！</h1>";
        std::string ctype = req.headers.count("Content-Type") ? req.headers.at("Content-Type") : "(无)";
        html << "<p>Content-Type: " << ctype << "</p>";
        html << "<p>body length: " << req.body.length() << "</p>";
        html << "<pre>body preview: " << req.body.substr(0, 200) << "</pre>";
        // 记录到服务器日志
        std::cerr << "[UPLOAD_FAIL] Content-Type: " << ctype << ", body length: " << req.body.length() << std::endl;
        std::cerr << "[UPLOAD_FAIL] body preview: " << req.body.substr(0, 200) << std::endl;
        // boundary调试
        size_t bpos = ctype.find("boundary=");
        if (bpos != std::string::npos) {
            std::string boundary = ctype.substr(bpos + 9);
            std::cerr << "[UPLOAD_FAIL] boundary: " << boundary << std::endl;
        }
        html << "<p>请检查上传文件大小、格式、目录权限或服务器端解析错误。</p>";
    } else {
        html << R"(
            <h1>上传文件</h1>
            <form method='POST' enctype='multipart/form-data'>
                <input type='file' name='file'><br><br>
                <button type='submit'>上传</button>
            </form>
        )";
    }

    html << "<p><a href='/'>返回首页</a ></p >";
    html << "</body></html>";
    return html.str();
}

// 新增：正则路由示例
std::string handle_user_profile(Request& req, const std::vector<std::string>& params) {
    std::string user_id = params.empty() ? "unknown" : params[0];
    
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>User Profile</title></head><body>";
    html << "<h1>User Profile</h1>";
    html << "<p>User ID: " << user_id << "</p >";
    html << "<p>Request Method: " << req.method << "</p >";
    html << "<p><a href='/'>返回首页</a ></p >";
    html << "</body></html>";
    return html.str();
}

// 静态文件处理器
std::string handle_static(Request& req, const std::vector<std::string>& params) {
    std::string file_path = "./www" + req.path;
    // 安全检查：防止目录遍历攻击
    if (file_path.find("..") != std::string::npos) {
        return "<h1>403 Forbidden</h1>";
    }
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "<h1>404 File Not Found</h1>";
    }
    // 读取文件内容（确保只返回文件内容，无调试/SQL等混入）
    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();
    // 判断 MIME 类型
    std::string mime = "application/octet-stream";
    if (req.path.find(".html") != std::string::npos || req.path.find(".htm") != std::string::npos)
        mime = "text/html";
    else if (req.path.find(".css") != std::string::npos)
        mime = "text/css";
    else if (req.path.find(".js") != std::string::npos)
        mime = "application/javascript";
    else if (req.path.find(".jpg") != std::string::npos || req.path.find(".jpeg") != std::string::npos)
        mime = "image/jpeg";
    else if (req.path.find(".png") != std::string::npos)
        mime = "image/png";
    else if (req.path.find(".gif") != std::string::npos)
        mime = "image/gif";
    else if (req.path.find(".txt") != std::string::npos)
        mime = "text/plain";
    else if (req.path.find(".pdf") != std::string::npos)
        mime = "application/pdf";
    // 构建额外头部（强制下载）
    std::unordered_map<std::string, std::string> extra_headers;
    bool force_download = (req.GET.find("download") != req.GET.end());  // ?download=1 参数
    // 所有图片、octet-stream、pdf都强制下载，并将 Content-Type 设为 application/octet-stream
    if (force_download ||
        mime == "application/octet-stream" ||
        mime == "application/pdf" ||
        mime == "image/jpeg" || mime == "image/png" || mime == "image/gif") {
        std::string filename = req.path.substr(req.path.find_last_of('/') + 1);
        if (filename.empty()) filename = "download";
        extra_headers["Content-Disposition"] = "attachment; filename=\"" + filename + "\"";
        mime = "application/octet-stream";
    }
    // 只返回文件内容，不拼接任何调试/SQL/其他内容
    return build_response(req, body, mime, false, extra_headers);
}

// 设置非阻塞socket
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool read_all(int fd, std::string& buffer, size_t expected_length) {
    buffer.clear();
    buffer.reserve(expected_length);
    char tmp[4096];
    size_t total_read = 0;
    while (total_read < expected_length) {
        ssize_t n = recv(fd, tmp, std::min(sizeof(tmp), expected_length - total_read), 0);
        if (n <= 0) {
            return false;
        }
        buffer.append(tmp, n);
        total_read += n;
    }
    return true;
}

int main() {
    // 注册路由
    router.add("/", handle_index);
    router.add("/index", handle_index);
    router.add("/login", handle_login);
    router.add("/info", handle_phpinfo);
    router.add("/upload", handle_upload);
    router.add("/user/(\\d+)", handle_user_profile);
	router.add("/uploads/(.+)",handle_static);
    
    // 创建socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }
    
    set_nonblocking(server_fd);
    std::cout << "CPPServ (类PHP C++ Web服务器) 启动在端口 " << PORT << std::endl;
    
    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    int max_fd = server_fd;
    
    std::unordered_map<int, std::string> client_buffers;
    std::unordered_map<int, time_t> client_last_active;
    
    while (true) {
        std::cerr << "[LOOP] 主循环进入" << std::endl;
        // 超时检查：每轮遍历所有 client_last_active，超时则关闭
        time_t now = time(NULL);
        int timeout_sec = 60; // 60秒超时，适应大文件上传
        for (auto it = client_last_active.begin(); it != client_last_active.end(); ) {
            int fd = it->first;
            time_t last = it->second;
            if (now - last > timeout_sec) {
                std::cerr << "[TIMEOUT] fd=" << fd << " 超过 " << timeout_sec << " 秒未收全数据，自动关闭" << std::endl;
                close(fd);
                FD_CLR(fd, &master_fds);
                client_buffers.erase(fd);
                it = client_last_active.erase(it);
            } else {
                ++it;
            }
        }
        // 打印所有活跃 fd
        std::cerr << "[DEBUG] 活跃fd: ";
        for (const auto& kv : client_last_active) std::cerr << kv.first << " ";
        std::cerr << std::endl;
        read_fds = master_fds;
        struct timeval tv;
        tv.tv_sec = 5; tv.tv_usec = 0; // select 最多等5秒，便于定期超时检查
        int sel = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (sel < 0) {
            perror("select");
            continue;
        }
        for (int fd = 0; fd <= max_fd; ++fd) {
            if (!FD_ISSET(fd, &read_fds)) continue;
            if (fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd >= 0) {
                    set_nonblocking(client_fd);
                    FD_SET(client_fd, &master_fds);
                    if (client_fd > max_fd) max_fd = client_fd;
                    if (client_buffers.count(client_fd) || client_last_active.count(client_fd)) {
                        std::cerr << "[ERROR] fd 冲突: " << client_fd << std::endl;
                    }
                    client_buffers[client_fd] = "";
                    client_last_active[client_fd] = time(NULL);
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                    std::cout << "新连接: " << client_ip << ":" << ntohs(client_addr.sin_port) << ", fd=" << client_fd << std::endl;
                }
            } else {
                // 处理所有活跃 fd
                bool got_data = false;
                while (true) {
                    char buffer[BUFFER_SIZE];
                    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        client_buffers[fd].append(buffer, bytes_read); // 二进制安全拼接
                        client_last_active[fd] = time(NULL);
                        got_data = true;
                        continue; // 继续读取直到EAGAIN
                    } else if (bytes_read == 0) {
                        std::cerr << "[WARN] fd=" << fd << " 读取失败或连接关闭, bytes_read=0" << std::endl;
                        close(fd);
                        FD_CLR(fd, &master_fds);
                        client_buffers.erase(fd);
                        client_last_active.erase(fd);
                        got_data = false;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 没有更多数据可读
                        } else {
                            std::cerr << "[WARN] fd=" << fd << " 读取失败, bytes_read=" << bytes_read << ", errno=" << errno << std::endl;
                            close(fd);
                            FD_CLR(fd, &master_fds);
                            client_buffers.erase(fd);
                            client_last_active.erase(fd);
                            got_data = false;
                            break;
                        }
                    }
                }
                if (!got_data) continue;
                size_t headers_end = client_buffers[fd].find("\r\n\r\n");
                size_t sep_len = 4;
                if (headers_end == std::string::npos) {
                    headers_end = client_buffers[fd].find("\n\n");
                    sep_len = 2;
                }
                if (headers_end != std::string::npos) {
                    // 解析 Content-Length
                    Request temp_req;
                    bool parse_ok = HttpParser::parse_request(client_buffers[fd], temp_req);
                    if (!parse_ok) {
                        std::cerr << "[ERROR] parse_request 头部解析失败, fd=" << fd << std::endl;
                        close(fd);
                        FD_CLR(fd, &master_fds);
                        client_buffers.erase(fd);
                        continue;
                    }
                    size_t content_length = 0;
                    std::string cl_str = temp_req.get_header("Content-Length");
                    if (!cl_str.empty()) {
                        try {
                            content_length = std::stoul(cl_str);
                        } catch (...) {
                            std::cerr << "[ERROR] Content-Length 非法: " << cl_str << std::endl;
                            close(fd);
                            FD_CLR(fd, &master_fds);
                            client_buffers.erase(fd);
                            continue;
                        }
                    }
                    size_t body_have = client_buffers[fd].size() - (headers_end + sep_len);
                    std::cerr << "[DEBUG] fd=" << fd << ", headers_end=" << headers_end << ", sep_len=" << sep_len << ", content_length=" << content_length << ", body_have=" << body_have << ", buffer_size=" << client_buffers[fd].size() << std::endl;
                    if (content_length > 0 && body_have < content_length) {
                        std::cerr << "[DEBUG] fd=" << fd << " 请求体未收全, 已有: " << body_have << ", 需: " << content_length << std::endl;
                        continue;
                    }
                    // 请求体已收全，处理请求
                    std::string headers_part = client_buffers[fd].substr(0, headers_end);
                    std::string full_body = client_buffers[fd].substr(headers_end + sep_len, content_length);
                    std::string full_request = headers_part + (sep_len == 4 ? "\r\n\r\n" : "\n\n") + full_body;
                    Request req;
                    if (!HttpParser::parse_request(full_request, req)) {
                        std::cerr << "[ERROR] parse_request 完整请求解析失败, fd=" << fd << std::endl;
                        close(fd);
                        FD_CLR(fd, &master_fds);
                        client_buffers.erase(fd);
                        continue;
                    }
                    // ---- 原有处理逻辑（初始化 SERVER、Session、路由等） ----
                    struct sockaddr_in peer, local;
                    socklen_t peer_len = sizeof(peer), local_len = sizeof(local);
                    getpeername(fd, (struct sockaddr*)&peer, &peer_len);
                    getsockname(fd, (struct sockaddr*)&local, &local_len);
                    char peer_ip[INET_ADDRSTRLEN], local_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
                    inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip));
                    std::string script_path = "./www" + req.path;
                    req.init_server_vars(peer_ip, ntohs(peer.sin_port), local_ip, ntohs(local.sin_port), script_path);
                    // Session 处理
                    bool session_changed = false;
                    std::string cookie_session_id = req.COOKIE["PHPSESSID"];
                    std::string session_id;
                    if (cookie_session_id.empty()) {
                        session_id = session_mgr.create();
                        session_changed = true;
                    } else {
                        std::string file_path = "/tmp/php_sessions/sess_" + cookie_session_id;
                        bool session_valid = false;
                        if (fs::exists(file_path)) {
                            auto ftime = fs::last_write_time(file_path);
                            auto now = fs::file_time_type::clock::now();
                            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - ftime).count();
                            if (age <= 1440) {
                                session_valid = true;
                            } else {
                                fs::remove(file_path);
                            }
                        }
                        if (session_valid) {
                            session_id = cookie_session_id;
                        } else {
                            session_id = session_mgr.create();
                            session_changed = true;
                        }
                    }
                    req.session_id = session_id;
                    // 路由分发
                    std::string body;
                    try {
                        body = router.dispatch(req);
                    } catch (const std::exception& ex) {
                        std::cerr << "[EXCEPTION] 路由处理异常: " << ex.what() << std::endl;
                        body = "<h1>500 Internal Server Error</h1><pre>";
                        body += ex.what();
                        body += "</pre>";
                    } catch (...) {
                        std::cerr << "[EXCEPTION] 路由处理未知异常" << std::endl;
                        body = "<h1>500 Internal Server Error</h1><pre>Unknown error</pre>";
                    }
                    // 如果 body 已经是完整 HTTP 响应（如 handle_static 返回），直接发送
                    std::string response;
                    if (body.rfind("HTTP/1.1 ", 0) == 0) {
                        response = body;
                    } else {
                        response = build_response(req, body, "text/html", session_changed);
                    }
                    std::cerr << "[RESPONSE_DEBUG] fd=" << fd << ", response.length=" << response.length() << std::endl;
                    ssize_t sent = send(fd, response.c_str(), response.length(), 0);
                    if (sent < 0) {
                        std::cerr << "[ERROR] send 响应失败, fd=" << fd << std::endl;
                    } else {
                        std::cerr << "[RESPONSE_DEBUG] send 成功, sent=" << sent << std::endl;
                    }
                    // 清理连接
                    close(fd);
                    FD_CLR(fd, &master_fds);
                    client_buffers.erase(fd);
                    client_last_active.erase(fd);
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
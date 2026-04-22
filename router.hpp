// router.hpp
#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <functional>
#include <vector>
#include <regex>
#include <string>
#include "request.hpp"

class Router {
public:
    using Handler = std::function<std::string(Request&, const std::vector<std::string>&)>;

    void add(const std::string& pattern, Handler handler) {
        routes.push_back({std::regex(pattern), handler});
    }

    std::string dispatch(Request& req) {
        for (const auto& route : routes) {
            std::smatch match;
            if (std::regex_match(req.path, match, route.pattern)) {
                std::vector<std::string> params;
                for (size_t i = 1; i < match.size(); ++i) {
                    params.push_back(match[i].str());
                }
                return route.handler(req, params);
            }
        }
        return "<h1>404 Not Found</h1><p>The requested URL was not found on this server.</p >";
    }

private:
    struct Route {
        std::regex pattern;
        Handler   handler;
    };
    std::vector<Route> routes;
};

#endif
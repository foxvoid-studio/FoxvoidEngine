#pragma once

#include <string>
#include <functional>
#include <unordered_map>

struct HttpResponse {
    int statusCode;
    std::string body;
};

class [[gnu::visibility("default")]] HttpClient {
    public:
        using SuccessCallback = std::function<void(const HttpResponse&)>;
        using ErrorCallback = std::function<void(const std::string&)>;

        // Performs an asynchronous HTTP GET request
        static void Get(
            const std::string& url,
            const std::unordered_map<std::string, std::string>& headers,
            SuccessCallback onSuccess,
            ErrorCallback onError
        );

        // Performs an asynchronous HTTP POST request
        static void Post(
            const std::string& url,
            const std::unordered_map<std::string, std::string>& headers,
            const std::string& body,
            SuccessCallback onSuccess,
            ErrorCallback onError
        );
};

#include "HttpClient.hpp"
#include <iostream>
#include <cstring>

#ifdef __EMSCRIPTEN__
    #include <emscripten/fetch.h>
#else
    #include <thread>
    #include <network/httplib.h>
#endif

#pragma region WebAssembly implementation

#ifdef __EMSCRIPTEN__

struct FetchContext {
    HttpClient::SuccessCallback onSuccess;
    HttpClient::ErrorCallback onError;
    std::string requestBody;
    std::vector<std::string> headerStrings;
    std::vector<const char*> customHeaders;
};

void OnFetchSuccess(emscripten_fetch_t *fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    HttpResponse response = {fetch->status, ""};
    if (fetch->numBytes > 0) {
        response.body = std::string(fetch->data, fetch->numBytes);
    }

    if (ctx->onSuccess) ctx->onSuccess(response);
    delete ctx;

    emscripten_fetch_close(fetch);
}

void OnFetchError(emscripten_fetch_t *fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    
    if (ctx->onError) ctx->onError("HTTP Request Failed. Status: " + std::to_string(fetch->status));
    delete ctx;

    emscripten_fetch_close(fetch);
}

void SetupWasmHeaders(emscripten_fetch_attr_t& attr, const std::unordered_map<std::string, std::string>& headers, FetchContext* ctx) {
    for (const auto& pair : headers) {
        ctx->headerStrings.push_back(pair.first);
        ctx->headerStrings.push_back(pair.second);
    }
    
    for (const auto& str : ctx->headerStrings) {
        ctx->customHeaders.push_back(str.c_str());
    }
    
    ctx->customHeaders.push_back(nullptr);
    attr.requestHeaders = ctx->customHeaders.data();
}

#endif

#pragma endregion

#pragma region Desktop / Mobile Helper

#ifndef __EMSCRIPTEN__

// Helper to split a full URL (e.g. "https://foxvoid.com/engine/...") into Base URL and Path
void ParseUrl(const std::string& fullUrl, std::string& baseUrl, std::string& path) {
    size_t protocolEnd = fullUrl.find("://");
    size_t pathStart = fullUrl.find("/", protocolEnd != std::string::npos ? protocolEnd + 3 : 0);

    if (pathStart != std::string::npos) {
        baseUrl = fullUrl.substr(0, pathStart);
        path = fullUrl.substr(pathStart);
    }
    else {
        baseUrl = fullUrl;
        path = "/";
    }
}

#endif

#pragma endregion

#pragma region API Implementations

void HttpClient::Get(const std::string& url, const std::unordered_map<std::string, std::string>& headers, SuccessCallback onSuccess, ErrorCallback onError) {
    #ifdef __EMSCRIPTEN__
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        
        FetchContext* ctx = new FetchContext{onSuccess, onError, ""};
        attr.userData = ctx;

        attr.onsuccess = OnFetchSuccess;
        attr.onerror = OnFetchError;

        SetupWasmHeaders(attr, headers, ctx);
        emscripten_fetch(&attr, url.c_str());
    #else
        // Desktop: Run HTTP request in a detached thread to prevent blocking the game loop
        std::thread([url, headers, onSuccess, onError]() {
            std::string baseUrl, path;
            ParseUrl(url, baseUrl, path);

            httplib::Client cli(baseUrl);
            httplib::Headers httplibHeaders;
            for (const auto& pair : headers) {
                httplibHeaders.insert({pair.first, pair.second});
            }

            if (auto res = cli.Get(path.c_str(), httplibHeaders)) {
                HttpResponse response = {res->status, res->body};
                if (onSuccess) onSuccess(response);
            } else {
                auto err = res.error();
                if (onError) onError("HTTP GET Failed with httplib error code: " + std::to_string(static_cast<int>(err)));
            }
        }).detach();
    #endif
}

void HttpClient::Post(const std::string& url, const std::unordered_map<std::string, std::string>& headers, const std::string& body, SuccessCallback onSuccess, ErrorCallback onError) {
    #ifdef __EMSCRIPTEN__
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "POST");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

        FetchContext* ctx = new FetchContext{onSuccess, onError, body};
        attr.userData = ctx;

        attr.onsuccess = OnFetchSuccess;
        attr.onerror = OnFetchError;

        attr.requestData = ctx->requestBody.c_str();
        attr.requestDataSize = ctx->requestBody.size();

        std::vector<const char*> customHeaders;
        SetupWasmHeaders(attr, headers, ctx);
        emscripten_fetch(&attr, url.c_str());
    #else
        // Desktop: Run HTTP request in a detached thread
        std::thread([url, headers, body, onSuccess, onError]() {
            std::string baseUrl, path;
            ParseUrl(url, baseUrl, path);

            httplib::Client cli(baseUrl);
            httplib::Headers httplibHeaders;
            for (const auto& pair : headers) {
                httplibHeaders.insert({pair.first, pair.second});
            }

            // Post expects the content type as a dedicated parameter in cpp-httplib
            std::string contentType = "application/json";
            auto it = headers.find("Content-Type");
            if (it != headers.end()) contentType = it->second;

            if (auto res = cli.Post(path.c_str(), httplibHeaders, body, contentType.c_str())) {
                HttpResponse response = {res->status, res->body};
                if (onSuccess) onSuccess(response);
            } else {
                auto err = res.error();
                if (onError) onError("HTTP POST Failed with httplib error code: " + std::to_string(static_cast<int>(err)));
            }
        }).detach();
    #endif
}

#pragma endregion

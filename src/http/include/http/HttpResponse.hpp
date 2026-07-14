#pragma once

#include "http/HttpHeaders.hpp"

enum class ConnectionPreference {
    KeepAlive,
    Close,
};

class HttpResponse {
private:
    int _statusCode{200};
    std::string _reasonPhrase{"OK"};
    HttpHeaders _headers;
    std::string _body;

    ConnectionPreference _connectionPreference{ConnectionPreference::KeepAlive};

public:
    HttpResponse() = default;

    void setStatus(int statusCode);

    void setStatus(int statusCode, std::string reasonPhrase);

    int statusCode() const noexcept;

    std::string_view reasonPhrase() const noexcept;

    void setHeader(std::string name, std::string value);

    bool removeHeader(const std::string& name);

    bool hasHeader(const std::string& name) const;

    std::optional<std::string> header(const std::string& name) const;

    const HttpHeaders& headers() const noexcept;

    void setBody(std::string body);

    void setBody(std::string body, std::string contentType);

    void clearBody() noexcept;

    std::string_view body() const noexcept;

    void setConnectionPreference(ConnectionPreference preference) noexcept;

    ConnectionPreference connectionPreference() const noexcept;

    bool shouldCloseConnection() const noexcept;

    static HttpResponse error(int statusCode, std::string body = {});

    static HttpResponse redirect(int statusCode, std::string target);
};

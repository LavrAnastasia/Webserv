#include "http/HttpSession.hpp"

#include <limits>
#include <utility>

#include "CgiStream.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "Router.hpp"
#include "UploadHandler.hpp"
#include "http/HttpMethod.hpp"
#include "http/RequestDispatcher.hpp"
#include "log/Log.hpp"

namespace {
    constexpr std::string_view kContinueResponse = "HTTP/1.1 100 Continue\r\n\r\n";
    constexpr std::size_t kChunkSize = 64 * 1024;
    constexpr std::chrono::seconds kLingerTimeout{5};
    constexpr std::chrono::seconds kLingerDuration{30};
    constexpr std::chrono::seconds kHeaderTimeout{20};
    constexpr std::chrono::seconds kBodyTimeout{60};
    constexpr std::chrono::seconds kKeepAliveTimeout{60};

    std::chrono::steady_clock::time_point now() {
        return std::chrono::steady_clock::now();
    }
} // namespace

HttpSession::HttpSession(const ServerConfig& server, ConnectionInfo info) : HttpSession(server, std::move(info), now) {
}

HttpSession::HttpSession(const ServerConfig& server, ConnectionInfo info, Clock clock)
    : server_(server), info_(std::move(info)), clock_(clock), parser_(std::numeric_limits<std::size_t>::max()),
      lastReceived_(clock_()) {
}

HttpSession::~HttpSession() = default;

HttpSession::Action HttpSession::receive(std::string_view bytes) {
    if (!bytes.empty()) {
        lastReceived_ = clock_();
    }

    if (phase_ == Phase::Lingering) {
        return drain(bytes);
    }

    if (phase_ == Phase::Responding || phase_ == Phase::Continuing) {
        parser_.queue(bytes);
        return Read{};
    }

    ParseResult result = parser_.append(bytes.data(), bytes.size());

    if (!requestStarted_ && !parser_.isIdle()) {
        requestStarted_ = clock_();
    }

    while (true) {
        if (const HeadersReady* headers = std::get_if<HeadersReady>(&result)) {
            if (auto action = prepare(*headers->request)) {
                return std::move(*action);
            }
        } else if (const BodyData* chunk = std::get_if<BodyData>(&result)) {
            if (!upload_->receive(chunk->bytes)) {
                return reject(HttpStatus::InternalServerError);
            }
        } else {
            break;
        }
        result = parser_.append(nullptr, 0);
    }

    if (std::holds_alternative<NeedMoreData>(result)) {
        return Read{};
    }

    phase_ = Phase::Responding;

    if (const Failed* failed = std::get_if<Failed>(&result)) {
        return reject(failed->status);
    }

    HttpRequest request = std::move(std::get<Complete>(result).request);

    framing_.close = !request.isPersistent();
    framing_.headersOnly = request.method == HttpMethod::Head;

    if (upload_) {
        HttpResponse response = upload_->end();
        upload_.reset();
        return respond(std::move(response));
    }

    HandlerResult handled = RequestDispatcher::dispatch(std::move(request), server_, info_);

    if (HttpResponse* response = std::get_if<HttpResponse>(&handled)) {
        return respond(std::move(*response));
    }

    cgi_ = std::make_unique<CgiStream>(framing_);
    return std::move(std::get<CgiRequest>(handled));
}

std::optional<HttpSession::Action> HttpSession::prepare(const HttpRequest& request) {
    framing_.close = !request.isPersistent();
    framing_.headersOnly = request.method == HttpMethod::Head;
    const auto route = Router::resolve(request, server_);

    if (route && !parser_.limit(route->clientMaxBodySize)) {
        return reject(HttpStatus::PayloadTooLarge);
    }

    const auto expect = request.headers.get(Http::Headers::Expect);
    if (expect && !HttpHeaders::equals(*expect, Http::Headers::Expectation::Continue)) {
        return reject(HttpStatus::ExpectationFailed);
    }
    if (!route) {
        return reject(HttpStatus::NotFound);
    }
    if (expect && !route->redirect && !route->allowedMethods.contains(request.method)) {
        return reject(HttpStatus::MethodNotAllowed);
    }

    if (!route->redirect && !route->cgi && route->upload && request.method == HttpMethod::Post &&
        route->allowedMethods.contains(request.method)) {
        upload_ = std::make_unique<UploadHandler>();
        if (const auto status = upload_->open(request, *route)) {
            return reject(*status);
        }
        parser_.stream();
    }

    if (expect && !parser_.isComplete()) {
        phase_ = Phase::Continuing;
        return Write{std::string(kContinueResponse)};
    }
    return std::nullopt;
}

HttpSession::Action HttpSession::reject(HttpStatus status) {
    upload_.reset();
    lingering_ = parser_.hasHeaders() && !parser_.isComplete();
    if (lingering_) {
        parser_.discard();
    }
    phase_ = Phase::Responding;
    framing_.close = true;
    return fail(status);
}

HttpSession::Action HttpSession::drain(std::string_view bytes) {
    const ParseResult result = parser_.append(bytes.data(), bytes.size());
    if (std::holds_alternative<Complete>(result)) {
        return Close{};
    }
    if (std::holds_alternative<Failed>(result)) {
        parser_.discard();
    }
    return Read{};
}

HttpSession::Action HttpSession::emit() {
    if (phase_ == Phase::Continuing) {
        phase_ = Phase::Reading;
        return receive({});
    }

    if (cgi_) {
        return ReadOutput{};
    }

    if (!body_) {
        return finish();
    }

    std::string bytes;

    if (!append(bytes)) {
        return Close{};
    }

    return Write{std::move(bytes)};
}

std::optional<HttpSession::Action> HttpSession::expire() {
    if (phase_ == Phase::Responding) {
        return std::nullopt;
    }

    const std::chrono::steady_clock::time_point current = clock_();

    if (phase_ == Phase::Lingering) {
        if (current - lastReceived_ >= kLingerTimeout || current - lingerStarted_ >= kLingerDuration) {
            return Close{};
        }
        return std::nullopt;
    }

    if (!requestStarted_) {
        if (current - lastReceived_ < kKeepAliveTimeout) {
            return std::nullopt;
        }

        return Close{};
    }

    if (!parser_.hasHeaders()) {
        if (current - *requestStarted_ < kHeaderTimeout) {
            return std::nullopt;
        }

        Log::info("client timed out while sending request header, client: " + info_.remoteAddr);
        return Close{};
    }

    if (current - lastReceived_ < kBodyTimeout) {
        return std::nullopt;
    }

    Log::info("client timed out while sending request body, client: " + info_.remoteAddr);
    return reject(HttpStatus::RequestTimeout);
}

HttpSession::Action HttpSession::receiveOutput(std::string_view output) {
    receivedOutput_ = receivedOutput_ || !output.empty();
    std::string bytes;

    if (!cgi_ || !cgi_->receive(output, bytes)) {
        return failOutput(std::move(bytes));
    }

    if (bytes.empty()) {
        return ReadOutput{};
    }

    startedOutput_ = true;
    return Stream{std::move(bytes)};
}

HttpSession::Action HttpSession::endOutput() {
    std::string bytes;

    if (!cgi_ || !cgi_->end(bytes)) {
        return failOutput();
    }

    cgi_.reset();

    if (!bytes.empty()) {
        return Write{std::move(bytes)};
    }

    return finish();
}

HttpSession::Action HttpSession::fail(CgiFailure failure) {
    cgi_.reset();

    if (startedOutput_) {
        return Close{};
    }

    switch (failure) {
        case CgiFailure::Busy:
            return fail(HttpStatus::ServiceUnavailable);
        case CgiFailure::Timeout:
            return fail(HttpStatus::GatewayTimeout);
        case CgiFailure::Launch:
        case CgiFailure::Output:
            break;
    }

    return fail(HttpStatus::BadGateway);
}

HttpSession::Action HttpSession::failOutput(std::string bytes) {
    const std::string problem = cgi_ && cgi_->hasHeaders() ? "CGI sent invalid response body"
        : receivedOutput_                                  ? "CGI sent invalid header"
                                                           : "CGI produced no usable output";
    Log::error(problem + ", client: " + info_.remoteAddr);
    cgi_.reset();

    if (!bytes.empty()) {
        framing_.close = true;
        startedOutput_ = true;
        return Write{std::move(bytes)};
    }

    return startedOutput_ ? Action{Close{}} : fail(HttpStatus::BadGateway);
}

HttpSession::Action HttpSession::fail(HttpStatus status) {
    return respond(ErrorResponseFactory::create(status, Router::mapErrorPages(server_)));
}

HttpSession::Action HttpSession::respond(HttpResponse response) {
    HttpSerializer::Output output = HttpSerializer::serialize(std::move(response), framing_);
    std::string bytes = std::move(output.headers);

    body_ = std::move(output.body);

    if (body_ && !append(bytes)) {
        return Close{};
    }

    return Write{std::move(bytes)};
}

bool HttpSession::append(std::string& bytes) {
    std::string chunk;

    if (!body_->next(chunk, kChunkSize)) {
        Log::error("failed to read response body (file was truncated or read failed), client: " + info_.remoteAddr);
        body_.reset();
        return false;
    }

    if (bytes.empty()) {
        bytes = std::move(chunk);
    } else {
        bytes += chunk;
    }

    if (body_->done()) {
        body_.reset();
    }

    return true;
}

HttpSession::Action HttpSession::finish() {
    if (lingering_) {
        phase_ = Phase::Lingering;
        lingerStarted_ = clock_();
        lastReceived_ = lingerStarted_;
        return drain({});
    }

    const bool close = framing_.close;

    parser_.reset();
    phase_ = Phase::Reading;
    framing_ = {};
    cgi_.reset();
    receivedOutput_ = false;
    startedOutput_ = false;
    lastReceived_ = clock_();
    requestStarted_.reset();

    if (close) {
        return Close{};
    }

    return receive({});
}

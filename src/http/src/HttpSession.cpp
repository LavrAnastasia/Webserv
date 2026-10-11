#include "http/HttpSession.hpp"

#include <utility>

#include "ErrorResponseFactory.hpp"
#include "Router.hpp"
#include "http/CgiResponseParser.hpp"
#include "http/HttpMethod.hpp"
#include "http/RequestDispatcher.hpp"
#include "log/Log.hpp"

namespace {
    constexpr std::size_t kChunkSize = 64 * 1024;
    // TODO: WEB-55 remove the cap once the output is streamed
    constexpr std::size_t kMaxOutputSize = 128 * 1024 * 1024;
} // namespace

HttpSession::HttpSession(const ServerConfig& server, ConnectionInfo info)
    : server_(server), info_(std::move(info)), parser_(server.maxBodySize()) {
}

HttpSession::Action HttpSession::receive(std::string_view bytes) {
    const ParseResult result = parser_.append(bytes.data(), bytes.size());

    if (phase_ == Phase::Responding || std::holds_alternative<NeedMoreData>(result)) {
        return Read{};
    }

    phase_ = Phase::Responding;

    if (const Failed* failed = std::get_if<Failed>(&result)) {
        framing_.close = true;
        return fail(failed->status);
    }

    const HttpRequest& request = std::get<Complete>(result).request;

    framing_.close = !request.isPersistent();
    framing_.headersOnly = request.method == HttpMethod::Head;

    HandlerResult handled = RequestDispatcher::dispatch(request, server_, info_);

    if (HttpResponse* response = std::get_if<HttpResponse>(&handled)) {
        return respond(std::move(*response));
    }

    return std::move(std::get<CgiRequest>(handled));
}

HttpSession::Action HttpSession::emit() {
    if (!body_) {
        return finish();
    }

    std::string bytes;

    if (!append(bytes)) {
        return Close{};
    }

    return Write{std::move(bytes)};
}

HttpSession::Action HttpSession::expire() {
    if (phase_ == Phase::Responding) {
        Log::info("client timed out while sending response, client: " + info_.remoteAddr);
        return Close{};
    }

    Log::info("client timed out while waiting for request, client: " + info_.remoteAddr);
    phase_ = Phase::Responding;
    framing_.close = true;
    return fail(HttpStatus::RequestTimeout);
}

HttpSession::Action HttpSession::receiveOutput(std::string_view bytes) {
    // TODO: WEB-55 stream the output to the client instead of collecting it
    if (output_.size() + bytes.size() > kMaxOutputSize) {
        Log::error("CGI output is too large, client: " + info_.remoteAddr);
        std::string().swap(output_);
        return fail(HttpStatus::BadGateway);
    }

    output_.append(bytes);
    return ReadOutput{};
}

HttpSession::Action HttpSession::endOutput() {
    std::string output;
    output.swap(output_);

    std::optional<HttpResponse> response = CgiResponseParser::parse(output);

    if (response) {
        return respond(std::move(*response));
    }

    const std::string problem = output.empty() ? "CGI produced no usable output" : "CGI sent invalid header";
    Log::error(problem + " while reading response header, client: " + info_.remoteAddr);
    return fail(HttpStatus::BadGateway);
}

HttpSession::Action HttpSession::fail(CgiFailure failure) {
    std::string().swap(output_);

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
    const bool close = framing_.close;

    parser_.reset();
    phase_ = Phase::Reading;
    framing_ = {};

    if (close) {
        return Close{};
    }

    return receive({});
}

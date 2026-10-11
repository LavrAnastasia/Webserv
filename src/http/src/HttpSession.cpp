#include "http/HttpSession.hpp"

#include <utility>

#include "http/HttpMethod.hpp"
#include "http/RequestDispatcher.hpp"
#include "log/Log.hpp"

namespace {
    constexpr std::size_t kChunkSize = 64 * 1024;
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
        return start(std::move(*response));
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

HttpSession::Action HttpSession::respond(HttpResponse response) {
    return start(std::move(response));
}

HttpSession::Action HttpSession::fail(HttpStatus status) {
    return start(RequestDispatcher::fail(status, server_));
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

HttpSession::Action HttpSession::start(HttpResponse response) {
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

    // TODO: WEB-53 parse a request that is already buffered (pipelining)
    parser_.reset();
    phase_ = Phase::Reading;
    framing_ = {};

    if (close) {
        return Close{};
    }

    return Read{};
}

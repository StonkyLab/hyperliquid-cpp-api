/**
Hyperliquid HTTPS Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_HTTP_SESSION_H
#define INCLUDE_STONKY_HYPERLIQUID_HTTP_SESSION_H

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace stonky::hyperliquid {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

class HTTPSession {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    explicit HTTPSession(const std::string& host = "");

    ~HTTPSession();

    [[nodiscard]] http::response<http::string_body> post(const std::string& path, const nlohmann::json& json) const;

    /**
     * POST a pre-serialised JSON body verbatim. The /exchange payload is signed
     * over a field-order-sensitive serialisation (ordered_json), so the caller
     * dumps it itself rather than letting nlohmann::json re-order keys.
     */
    [[nodiscard]] http::response<http::string_body> post(const std::string& path, const std::string& body) const;
};
} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_HTTP_SESSION_H

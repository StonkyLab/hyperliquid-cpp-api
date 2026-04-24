/**
Hyperliquid HTTPS Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_http_session.h"
#include "nlohmann/json.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/beast/version.hpp>
#include <openssl/err.h>

namespace stonky::hyperliquid {
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

constexpr auto API_URI = "api.hyperliquid.xyz";

struct HTTPSession::P {
    net::io_context ioc;
    std::string uri;

    http::response<http::string_body> request(http::request<http::string_body> req);
};

HTTPSession::HTTPSession(const std::string& host) : m_p(std::make_unique<P>()) {
    m_p->uri = host.empty() ? API_URI : host;
}

HTTPSession::~HTTPSession() = default;

http::response<http::string_body> HTTPSession::post(const std::string& path, const nlohmann::json& json) const {
    http::request<http::string_body> req{http::verb::post, path, 11};
    req.body() = json.dump();
    req.prepare_payload();
    req.set(http::field::content_type, "application/json");
    return m_p->request(req);
}

http::response<http::string_body> HTTPSession::P::request(http::request<http::string_body> req) {
    req.set(http::field::host, uri);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    ssl::context ctx{ssl::context::sslv23_client};
    ctx.set_default_verify_paths();

    tcp::resolver resolver{ioc};
    ssl::stream<tcp::socket> stream{ioc, ctx};

    if (!SSL_set_tlsext_host_name(stream.native_handle(), uri.c_str())) {
        boost::system::error_code ec{static_cast<int>(ERR_get_error()), net::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    auto const results = resolver.resolve(uri, "443");
    net::connect(stream.next_layer(), results.begin(), results.end());
    stream.handshake(ssl::stream_base::client);

    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);

    boost::system::error_code ec;
    [[maybe_unused]] const auto rc = stream.shutdown(ec);
    if (ec == boost::asio::error::eof) {
        ec.assign(0, ec.category());
    }

    return response;
}
} // namespace stonky::hyperliquid

/**
Hyperliquid WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_WS_SESSION_H
#define INCLUDE_STONKY_HYPERLIQUID_WS_SESSION_H

#include "stonky/utils/log_utils.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace stonky::hyperliquid {
using onLogMessage = std::function<void(LogSeverity, const std::string &)>;

/// Raw inbound data message ({"channel": ..., "data": ...}). The stream manager
/// dispatches on "channel".
using onDataEvent = std::function<void(const nlohmann::json &)>;

/**
 * One TLS WebSocket connection to wss://<host>/ws.
 *
 * Architecture mirrors the proven Lighter/MEXC sessions: outbound messages
 * (subscribe, ping) go through an internal write pump; every completion handler
 * is generation-guarded; on any transport error or inbound-silence timeout the
 * socket is torn down and reconnected with exponential backoff and all
 * subscriptions are replayed. Only close() stops the loop.
 *
 * Hyperliquid needs no login and sends no greeting — subscriptions flush right
 * after the WS handshake. Keepalive is CLIENT-driven: {"method":"ping"} every
 * 30 s, answered by {"channel":"pong"} (server drops quiet connections).
 * Subscriptions are JSON objects: {"method":"subscribe","subscription":{...}}.
 */
class WebSocketSession final : public std::enable_shared_from_this<WebSocketSession> {
    struct P;
    std::unique_ptr<P> m_p;

public:
    WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB);

    ~WebSocketSession();

    void run(const std::string &host, const std::string &port, const std::string &path, const onDataEvent &dataEventCB);

    /// Close asynchronously and disable automatic reconnect.
    void close() const;

    /**
     * Subscribe. Safe from any thread; queued until the connection is up.
     * @param subscription e.g. {"type":"bbo","coin":"ETH"}
     */
    void subscribe(const nlohmann::json &subscription) const;

    /// Unsubscribe a subscription previously passed to subscribe(). No-op otherwise.
    void unsubscribe(const nlohmann::json &subscription) const;

    /// True while the transport is up (post-handshake).
    [[nodiscard]] bool isConnected() const;

    [[nodiscard]] bool isSubscribed(const nlohmann::json &subscription) const;
};
} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_WS_SESSION_H

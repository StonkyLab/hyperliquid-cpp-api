/**
Hyperliquid WebSocket Stream Manager

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_WS_STREAM_MANAGER_H
#define INCLUDE_STONKY_HYPERLIQUID_WS_STREAM_MANAGER_H

#include "stonky/hyperliquid/hyperliquid_ws_session.h"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace stonky::hyperliquid {

/// Top-of-book update for a coin (0 when that side is empty).
using onBboUpdate = std::function<void(const std::string &coin, double bestBid, double bestAsk)>;

/// One orderUpdates entry: {"order":{coin,side,limitPx,sz,origSz,oid,cloid?},
/// "status":"open|filled|canceled|rejected|...","statusTimestamp":ms}.
using onOrderUpdate = std::function<void(const nlohmann::json &entry)>;

/// The userFills data object: {"isSnapshot"?:true,"fills":[{coin,px,sz,side,
/// time,oid,tid,cloid?,crossed,fee,dir},...]}.
using onUserFills = std::function<void(const nlohmann::json &data)>;

/**
 * Owns the io context + thread + WebSocketSession and turns Hyperliquid's
 * stream into typed callbacks. The bbo channel already carries best bid/ask —
 * no local book is maintained. Callbacks run on the io thread — keep them fast.
 */
class WSStreamManager {
    struct P;
    std::unique_ptr<P> m_p;

public:
    WSStreamManager();

    ~WSStreamManager();

    void setLoggerCallback(const onLogMessage &cb) const;

    void setBboCallback(const onBboUpdate &cb) const;

    void setOrderUpdateCallback(const onOrderUpdate &cb) const;

    void setUserFillsCallback(const onUserFills &cb) const;

    /// Override the endpoint (defaults to wss://api.hyperliquid.xyz/ws).
    void setEndpoint(const std::string &host, const std::string &port, const std::string &path) const;

    /// Connect and start the io thread. Idempotent.
    void start() const;

    void subscribeBbo(const std::string &coin) const;

    void unsubscribeBbo(const std::string &coin) const;

    /// Subscribe the account's order-status stream (orderUpdates).
    void subscribeOrderUpdates(const std::string &userAddress) const;

    /// Subscribe the account's fill stream (userFills).
    void subscribeUserFills(const std::string &userAddress) const;

    [[nodiscard]] bool isConnected() const;
};
} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_WS_STREAM_MANAGER_H

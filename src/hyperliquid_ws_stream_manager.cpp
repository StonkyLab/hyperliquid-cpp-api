/**
Hyperliquid WebSocket Stream Manager

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_ws_stream_manager.h"
#include "stonky/hyperliquid/tls_verify.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <fmt/format.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace stonky::hyperliquid {
namespace {
constexpr auto WS_HOST = "api.hyperliquid.xyz";
constexpr auto WS_PORT = "443";
constexpr auto WS_PATH = "/ws";

double levelPx(const nlohmann::json &level) {
    if (!level.is_object()) {
        return 0.0; /// empty book side arrives as null
    }

    if (const auto it = level.find("px"); it != level.end()) {
        if (it->is_string()) {
            try {
                return std::stod(it->get<std::string>());
            } catch (...) {
                return 0.0;
            }
        }
        if (it->is_number()) {
            return it->get<double>();
        }
    }

    return 0.0;
}
} // namespace

struct WSStreamManager::P {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23_client};
    std::shared_ptr<WebSocketSession> session;
    std::thread ioThread;
    std::atomic<bool> running{false};

    std::string host{WS_HOST};
    std::string port{WS_PORT};
    std::string path{WS_PATH};

    onLogMessage logCB;
    onBboUpdate bboCB;
    onOrderUpdate orderCB;
    onUserFills fillsCB;

    /// Guards the lazy session/io-thread creation (subscribe* can be called
    /// from multiple threads before start()).
    std::recursive_mutex clientLocker;

    void log(const LogSeverity sev, const std::string &msg) const {
        if (logCB) {
            logCB(sev, msg);
        }
    }

    void onMessage(const nlohmann::json &json) const {
        const auto channelIt = json.find("channel");
        if (channelIt == json.end() || !channelIt->is_string()) {
            return;
        }

        const auto channel = channelIt->get<std::string>();
        const auto dataIt = json.find("data");

        if (dataIt == json.end()) {
            return;
        }

        if (channel == "bbo") {
            if (bboCB && dataIt->is_object()) {
                const std::string coin = dataIt->value("coin", "");
                const auto bbo = dataIt->value("bbo", nlohmann::json::array());
                const double bid = !bbo.empty() ? levelPx(bbo.at(0)) : 0.0;
                const double ask = bbo.size() > 1 ? levelPx(bbo.at(1)) : 0.0;

                if (!coin.empty()) {
                    bboCB(coin, bid, ask);
                }
            }
        } else if (channel == "orderUpdates") {
            if (orderCB && dataIt->is_array()) {
                for (const auto &entry: *dataIt) {
                    orderCB(entry);
                }
            }
        } else if (channel == "userFills") {
            if (fillsCB && dataIt->is_object()) {
                fillsCB(*dataIt);
            }
        } else if (channel == "error") {
            log(LogSeverity::Error, fmt::format("Hyperliquid WS: {}", json.dump()));
        }
    }

    void ensureSession() {
        std::lock_guard lk(clientLocker);

        if (session) {
            return;
        }

        // Configure the SSL context BEFORE the session/io thread exists — a
        // concurrent set_default_verify_paths during the handshake is a data
        // race on the shared X509_STORE.
        enableTlsPeerVerification(ctx);

        session = std::make_shared<WebSocketSession>(ioc, ctx, logCB);
        session->run(host, port, path, [this](const nlohmann::json &json) { onMessage(json); });

        if (!running.exchange(true)) {
            ioThread = std::thread([this] {
                for (;;) {
                    try {
                        if (ioc.stopped()) {
                            ioc.restart();
                        }
                        ioc.run();
                        break;
                    } catch (std::exception &e) {
                        log(LogSeverity::Error, fmt::format("Hyperliquid WS io: {}", e.what()));
                    }
                }
            });
        }
    }
};

WSStreamManager::WSStreamManager() : m_p(std::make_unique<P>()) {}

WSStreamManager::~WSStreamManager() {
    if (m_p->session) {
        m_p->session->close();
    }

    m_p->ioc.stop();

    if (m_p->ioThread.joinable()) {
        m_p->ioThread.join();
    }
}

void WSStreamManager::setLoggerCallback(const onLogMessage &cb) const { m_p->logCB = cb; }

void WSStreamManager::setBboCallback(const onBboUpdate &cb) const { m_p->bboCB = cb; }

void WSStreamManager::setOrderUpdateCallback(const onOrderUpdate &cb) const { m_p->orderCB = cb; }

void WSStreamManager::setUserFillsCallback(const onUserFills &cb) const { m_p->fillsCB = cb; }

void WSStreamManager::setEndpoint(const std::string &host, const std::string &port, const std::string &path) const {
    m_p->host = host;
    m_p->port = port;
    m_p->path = path;
}

void WSStreamManager::start() const { m_p->ensureSession(); }

void WSStreamManager::subscribeBbo(const std::string &coin) const {
    m_p->ensureSession();
    m_p->session->subscribe(nlohmann::json{{"type", "bbo"}, {"coin", coin}});
}

void WSStreamManager::unsubscribeBbo(const std::string &coin) const {
    if (m_p->session) {
        m_p->session->unsubscribe(nlohmann::json{{"type", "bbo"}, {"coin", coin}});
    }
}

void WSStreamManager::subscribeOrderUpdates(const std::string &userAddress) const {
    m_p->ensureSession();
    m_p->session->subscribe(nlohmann::json{{"type", "orderUpdates"}, {"user", userAddress}});
}

void WSStreamManager::subscribeUserFills(const std::string &userAddress) const {
    m_p->ensureSession();
    m_p->session->subscribe(nlohmann::json{{"type", "userFills"}, {"user", userAddress}});
}

bool WSStreamManager::isConnected() const { return m_p->session && m_p->session->isConnected(); }
} // namespace stonky::hyperliquid

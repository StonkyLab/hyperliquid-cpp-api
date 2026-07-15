/**
Hyperliquid Exchange Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_exchange_client.h"
#include "stonky/hyperliquid/hyperliquid_http_session.h"
#include "stonky/hyperliquid/hyperliquid_signer.h"
#include <fmt/format.h>
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace stonky::hyperliquid {
namespace {
const char *tifName(const Tif tif) {
    switch (tif) {
        case Tif::Alo: return "Alo";
        case Tif::Gtc: return "Gtc";
        case Tif::Ioc: return "Ioc";
    }

    return "Alo";
}

double readNum(const nlohmann::json &json, const char *key) {
    if (const auto it = json.find(key); it != json.end()) {
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

/// Parse the first entry of response.data.statuses (single-order actions).
ActionStatus parseStatuses(const nlohmann::json &response) {
    ActionStatus status;

    const auto statusIt = response.find("status");
    if (statusIt == response.end() || !statusIt->is_string() || statusIt->get<std::string>() != "ok") {
        status.kind = ActionStatus::Kind::Error;
        const auto respIt = response.find("response");
        status.error = respIt != response.end() ? (respIt->is_string() ? respIt->get<std::string>() : respIt->dump()) : response.dump();
        return status;
    }

    const auto &data = response.at("response").value("data", nlohmann::json::object());
    const auto statuses = data.value("statuses", nlohmann::json::array());

    if (statuses.empty()) {
        /// e.g. cancel responses can be plain {"type":"cancel","data":{"statuses":["success"]}};
        /// an empty list still means the action itself was accepted.
        status.kind = ActionStatus::Kind::Success;
        return status;
    }

    const auto &first = statuses.front();

    if (first.is_string()) {
        if (first.get<std::string>() == "success" || first.get<std::string>() == "waitingForFill" || first.get<std::string>() == "waitingForTrigger") {
            status.kind = ActionStatus::Kind::Success;
        } else {
            status.kind = ActionStatus::Kind::Error;
            status.error = first.get<std::string>();
        }
        return status;
    }

    if (first.contains("resting")) {
        status.kind = ActionStatus::Kind::Resting;
        status.oid = first["resting"].value("oid", 0ULL);
        return status;
    }

    if (first.contains("filled")) {
        status.kind = ActionStatus::Kind::Filled;
        status.oid = first["filled"].value("oid", 0ULL);
        status.filledSz = readNum(first["filled"], "totalSz");
        status.avgPx = readNum(first["filled"], "avgPx");
        return status;
    }

    status.kind = ActionStatus::Kind::Error;
    status.error = first.value("error", first.dump());
    return status;
}
} // namespace

std::string floatToWire(const double value) {
    std::string s = fmt::format("{:.8f}", value);

    /// Strip trailing zeros, then a trailing dot ("100.00000000" → "100").
    const auto lastNonZero = s.find_last_not_of('0');
    s.erase(s[lastNonZero] == '.' ? lastNonZero : lastNonZero + 1);

    if (s == "-0") {
        s = "0";
    }

    return s;
}

struct ExchangeClient::P {
    std::unique_ptr<L1Signer> signer;
    std::unique_ptr<HTTPSession> httpSession;

    /// Strictly increasing ms nonce (the venue requires per-address uniqueness
    /// and rough recency; concurrent legs bump it atomically).
    std::atomic<std::uint64_t> lastNonce{0};

    [[nodiscard]] std::uint64_t nextNonce() {
        const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        std::uint64_t prev = lastNonce.load();

        for (;;) {
            const std::uint64_t next = std::max(now, prev + 1);

            if (lastNonce.compare_exchange_weak(prev, next)) {
                return next;
            }
        }
    }

    /// Sign and POST one action; throws on transport error, returns the parsed
    /// venue verdict otherwise.
    [[nodiscard]] ActionStatus postAction(const nlohmann::ordered_json &action) {
        const auto nonce = nextNonce();
        const auto signature = signer->signL1Action(action, nonce);

        nlohmann::ordered_json payload;
        payload["action"] = action;
        payload["nonce"] = nonce;
        payload["signature"] = {{"r", signature.r}, {"s", signature.s}, {"v", signature.v}};
        payload["vaultAddress"] = nullptr;

        const auto response = httpSession->post("/exchange", payload.dump());

        if (response.result() != boost::beast::http::status::ok) {
            throw std::runtime_error(fmt::format("Hyperliquid /exchange HTTP {}: {}", response.result_int(), response.body()));
        }

        return parseStatuses(nlohmann::json::parse(response.body()));
    }
};

ExchangeClient::ExchangeClient(const std::string &privateKeyHex, const bool isMainnet, const std::string &host) : m_p(std::make_unique<P>()) {
    m_p->signer = std::make_unique<L1Signer>(privateKeyHex, isMainnet);
    m_p->httpSession = std::make_unique<HTTPSession>(host);
}

ExchangeClient::~ExchangeClient() = default;

std::string ExchangeClient::signerAddress() const { return m_p->signer->address(); }

ActionStatus ExchangeClient::placeLimit(const int asset, const bool isBuy, const std::string &pxWire, const std::string &szWire, const bool reduceOnly, const Tif tif,
                                        const std::string &cloid) const {
    /// Field order is signature-relevant — mirror the reference SDK exactly:
    /// order {a,b,p,s,r,t,c}; action {type,orders,grouping}.
    nlohmann::ordered_json order;
    order["a"] = asset;
    order["b"] = isBuy;
    order["p"] = pxWire;
    order["s"] = szWire;
    order["r"] = reduceOnly;
    order["t"] = nlohmann::ordered_json{{"limit", nlohmann::ordered_json{{"tif", tifName(tif)}}}};
    order["c"] = cloid;

    nlohmann::ordered_json action;
    action["type"] = "order";
    action["orders"] = nlohmann::ordered_json::array({order});
    action["grouping"] = "na";

    return m_p->postAction(action);
}

ActionStatus ExchangeClient::cancelByCloid(const int asset, const std::string &cloid) const {
    nlohmann::ordered_json cancel;
    cancel["asset"] = asset;
    cancel["cloid"] = cloid;

    nlohmann::ordered_json action;
    action["type"] = "cancelByCloid";
    action["cancels"] = nlohmann::ordered_json::array({cancel});

    return m_p->postAction(action);
}

} // namespace stonky::hyperliquid

/**
Hyperliquid Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_execution_gateway.h"
#include "stonky/hyperliquid/hyperliquid_exchange_client.h"
#include "stonky/hyperliquid/hyperliquid_rest_client.h"
#include "stonky/hyperliquid/hyperliquid_ws_stream_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <thread>

namespace stonky::execution {
using namespace stonky::hyperliquid;

namespace {
constexpr double MIN_ORDER_VALUE_USD = 10.0; ///< venue-wide minimum order notional
constexpr double MARKET_SLIPPAGE = 0.05;     ///< IOC "market" aggression, the SDK default

void logForwarder(const LogSeverity severity, const std::string &message) {
    switch (severity) {
        case LogSeverity::Info:
            spdlog::info(message);
            break;
        case LogSeverity::Warning:
            spdlog::warn(message);
            break;
        case LogSeverity::Critical:
        case LogSeverity::Error:
            spdlog::error(message);
            break;
        default:
            spdlog::debug(message);
            break;
    }
}

std::string toLower(std::string s) {
    std::ranges::transform(s, s.begin(), [](const unsigned char c) { return std::tolower(c); });
    return s;
}

double pow10i(const int n) {
    double r = 1.0;
    for (int i = 0; i < std::abs(n); ++i) {
        r *= 10.0;
    }
    return n < 0 ? 1.0 / r : r;
}

/// Decimals allowed at this price level: 5 significant figures, capped at
/// (6 − szDecimals) for perps; never negative (integer prices always pass).
int allowedDecimals(const double px, const int szDecimals) {
    const int sigFigDecimals = px > 0.0 ? 4 - static_cast<int>(std::floor(std::log10(px))) : 6 - szDecimals;
    return std::clamp(std::min(6 - szDecimals, sigFigDecimals), 0, 8);
}

/// Snap a price onto the venue grid at its own magnitude.
double normalizePrice(const double px, const int szDecimals) {
    const int decimals = allowedDecimals(px, szDecimals);
    return std::round(px * pow10i(decimals)) / pow10i(decimals);
}

/// Classify a synchronous /exchange rejection string.
RejectKind classifyReject(const std::string &reason) {
    const auto r = toLower(reason);

    if (r.find("post only") != std::string::npos || r.find("immediately match") != std::string::npos) {
        return RejectKind::BenignPostOnlyCross;
    }

    if (r.find("minimum value") != std::string::npos || r.find("order must have") != std::string::npos) {
        return RejectKind::MinNotional;
    }

    if (r.find("rate limit") != std::string::npos || r.find("too many") != std::string::npos || r.find("429") != std::string::npos) {
        return RejectKind::Throttled;
    }

    if (r.find("reduce only") != std::string::npos) {
        /// "Reduce only order would increase position" — the position this
        /// close was meant for is gone/flipped; the goal is met.
        return RejectKind::PositionClosed;
    }

    if (r.find("invalid asset") != std::string::npos || r.find("does not exist") != std::string::npos || r.find("not allowed") != std::string::npos ||
        r.find("open interest cap") != std::string::npos || r.find("delisted") != std::string::npos) {
        return RejectKind::Permanent;
    }

    return RejectKind::Hard;
}

/// Cancel rejection meaning the order already left the book.
bool isOrderGoneReason(const std::string &reason) {
    const auto r = toLower(reason);
    return r.find("never placed") != std::string::npos || r.find("already canceled") != std::string::npos || r.find("or filled") != std::string::npos ||
           r.find("unknown oid") != std::string::npos || r.find("was already") != std::string::npos;
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
} // namespace

struct HyperliquidExecutionGateway::P {
    std::unique_ptr<ExchangeClient> exchangeClient;
    std::unique_ptr<RESTClient> restClient;
    std::unique_ptr<WSStreamManager> stream;

    std::string accountAddress; ///< master address for state/event queries
    bool isMainnet{true};

    onOrderUpdateEvent orderUpdateCB;
    onFillEvent fillCB;
    onQuoteEvent quoteCB;

    // ── Instrument metadata ─────────────────────────────────────────
    std::mutex specM;
    std::map<std::string, int> assetIndex;   ///< coin → trading asset id
    std::map<std::string, int> szDecimals;   ///< coin → size decimals
    std::map<std::string, double> lastMid;   ///< coin → mid at the last ctx refresh
    std::map<std::string, InstrumentSpec> specCache;

    // ── Quotes ──────────────────────────────────────────────────────
    std::mutex quoteM;
    std::map<std::string, Quote> quoteCache;

    // ── Order bookkeeping ───────────────────────────────────────────
    /// The core's clientOrderId is a free-form string; the venue's cloid is a
    /// fixed 16-byte hex. Map both ways; entries are pruned on terminal states.
    struct OrderRec {
        std::string clientOrderId;
        std::string symbol;
        double origSz{0.0};
    };

    std::mutex orderM;
    std::map<std::string, OrderRec> byCloid;          ///< cloid → rec
    std::map<std::string, std::string> clientToCloid; ///< clientOrderId → cloid
    std::atomic<std::uint64_t> cloidCounter{1};

    [[nodiscard]] std::string makeCloid(const std::string &clientOrderId, const std::string &symbol, const double origSz) {
        const auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        const auto cloid = fmt::format("0x{:016x}{:016x}", ms, cloidCounter.fetch_add(1));

        std::lock_guard lk(orderM);
        byCloid[cloid] = OrderRec{clientOrderId, symbol, origSz};
        clientToCloid[clientOrderId] = cloid;
        return cloid;
    }

    [[nodiscard]] std::optional<OrderRec> recForCloid(const std::string &cloid) {
        std::lock_guard lk(orderM);
        const auto it = byCloid.find(cloid);
        return it != byCloid.end() ? std::optional(it->second) : std::nullopt;
    }

    [[nodiscard]] std::string cloidFor(const std::string &clientOrderId) {
        std::lock_guard lk(orderM);
        const auto it = clientToCloid.find(clientOrderId);
        return it != clientToCloid.end() ? it->second : std::string{};
    }

    /// Terminal order → drop the mapping (unbounded growth otherwise; a late
    /// duplicate event is dropped by the core's route/fill dedup anyway).
    void pruneCloid(const std::string &cloid) {
        std::lock_guard lk(orderM);

        if (const auto it = byCloid.find(cloid); it != byCloid.end()) {
            clientToCloid.erase(it->second.clientOrderId);
            byCloid.erase(it);
        }
    }

    [[nodiscard]] int assetFor(const std::string &symbol) {
        std::lock_guard lk(specM);
        const auto it = assetIndex.find(symbol);
        return it != assetIndex.end() ? it->second : -1;
    }

    [[nodiscard]] int szDecimalsFor(const std::string &symbol) {
        std::lock_guard lk(specM);
        const auto it = szDecimals.find(symbol);
        return it != szDecimals.end() ? it->second : 0;
    }

    /// (Re)load the tradeable universe: asset ids, size decimals and a spec
    /// per coin. tickSize reflects the venue's 5-significant-figure grid AT the
    /// coin's current price level (refreshed here and on refreshInstruments;
    /// submit re-normalises the actual order price, so a mid-leg magnitude
    /// crossing cannot produce an off-grid price).
    void loadUniverse() {
        auto contexts = restClient->getAssetContexts();

        std::lock_guard lk(specM);

        for (const auto &ctx: contexts) {
            if (ctx.coin.empty() || ctx.isDelisted) {
                continue;
            }

            const double refPx = ctx.midPx > 0.0 ? ctx.midPx : ctx.markPx;
            assetIndex[ctx.coin] = ctx.assetIndex;
            szDecimals[ctx.coin] = ctx.szDecimals;
            lastMid[ctx.coin] = refPx;

            InstrumentSpec spec;
            spec.symbol = ctx.coin;
            spec.qtyStep = pow10i(-ctx.szDecimals);
            spec.minQty = spec.qtyStep;
            spec.maxQty = 0.0;
            spec.minNotional = MIN_ORDER_VALUE_USD;
            spec.tickSize = refPx > 0.0 ? pow10i(-allowedDecimals(refPx, ctx.szDecimals)) : pow10i(-(6 - ctx.szDecimals));
            specCache[ctx.coin] = spec;
        }
    }

    // ── Event handlers (io thread) ──────────────────────────────────

    void onOrderUpdateMsg(const nlohmann::json &entry) {
        const auto orderIt = entry.find("order");

        if (orderIt == entry.end() || !orderIt->is_object()) {
            return;
        }

        const std::string cloid = orderIt->value("cloid", "");

        if (cloid.empty()) {
            return; /// not ours (no cloid attached)
        }

        const auto rec = recForCloid(cloid);

        if (!rec) {
            return; /// unknown/foreign order
        }

        const std::string status = entry.value("status", "");
        const double remaining = readNum(*orderIt, "sz");
        const double origSz = readNum(*orderIt, "origSz");

        OrderUpdate update;
        update.clientOrderId = rec->clientOrderId;
        update.symbol = rec->symbol;
        update.price = readNum(*orderIt, "limitPx");
        update.cumFilledQty = std::max(0.0, origSz - remaining);
        update.reason = status;

        bool terminal = false;

        if (status == "open") {
            update.state = OrderState::Accepted;
        } else if (status == "filled") {
            update.state = OrderState::Filled;
            terminal = true;
        } else if (status == "canceled" || status == "marginCanceled" || status == "liquidatedCanceled" || status == "openInterestCapCanceled" ||
                   status == "selfTradeCanceled" || status == "reduceOnlyCanceled" || status == "siblingFilledCanceled" || status == "delistedCanceled") {
            update.state = OrderState::Cancelled;
            terminal = true;
        } else if (status == "rejected") {
            update.state = OrderState::Rejected;
            update.rejectKind = RejectKind::Hard;
            terminal = true;
        } else {
            return; /// triggered/other conditional states — not used
        }

        spdlog::debug("HyperliquidGW order: {} cloid={} status={} filled={}/{}", rec->symbol, cloid, status, update.cumFilledQty, origSz);

        if (orderUpdateCB) {
            orderUpdateCB(update);
        }

        if (terminal) {
            pruneCloid(cloid);
        }
    }

    void onUserFillsMsg(const nlohmann::json &data) {
        /// The first message after subscribe is a snapshot of HISTORICAL fills —
        /// crediting those would corrupt the accounting.
        if (data.value("isSnapshot", false)) {
            return;
        }

        const auto fillsIt = data.find("fills");

        if (fillsIt == data.end() || !fillsIt->is_array()) {
            return;
        }

        for (const auto &fillJson: *fillsIt) {
            emitFill(fillJson);
        }
    }

    /// Emit one fill (WS or REST re-emission — same tid key, the core dedups).
    void emitFill(const nlohmann::json &fillJson) {
        if (!fillCB) {
            return;
        }

        const std::string cloid = fillJson.value("cloid", "");

        if (cloid.empty()) {
            return;
        }

        const auto rec = recForCloid(cloid);

        if (!rec) {
            spdlog::debug("HyperliquidGW fill for unknown cloid {} — dropped", cloid);
            return;
        }

        FillEvent fill;
        fill.clientOrderId = rec->clientOrderId;
        fill.symbol = rec->symbol;
        fill.fillId = std::to_string(fillJson.value("tid", 0ULL));
        fill.qty = readNum(fillJson, "sz");
        fill.price = readNum(fillJson, "px");
        fill.isMaker = !fillJson.value("crossed", false);
        fillCB(fill);
    }

    /// The venue reported an order gone at cancel time — "gone" can mean
    /// FILLED. Re-emit the order's fills from REST (fillId = tid → the core
    /// dedups against the WS feed and credits only what it missed).
    void reconcileMissedFills(const std::string &clientOrderId, const std::string &symbol) {
        if (!fillCB) {
            return;
        }

        const auto cloid = cloidFor(clientOrderId);

        if (cloid.empty()) {
            return;
        }

        try {
            int reemitted = 0;

            for (const auto &fill: restClient->getUserFills(accountAddress)) {
                if (fill.cloid != cloid || fill.tid == 0) {
                    continue;
                }

                FillEvent event;
                event.clientOrderId = clientOrderId;
                event.symbol = symbol;
                event.fillId = std::to_string(fill.tid);
                event.qty = fill.sz;
                event.price = fill.px;
                event.isMaker = !fill.crossed;
                fillCB(event);
                ++reemitted;
            }

            if (reemitted > 0) {
                spdlog::info("HyperliquidGW: {} order {} gone at cancel — re-emitted {} fill(s) from REST (WS-gap safety; core dedups by tid)", symbol, clientOrderId, reemitted);
            }
        } catch (std::exception &e) {
            spdlog::warn("HyperliquidGW: {} fill reconciliation for {} failed ({}) — verify the position; a WS-gap fill may be uncredited", symbol, clientOrderId, e.what());
        }
    }
};

HyperliquidExecutionGateway::HyperliquidExecutionGateway(const std::string &privateKeyHex, const std::string &accountAddress, const bool isMainnet) : m_p(std::make_unique<P>()) {
    m_p->isMainnet = isMainnet;
    m_p->exchangeClient = std::make_unique<ExchangeClient>(privateKeyHex, isMainnet);
    m_p->restClient = std::make_unique<RESTClient>();
    m_p->stream = std::make_unique<WSStreamManager>();
    m_p->stream->setLoggerCallback(&logForwarder);
    m_p->accountAddress = accountAddress.empty() ? m_p->exchangeClient->signerAddress() : toLower(accountAddress);
}

HyperliquidExecutionGateway::~HyperliquidExecutionGateway() = default;

std::string HyperliquidExecutionGateway::name() const { return "Hyperliquid"; }

void HyperliquidExecutionGateway::start() {
    m_p->loadUniverse();

    m_p->stream->setBboCallback([this](const std::string &coin, const double bid, const double ask) {
        Quote quote;
        quote.bid = bid;
        quote.ask = ask;
        quote.receivedAt = std::chrono::steady_clock::now();

        {
            std::lock_guard lk(m_p->quoteM);
            m_p->quoteCache[coin] = quote;
        }

        if (m_p->quoteCB) {
            m_p->quoteCB(coin, quote);
        }
    });

    m_p->stream->setOrderUpdateCallback([this](const nlohmann::json &entry) { m_p->onOrderUpdateMsg(entry); });
    m_p->stream->setUserFillsCallback([this](const nlohmann::json &data) { m_p->onUserFillsMsg(data); });

    /// The event feed must be live before any order op — orders must never be
    /// placed before fills can be observed.
    m_p->stream->subscribeOrderUpdates(m_p->accountAddress);
    m_p->stream->subscribeUserFills(m_p->accountAddress);
    m_p->stream->start();

    for (int i = 0; i < 100 && !m_p->stream->isConnected(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!m_p->stream->isConnected()) {
        throw std::runtime_error("Hyperliquid gateway: WS did not connect within 10 s");
    }

    spdlog::info("HyperliquidExecutionGateway: ready ({} instruments, account {}, signer {})", m_p->specCache.size(), m_p->accountAddress,
                 m_p->exchangeClient->signerAddress());
}

InstrumentSpec HyperliquidExecutionGateway::instrumentSpec(const std::string &symbol) {
    {
        std::lock_guard lk(m_p->specM);

        if (const auto it = m_p->specCache.find(symbol); it != m_p->specCache.end()) {
            /// Refresh the tick to the CURRENT price level when a live quote is
            /// available — the 5-sig-fig grid moves with price magnitude.
            std::lock_guard qlk(m_p->quoteM);

            if (const auto q = m_p->quoteCache.find(symbol); q != m_p->quoteCache.end() && q->second.bid > 0.0) {
                auto spec = it->second;
                spec.tickSize = pow10i(-allowedDecimals(q->second.bid, m_p->szDecimals[symbol]));
                return spec;
            }

            return it->second;
        }
    }

    /// Miss → maybe listed after start; refresh once.
    m_p->loadUniverse();

    std::lock_guard lk(m_p->specM);

    if (const auto it = m_p->specCache.find(symbol); it != m_p->specCache.end()) {
        return it->second;
    }

    throw std::runtime_error(fmt::format("Hyperliquid: unknown instrument {}", symbol));
}

void HyperliquidExecutionGateway::refreshInstruments() { m_p->loadUniverse(); }

void HyperliquidExecutionGateway::subscribeQuotes(const std::string &symbol) { m_p->stream->subscribeBbo(symbol); }

void HyperliquidExecutionGateway::unsubscribeQuotes(const std::string &symbol) { m_p->stream->unsubscribeBbo(symbol); }

std::optional<Quote> HyperliquidExecutionGateway::lastQuote(const std::string &symbol) {
    std::lock_guard lk(m_p->quoteM);
    const auto it = m_p->quoteCache.find(symbol);

    if (it == m_p->quoteCache.end()) {
        return std::nullopt;
    }

    return it->second;
}

void HyperliquidExecutionGateway::setOrderUpdateCallback(const onOrderUpdateEvent &cb) { m_p->orderUpdateCB = cb; }

void HyperliquidExecutionGateway::setFillCallback(const onFillEvent &cb) { m_p->fillCB = cb; }

void HyperliquidExecutionGateway::setQuoteCallback(const onQuoteEvent &cb) { m_p->quoteCB = cb; }

void HyperliquidExecutionGateway::submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty, const double price,
                                                      const bool reduceOnly) {
    const int asset = m_p->assetFor(symbol);

    if (asset < 0) {
        throw GatewayError(RejectKind::Permanent, fmt::format("Hyperliquid: unknown instrument {}", symbol));
    }

    const int sd = m_p->szDecimalsFor(symbol);
    const double px = normalizePrice(price, sd);
    const double sz = std::round(qty * pow10i(sd)) / pow10i(sd);

    if (sz <= 0.0) {
        throw GatewayError(RejectKind::MinNotional, fmt::format("Hyperliquid: qty {} rounds to 0 (szDecimals {})", qty, sd));
    }

    const auto cloid = m_p->makeCloid(clientOrderId, symbol, sz);
    const auto status = m_p->exchangeClient->placeLimit(asset, side == OrderSide::Buy, floatToWire(px), floatToWire(sz), reduceOnly, Tif::Alo, cloid);

    if (status.kind == ActionStatus::Kind::Error) {
        m_p->pruneCloid(cloid);
        throw GatewayError(classifyReject(status.error), status.error);
    }

    spdlog::debug("HyperliquidGW submit ok: {} {} sz={} px={} cloid={} {}", symbol, side == OrderSide::Buy ? "Buy" : "Sell", sz, px, cloid,
                  status.kind == ActionStatus::Kind::Filled ? "(filled immediately)" : "(resting)");
}

bool HyperliquidExecutionGateway::supportsAmend() const { return false; }

void HyperliquidExecutionGateway::amendPrice(const std::string &, const std::string &, double) {
    throw GatewayError(RejectKind::Hard, "Hyperliquid: amend not wired (cancel + resubmit)");
}

bool HyperliquidExecutionGateway::cancel(const std::string &clientOrderId, const std::string &symbol) {
    const int asset = m_p->assetFor(symbol);
    const auto cloid = m_p->cloidFor(clientOrderId);

    if (asset < 0 || cloid.empty()) {
        return false;
    }

    const auto status = m_p->exchangeClient->cancelByCloid(asset, cloid);

    if (status.kind == ActionStatus::Kind::Error) {
        if (isOrderGoneReason(status.error)) {
            /// "Gone" can mean FILLED — if the WS dropped the fill, the core
            /// would resubmit the full remaining and overfill. Reconcile first.
            m_p->reconcileMissedFills(clientOrderId, symbol);
            return false;
        }

        throw GatewayError(classifyReject(status.error), status.error);
    }

    return true;
}

void HyperliquidExecutionGateway::submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty) {
    const int asset = m_p->assetFor(symbol);

    if (asset < 0) {
        throw GatewayError(RejectKind::Permanent, fmt::format("Hyperliquid: unknown instrument {}", symbol));
    }

    /// No native market order — an aggressive IOC limit is the venue idiom
    /// (the reference SDK's market_open does exactly this).
    double ref = 0.0;
    {
        std::lock_guard lk(m_p->quoteM);

        if (const auto it = m_p->quoteCache.find(symbol); it != m_p->quoteCache.end()) {
            ref = side == OrderSide::Buy ? it->second.ask : it->second.bid;
        }
    }

    if (ref <= 0.0) {
        std::lock_guard lk(m_p->specM);
        ref = m_p->lastMid[symbol];
    }

    if (ref <= 0.0) {
        throw GatewayError(RejectKind::Hard, fmt::format("Hyperliquid: no reference price for {} market close", symbol));
    }

    const int sd = m_p->szDecimalsFor(symbol);
    const double px = normalizePrice(side == OrderSide::Buy ? ref * (1.0 + MARKET_SLIPPAGE) : ref * (1.0 - MARKET_SLIPPAGE), sd);
    const double sz = std::round(qty * pow10i(sd)) / pow10i(sd);

    if (sz <= 0.0) {
        throw GatewayError(RejectKind::MinNotional, fmt::format("Hyperliquid: qty {} rounds to 0 (szDecimals {})", qty, sd));
    }

    const auto cloid = m_p->makeCloid(clientOrderId, symbol, sz);
    const auto status = m_p->exchangeClient->placeLimit(asset, side == OrderSide::Buy, floatToWire(px), floatToWire(sz), true, Tif::Ioc, cloid);

    if (status.kind == ActionStatus::Kind::Error) {
        m_p->pruneCloid(cloid);
        throw GatewayError(classifyReject(status.error), status.error);
    }
}

} // namespace stonky::execution

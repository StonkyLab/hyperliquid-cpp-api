/**
Hyperliquid Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef HYPERLIQUID_API_HYPERLIQUID_MODELS_H
#define HYPERLIQUID_API_HYPERLIQUID_MODELS_H

#include "stonky/hyperliquid/hyperliquid_enums.h"
#include "stonky/interface/i_json.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

namespace stonky::hyperliquid {

struct Candle final : IJson {
    std::int64_t startTime{};
    std::int64_t closeTime{};
    std::string coin{};
    std::string interval{};
    double open{};
    double close{};
    double high{};
    double low{};
    double volume{};
    int numTrades{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct FundingRate final : IJson {
    std::string coin{};
    double fundingRate{};
    double premium{};
    std::int64_t time{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct PerpAsset final : IJson {
    std::string name{};
    int szDecimals{};
    int maxLeverage{};
    bool isDelisted{false};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/// One perp market merged from metaAndAssetCtxs: the universe entry (name,
/// szDecimals, delisted) + its live context (funding, prices). assetIndex is
/// the coin's position in the FULL universe — the id trading actions use.
struct AssetContext final : IJson {
    int assetIndex{-1};
    std::string coin{};
    int szDecimals{};
    bool isDelisted{false};
    double funding{};  ///< current hourly funding rate (signed decimal)
    double markPx{};
    double midPx{};    ///< 0 when the book is empty
    double oraclePx{};
    double dayNtlVlm{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/// One account fill (userFills REST/WS share this shape).
struct UserFill final : IJson {
    std::string coin{};
    double px{};
    double sz{};
    std::int64_t time{};
    std::uint64_t oid{};
    std::uint64_t tid{};    ///< unique trade id — the fill dedup key
    std::string cloid{};    ///< client order id, empty when none was attached
    bool crossed{false};    ///< true = taker
    double fee{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/// An open perp position from clearinghouseState.
struct AccountPosition final : IJson {
    std::string coin{};
    double signedSize{};     ///< szi: >0 long, <0 short (base units)
    double entryPx{};
    double positionValue{};  ///< notional in USDC
    double unrealizedPnl{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/// Perp account state (clearinghouseState).
struct ClearinghouseState final : IJson {
    double accountValue{};  ///< total equity incl. unrealized PnL (USDC)
    double withdrawable{};
    double totalMarginUsed{};
    std::vector<AccountPosition> positions{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
} // namespace stonky::hyperliquid

#endif // HYPERLIQUID_API_HYPERLIQUID_MODELS_H

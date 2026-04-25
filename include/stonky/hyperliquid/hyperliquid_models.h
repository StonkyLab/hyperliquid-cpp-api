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
} // namespace stonky::hyperliquid

#endif // HYPERLIQUID_API_HYPERLIQUID_MODELS_H

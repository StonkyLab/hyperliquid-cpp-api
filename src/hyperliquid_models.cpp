/**
Hyperliquid Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_models.h"
#include "stonky/utils/json_utils.h"

namespace stonky::hyperliquid {

nlohmann::json Candle::toJson() const {
    throw std::runtime_error("Unimplemented: Candle::toJson()");
}

void Candle::fromJson(const nlohmann::json& json) {
    readValue<std::int64_t>(json, "t", startTime);
    readValue<std::int64_t>(json, "T", closeTime);
    readValue<std::string>(json, "s", coin);
    readValue<std::string>(json, "i", interval);
    open = readStringAsDouble(json, "o", open);
    close = readStringAsDouble(json, "c", close);
    high = readStringAsDouble(json, "h", high);
    low = readStringAsDouble(json, "l", low);
    volume = readStringAsDouble(json, "v", volume);
    readValue<int>(json, "n", numTrades);
}

nlohmann::json FundingRate::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRate::toJson()");
}

void FundingRate::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "coin", coin);
    fundingRate = readStringAsDouble(json, "fundingRate", fundingRate);
    premium = readStringAsDouble(json, "premium", premium);
    readValue<std::int64_t>(json, "time", time);
}

} // namespace stonky::hyperliquid

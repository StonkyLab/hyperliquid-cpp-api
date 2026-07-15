/**
Hyperliquid Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_models.h"
#include "stonky/utils/json_utils.h"

namespace stonky::hyperliquid {

nlohmann::json Candle::toJson() const { throw std::runtime_error("Unimplemented: Candle::toJson()"); }

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

nlohmann::json FundingRate::toJson() const { throw std::runtime_error("Unimplemented: FundingRate::toJson()"); }

void FundingRate::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "coin", coin);
    fundingRate = readStringAsDouble(json, "fundingRate", fundingRate);
    premium = readStringAsDouble(json, "premium", premium);
    readValue<std::int64_t>(json, "time", time);
}

nlohmann::json PerpAsset::toJson() const { throw std::runtime_error("Unimplemented: PerpAsset::toJson()"); }

void PerpAsset::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "name", name);
    readValue<int>(json, "szDecimals", szDecimals);
    readValue<int>(json, "maxLeverage", maxLeverage);
    readValue<bool>(json, "isDelisted", isDelisted);
}
nlohmann::json AssetContext::toJson() const { throw std::runtime_error("Unimplemented: AssetContext::toJson()"); }

void AssetContext::fromJson(const nlohmann::json& json) {
    // Merged by the REST client from meta.universe[i] + assetCtxs[i]; this
    // parses the UNION of both objects' fields (numbers arrive as strings).
    readValue<std::string>(json, "name", coin);
    readValue<int>(json, "szDecimals", szDecimals);
    readValue<bool>(json, "isDelisted", isDelisted);
    funding   = readStringAsDouble(json, "funding", funding);
    markPx    = readStringAsDouble(json, "markPx", markPx);
    midPx     = readStringAsDouble(json, "midPx", midPx);
    oraclePx  = readStringAsDouble(json, "oraclePx", oraclePx);
    dayNtlVlm = readStringAsDouble(json, "dayNtlVlm", dayNtlVlm);
}

nlohmann::json UserFill::toJson() const { throw std::runtime_error("Unimplemented: UserFill::toJson()"); }

void UserFill::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "coin", coin);
    px = readStringAsDouble(json, "px", px);
    sz = readStringAsDouble(json, "sz", sz);
    readValue<std::int64_t>(json, "time", time);
    readValue<std::uint64_t>(json, "oid", oid);
    readValue<std::uint64_t>(json, "tid", tid);
    readValue<std::string>(json, "cloid", cloid);
    readValue<bool>(json, "crossed", crossed);
    fee = readStringAsDouble(json, "fee", fee);
}

nlohmann::json AccountPosition::toJson() const { throw std::runtime_error("Unimplemented: AccountPosition::toJson()"); }

void AccountPosition::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "coin", coin);
    signedSize    = readStringAsDouble(json, "szi", signedSize);
    entryPx       = readStringAsDouble(json, "entryPx", entryPx);
    positionValue = readStringAsDouble(json, "positionValue", positionValue);
    unrealizedPnl = readStringAsDouble(json, "unrealizedPnl", unrealizedPnl);
}

nlohmann::json ClearinghouseState::toJson() const { throw std::runtime_error("Unimplemented: ClearinghouseState::toJson()"); }

void ClearinghouseState::fromJson(const nlohmann::json& json) {
    if (const auto it = json.find("marginSummary"); it != json.end() && it->is_object()) {
        accountValue   = readStringAsDouble(*it, "accountValue", accountValue);
        totalMarginUsed = readStringAsDouble(*it, "totalMarginUsed", totalMarginUsed);
    }

    withdrawable = readStringAsDouble(json, "withdrawable", withdrawable);

    if (const auto it = json.find("assetPositions"); it != json.end() && it->is_array()) {
        for (const auto& el : *it) {
            if (const auto pos = el.find("position"); pos != el.end() && pos->is_object()) {
                AccountPosition position;
                position.fromJson(*pos);
                positions.push_back(position);
            }
        }
    }
}
} // namespace stonky::hyperliquid

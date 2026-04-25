/**
Hyperliquid REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_rest_client.h"
#include "stonky/hyperliquid/hyperliquid_http_session.h"
#include "stonky/hyperliquid/hyperliquid.h"
#include <spdlog/spdlog.h>

namespace stonky::hyperliquid {

struct RESTClient::P {
    std::shared_ptr<HTTPSession> httpSession;

    http::response<http::string_body> checkResponse(const http::response<http::string_body>& response) {
        if (response.result() != http::status::ok) {
            throw std::runtime_error(fmt::format("Bad response, code {}, msg: {}", response.result_int(), response.body()));
        }
        return response;
    }

    [[nodiscard]] std::vector<Candle> getCandles(const std::string& coin, const CandleInterval interval, const std::int64_t startTime, const std::int64_t endTime) {
        nlohmann::json body;
        body["type"] = "candleSnapshot";
        body["req"]["coin"] = coin;
        body["req"]["interval"] = magic_enum::enum_name(interval);
        body["req"]["startTime"] = startTime;
        body["req"]["endTime"] = endTime;

        const auto response = checkResponse(httpSession->post("/info", body));
        const auto json = nlohmann::json::parse(response.body());

        std::vector<Candle> candles;
        for (const auto& el: json.items()) {
            Candle candle;
            candle.fromJson(el.value());
            candles.push_back(candle);
        }
        return candles;
    }

    [[nodiscard]] std::vector<FundingRate> getFundingRates(const std::string& coin, const std::int64_t startTime, const std::int64_t endTime) {
        nlohmann::json body;
        body["type"] = "fundingHistory";
        body["req"]["coin"] = coin;
        body["req"]["startTime"] = startTime;
        body["req"]["endTime"] = endTime;

        const auto response = checkResponse(httpSession->post("/info", body));
        const auto json = nlohmann::json::parse(response.body());

        std::vector<FundingRate> rates;
        for (const auto& el: json.items()) {
            FundingRate fr;
            fr.fromJson(el.value());
            rates.push_back(fr);
        }
        return rates;
    }

    [[nodiscard]] std::vector<PerpAsset> getPerpetualAssets() {
        nlohmann::json body;
        body["type"] = "meta";

        const auto response = checkResponse(httpSession->post("/info", body));
        const auto json = nlohmann::json::parse(response.body());

        std::vector<PerpAsset> assets;
        for (const auto& el : json.at("universe").items()) {
            PerpAsset asset;
            asset.fromJson(el.value());
            assets.push_back(asset);
        }
        return assets;
    }

};

RESTClient::RESTClient() : m_p(std::make_unique<P>()) { m_p->httpSession = std::make_shared<HTTPSession>(); }

RESTClient::~RESTClient() = default;

std::vector<Candle> RESTClient::getHistoricalPrices(const std::string& coin, const CandleInterval interval, std::int64_t from, const std::int64_t to,
                                                    const onCandlesDownloaded& writer) const {
    const std::int64_t intervalMs = Hyperliquid::numberOfMsForCandleInterval(interval);
    // Batch in windows of at most 5000 candles to keep request sizes reasonable
    constexpr std::int64_t maxCandlesPerBatch = 5000;

    std::vector<Candle> retVal;
    std::int64_t batchFrom = from;

    while (batchFrom < to) {
        const std::int64_t batchTo = std::min(batchFrom + intervalMs * maxCandlesPerBatch, to);
        auto candles = m_p->getCandles(coin, interval, batchFrom, batchTo);

        if (candles.empty()) break;

        // Drop the last candle if it is still open (its end would exceed 'to')
        if (candles.back().startTime + intervalMs > to) {
            candles.pop_back();
        }

        if (candles.empty()) break;

        if (writer) writer(candles);

        retVal.insert(retVal.end(), candles.begin(), candles.end());
        batchFrom = candles.back().startTime + intervalMs;
    }

    return retVal;
}

std::vector<FundingRate> RESTClient::getFundingRates(const std::string& coin, const std::int64_t startTime, const std::int64_t endTime) const {
    return m_p->getFundingRates(coin, startTime, endTime);
}

std::vector<PerpAsset> RESTClient::getPerpetualAssets(const bool includeDelisted) const {
    auto assets = m_p->getPerpetualAssets();
    if (!includeDelisted) {
        std::erase_if(assets, [](const PerpAsset& a) { return a.isDelisted; });
    }
    return assets;
}
} // namespace stonky::hyperliquid

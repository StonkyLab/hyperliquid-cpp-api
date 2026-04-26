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

    // fundingHistory uses flat params (no "req" wrapper), returns max 500 records per call
    [[nodiscard]] std::vector<FundingRate> getFundingRatesPage(const std::string& coin, const std::int64_t startTime, const std::int64_t endTime) {
        nlohmann::json body;
        body["type"] = "fundingHistory";
        body["coin"] = coin;
        body["startTime"] = startTime;
        body["endTime"] = endTime;

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
    // When skipping over empty pre-listing time, jump at least this far per API call
    // so that short intervals (1m, 5m) don't make hundreds of requests scanning years
    // of missing data. Within this distance of 'to' we revert to normal batch size
    // so that recent data isn't missed regardless of interval.
    constexpr int64_t coarseSkipMs = 30LL * 24 * 3600 * 1000; // 30 days
    constexpr int64_t maxEmptySearchMs = 5LL * 365 * 24 * 3600 * 1000; // 5 years
    int64_t emptySearchedMs = 0;

    while (batchFrom < to) {
        const std::int64_t batchTo = std::min(batchFrom + intervalMs * maxCandlesPerBatch, to);
        auto candles = m_p->getCandles(coin, interval, batchFrom, batchTo);

        if (candles.empty()) {
            const int64_t remaining = to - batchFrom;
            // Switch to fine batches when within 2x coarseSkip of the end so that
            // data starting anywhere in the last coarseSkip window isn't missed.
            const int64_t skip = (remaining > 2 * coarseSkipMs)
                                     ? std::max(batchTo - batchFrom, coarseSkipMs)
                                     : batchTo - batchFrom;
            emptySearchedMs += skip;
            if (emptySearchedMs >= maxEmptySearchMs) break;
            batchFrom = std::min(batchFrom + skip, to);
            continue;
        }
        emptySearchedMs = 0;

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
    constexpr int pageLimit = 500;
    std::vector<FundingRate> retVal;
    std::int64_t pageStart = startTime;

    while (pageStart < endTime) {
        auto page = m_p->getFundingRatesPage(coin, pageStart, endTime);
        if (page.empty()) break;

        retVal.insert(retVal.end(), page.begin(), page.end());

        if (static_cast<int>(page.size()) < pageLimit) break;

        // Advance past the last received record (funding is hourly, +1ms avoids duplicate)
        pageStart = page.back().time + 1;
    }

    return retVal;
}

std::vector<PerpAsset> RESTClient::getPerpetualAssets(const bool includeDelisted) const {
    auto assets = m_p->getPerpetualAssets();
    if (!includeDelisted) {
        std::erase_if(assets, [](const PerpAsset& a) { return a.isDelisted; });
    }
    return assets;
}
} // namespace stonky::hyperliquid

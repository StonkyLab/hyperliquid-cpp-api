#include "stonky/hyperliquid/hyperliquid_rest_client.h"
#include "stonky/hyperliquid/hyperliquid.h"
#include "stonky/utils/json_utils.h"
#include "stonky/utils/utils.h"
#include <iostream>
#include <spdlog/spdlog.h>

using namespace stonky::hyperliquid;

void testCandles() {
    const auto restClient = std::make_unique<RESTClient>();

    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto fromMs = nowMs - 7 * 86400000LL; // last 7 days

    const auto candles = restClient->getHistoricalPrices("BTC", CandleInterval::_1h, fromMs, nowMs);
    spdlog::info("Downloaded {} BTC 1h candles", candles.size());

    if (!candles.empty()) {
        const auto& first = candles.front();
        spdlog::info("First candle: open={}, high={}, low={}, close={}, volume={}",
                     first.open, first.high, first.low, first.close, first.volume);
    }
}

void testFundingRates() {
    const auto restClient = std::make_unique<RESTClient>();

    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto fromMs = nowMs - 30 * 86400000LL; // last 30 days

    const auto rates = restClient->getFundingRates("BTC", fromMs, nowMs);
    spdlog::info("Downloaded {} BTC funding rates", rates.size());

    if (!rates.empty()) {
        const auto& last = rates.back();
        spdlog::info("Last funding rate: coin={}, rate={}, time={}", last.coin, last.fundingRate, last.time);
    }
}

int main() {
    try {
        testCandles();
        testFundingRates();
    } catch (const std::exception& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    return 0;
}

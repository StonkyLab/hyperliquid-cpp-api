/**
Hyperliquid REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_HYPERLIQUID_REST_CLIENT_H
#define INCLUDE_VK_HYPERLIQUID_REST_CLIENT_H

#include "vk/hyperliquid/hyperliquid_models.h"
#include <string>
#include <memory>
#include <functional>

namespace vk::hyperliquid {

using onCandlesDownloaded = std::function<void(const std::vector<Candle>&)>;

class RESTClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    RESTClient();

    ~RESTClient();

    /**
     * Download historical candles
     * @param coin e.g. "BTC", "ETH"
     * @param interval Candle interval
     * @param from timestamp in ms, must be smaller than "to"
     * @param to timestamp in ms, must be bigger than "from"
     * @param writer Optional callback invoked for each downloaded batch
     * @return vector of Candle structures, chronologically ascending
     * @throws std::exception
     * @see https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint#candle-snapshot
     */
    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(const std::string& coin, CandleInterval interval, std::int64_t from, std::int64_t to,
                        const onCandlesDownloaded& writer = {}) const;

    /**
     * Download historical funding rates
     * @param coin e.g. "BTC", "ETH"
     * @param startTime timestamp in ms (inclusive)
     * @param endTime timestamp in ms (inclusive)
     * @return vector of FundingRate structures, chronologically ascending
     * @throws std::exception
     * @see https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint#funding-history
     */
    [[nodiscard]] std::vector<FundingRate>
    getFundingRates(const std::string& coin, std::int64_t startTime, std::int64_t endTime) const;
};

} // namespace vk::hyperliquid

#endif // INCLUDE_VK_HYPERLIQUID_REST_CLIENT_H

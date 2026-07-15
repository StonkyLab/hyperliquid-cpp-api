/**
Hyperliquid REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_REST_CLIENT_H
#define INCLUDE_STONKY_HYPERLIQUID_REST_CLIENT_H

#include "stonky/hyperliquid/hyperliquid_models.h"
#include <functional>
#include <memory>
#include <string>

namespace stonky::hyperliquid {

using onCandlesDownloaded = std::function<void(const std::vector<Candle> &)>;

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
     * @see
     * https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint#candle-snapshot
     */
    [[nodiscard]] std::vector<Candle> getHistoricalPrices(const std::string &coin, CandleInterval interval, std::int64_t from, std::int64_t to,
                                                          const onCandlesDownloaded &writer = {}) const;

    /**
     * Download historical funding rates
     * @param coin e.g. "BTC", "ETH"
     * @param startTime timestamp in ms (inclusive)
     * @param endTime timestamp in ms (inclusive)
     * @return vector of FundingRate structures, chronologically ascending
     * @throws std::exception
     * @see
     * https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint#funding-history
     */
    [[nodiscard]] std::vector<FundingRate> getFundingRates(const std::string &coin, std::int64_t startTime, std::int64_t endTime) const;

    /**
     * Get all perpetual assets
     * @param includeDelisted If true, delisted assets are included in the result
     * @return vector of PerpAsset structures
     * @throws std::exception
     * @see
     * https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint#retrieve-perpetuals-asset-contexts
     */
    [[nodiscard]] std::vector<PerpAsset> getPerpetualAssets(bool includeDelisted = false) const;

    /**
     * All perp markets with their live context (metaAndAssetCtxs): current
     * hourly funding, mark/mid/oracle prices, szDecimals and — crucially — the
     * assetIndex trading actions address the coin by (its position in the FULL
     * universe, delisted entries included).
     */
    [[nodiscard]] std::vector<AssetContext> getAssetContexts() const;

    /**
     * Perp account state: equity, withdrawable and open positions.
     * @param userAddress the MASTER account address ("0x..."), not an API wallet
     */
    [[nodiscard]] ClearinghouseState getClearinghouseState(const std::string &userAddress) const;

    /**
     * The account's recent fills (most recent first, up to the venue cap of
     * ~2000). Each carries a unique tid and, when set at order time, the cloid
     * — the WS-gap fill reconcile keys on those.
     */
    [[nodiscard]] std::vector<UserFill> getUserFills(const std::string &userAddress) const;
};

} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_REST_CLIENT_H

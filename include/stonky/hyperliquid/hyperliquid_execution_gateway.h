/**
Hyperliquid Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_EXECUTION_GATEWAY_H
#define INCLUDE_STONKY_HYPERLIQUID_EXECUTION_GATEWAY_H

#include <stonky/interface/i_execution_gateway.h>
#include <memory>

namespace stonky::execution {

/**
 * IExecutionGateway adapter for the Hyperliquid L1 perpetuals DEX.
 *
 * Quotes come from the bbo WS channel; order state from orderUpdates and fills
 * from userFills (fillId = the venue's unique tid, so REST re-emission dedups).
 * Orders are EIP-712-signed actions POSTed to /exchange, addressed by cloid —
 * the venue answers synchronously (resting / filled / error), so submit-time
 * rejects classify immediately. Price/size are normalised to the venue grid
 * (5 significant figures, max 6−szDecimals decimals) inside the adapter.
 */
class HyperliquidExecutionGateway final : public IExecutionGateway {
    struct P;
    std::unique_ptr<P> m_p;

public:
    /**
     * @param privateKeyHex API/agent wallet (or master) private key — signs orders
     * @param accountAddress the MASTER account address for state/event queries;
     *        pass empty when the private key IS the master account's
     * @param isMainnet mainnet vs testnet endpoints + signing source
     */
    HyperliquidExecutionGateway(const std::string &privateKeyHex, const std::string &accountAddress, bool isMainnet = true);

    ~HyperliquidExecutionGateway() override;

    [[nodiscard]] std::string name() const override;

    void start() override;

    InstrumentSpec instrumentSpec(const std::string &symbol) override;

    void refreshInstruments() override;

    void subscribeQuotes(const std::string &symbol) override;

    void unsubscribeQuotes(const std::string &symbol) override;

    std::optional<Quote> lastQuote(const std::string &symbol) override;

    void setOrderUpdateCallback(const onOrderUpdateEvent &cb) override;

    void setFillCallback(const onFillEvent &cb) override;

    void setQuoteCallback(const onQuoteEvent &cb) override;

    void submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty, double price, bool reduceOnly) override;

    [[nodiscard]] bool supportsAmend() const override;

    void amendPrice(const std::string &clientOrderId, const std::string &symbol, double price) override;

    bool cancel(const std::string &clientOrderId, const std::string &symbol) override;

    void submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty) override;
};

} // namespace stonky::execution

#endif // INCLUDE_STONKY_HYPERLIQUID_EXECUTION_GATEWAY_H

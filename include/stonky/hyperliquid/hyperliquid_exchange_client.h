/**
Hyperliquid Exchange Client

Signed trading actions against POST /exchange: post-only/IOC limit orders
addressed by client order id (cloid) and cancels by cloid. Every action is
EIP-712 signed by L1Signer with a strictly increasing millisecond nonce.
The venue responds SYNCHRONOUSLY with the order outcome (resting / filled /
error) — unlike the CEX adapters there is no ack-then-event gap on submit.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_EXCHANGE_CLIENT_H
#define INCLUDE_STONKY_HYPERLIQUID_EXCHANGE_CLIENT_H

#include <cstdint>
#include <memory>
#include <string>

namespace stonky::hyperliquid {

/// Outcome of one order/cancel action (the venue's synchronous verdict).
struct ActionStatus {
    enum class Kind {
        Resting, ///< order accepted onto the book (oid set)
        Filled,  ///< order matched immediately (filledSz/avgPx/oid set)
        Success, ///< cancel accepted
        Error,   ///< venue rejected the action (error set)
    };

    Kind kind{Kind::Error};
    std::uint64_t oid{};
    double filledSz{};
    double avgPx{};
    std::string error;
};

/// Time-in-force for placeLimit.
enum class Tif { Alo, Gtc, Ioc };

class ExchangeClient {
    struct P;
    std::unique_ptr<P> m_p;

public:
    /**
     * @param privateKeyHex API/agent wallet (or master) secp256k1 private key
     * @param isMainnet mainnet vs testnet (selects signing source; the host
     *        should match — pass the testnet host when isMainnet is false)
     * @param host override, defaults to api.hyperliquid.xyz
     */
    ExchangeClient(const std::string &privateKeyHex, bool isMainnet, const std::string &host = "");

    ~ExchangeClient();

    /**
     * Place a single limit order.
     * @param asset the coin's index in the meta universe
     * @param pxWire price in wire format (minimal decimal string, pre-rounded
     *        to the venue grid — use floatToWire)
     * @param szWire size in wire format
     * @param cloid client order id: "0x" + 32 hex chars, unique per order
     * @throws std::runtime_error on transport failure (the venue's business
     *         rejections come back as Kind::Error, not exceptions)
     */
    [[nodiscard]] ActionStatus placeLimit(int asset, bool isBuy, const std::string &pxWire, const std::string &szWire, bool reduceOnly, Tif tif, const std::string &cloid) const;

    /// Cancel one order by its cloid.
    [[nodiscard]] ActionStatus cancelByCloid(int asset, const std::string &cloid) const;

    /// Address of the signing key (agent or master).
    [[nodiscard]] std::string signerAddress() const;
};

/**
 * Format a price/size for the wire: minimal decimal string (no exponent, no
 * trailing zeros), the reference SDK's float_to_wire. The value must be exactly
 * representable within 8 decimals — callers pass grid-snapped values.
 */
[[nodiscard]] std::string floatToWire(double value);

} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_EXCHANGE_CLIENT_H

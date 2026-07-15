/**
Hyperliquid L1 Action Signer

Hyperliquid exchange actions are authenticated with an EIP-712 signature over a
"phantom agent": connectionId = keccak256(msgpack(action) + nonce_be64 + 0x00),
signed as Agent{source: "a"|"b", connectionId} under the constant Exchange/1/
1337/0x0 domain (the scheme of the official SDKs). The msgpack BYTE ORDER is
part of the signature, so actions are built as nlohmann::ordered_json — field
insertion order must match the reference SDK exactly.

Implemented with OpenSSL's KECCAK-256 (the Ethereum variant, not SHA3) and
libsecp256k1 recoverable ECDSA. Validated bit-for-bit against the official
hyperliquid-python-sdk signing test vectors (see test/signing_vectors.cpp).

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_SIGNER_H
#define INCLUDE_STONKY_HYPERLIQUID_SIGNER_H

#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace stonky::hyperliquid {

/// An Ethereum-style recoverable signature in the wire format /exchange expects.
/// r and s are minimal hex ("0x" + no leading zeros — the reference SDK's
/// encoding); v is 27 or 28.
struct EthSignature {
    std::string r;
    std::string s;
    int v{};
};

/**
 * Signs Hyperliquid L1 actions with one secp256k1 private key (an API/agent
 * wallet or the master wallet). Stateless per call and thread-safe.
 */
class L1Signer {
    struct P;
    std::unique_ptr<P> m_p;

public:
    /**
     * @param privateKeyHex 32-byte secp256k1 private key, hex (0x prefix optional)
     * @param isMainnet selects the phantom-agent source ("a" mainnet / "b" testnet)
     * @throws std::runtime_error on an invalid key
     */
    L1Signer(const std::string &privateKeyHex, bool isMainnet);

    ~L1Signer();

    /**
     * Sign an L1 action (order, cancel, ...). The action must be built with the
     * exact field order of the reference SDK — it is msgpack-serialised as-is.
     * No vault address and no expiresAfter (both unused by this bot).
     * @param action ordered action JSON
     * @param nonce the same nonce that will be POSTed alongside the action (ms)
     */
    [[nodiscard]] EthSignature signL1Action(const nlohmann::ordered_json &action, std::uint64_t nonce) const;

    /// The Ethereum address derived from the signing key ("0x" + 40 lowercase
    /// hex) — the venue must have it registered (API wallet) or it must be the
    /// master account.
    [[nodiscard]] std::string address() const;
};

/// Ethereum-variant keccak256 (exposed for tests).
[[nodiscard]] std::array<unsigned char, 32> keccak256(const unsigned char *data, std::size_t size);

} // namespace stonky::hyperliquid

#endif // INCLUDE_STONKY_HYPERLIQUID_SIGNER_H

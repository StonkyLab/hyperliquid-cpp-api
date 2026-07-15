/**
Hyperliquid L1 Action Signer

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/hyperliquid/hyperliquid_signer.h"
#include <openssl/evp.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <fmt/format.h>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace stonky::hyperliquid {
namespace {
/// Hex string (0x optional) → bytes. Throws on odd length / non-hex.
std::vector<unsigned char> fromHex(std::string hex) {
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex = hex.substr(2);
    }

    if (hex.size() % 2 != 0) {
        throw std::runtime_error("hex string has odd length");
    }

    std::vector<unsigned char> out(hex.size() / 2);

    for (std::size_t i = 0; i < out.size(); ++i) {
        const auto nibble = [](const char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            throw std::runtime_error("invalid hex character");
        };
        out[i] = static_cast<unsigned char>(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
    }

    return out;
}

/// 32-byte big-endian word → minimal "0x..." hex (no leading zeros — the
/// reference SDK encodes r/s with eth_utils to_hex(int), which is minimal).
std::string toMinimalHex(const unsigned char *word32) {
    std::size_t start = 0;
    while (start < 31 && word32[start] == 0) {
        ++start;
    }

    std::string out = "0x";
    bool first = true;

    for (std::size_t i = start; i < 32; ++i) {
        if (first) {
            /// The leading byte drops its high nibble when zero.
            if (word32[i] >> 4 != 0) {
                out += fmt::format("{:x}", word32[i] >> 4);
            }
            out += fmt::format("{:x}", word32[i] & 0xF);
            first = false;
        } else {
            out += fmt::format("{:02x}", word32[i]);
        }
    }

    return out;
}

std::array<unsigned char, 32> keccak(const std::vector<unsigned char> &data) { return keccak256(data.data(), data.size()); }

std::array<unsigned char, 32> keccak(const std::string &s) { return keccak256(reinterpret_cast<const unsigned char *>(s.data()), s.size()); }

void append(std::vector<unsigned char> &buf, const std::array<unsigned char, 32> &word) { buf.insert(buf.end(), word.begin(), word.end()); }

/// EIP-712 domain separator for Hyperliquid's constant Exchange domain
/// (identical on mainnet and testnet — the network lives in Agent.source).
std::array<unsigned char, 32> exchangeDomainSeparator() {
    std::vector<unsigned char> buf;
    append(buf, keccak(std::string("EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)")));
    append(buf, keccak(std::string("Exchange")));
    append(buf, keccak(std::string("1")));

    std::array<unsigned char, 32> chainId{};
    chainId[30] = 0x05; /// 1337 = 0x0539
    chainId[31] = 0x39;
    append(buf, chainId);

    append(buf, std::array<unsigned char, 32>{}); /// verifyingContract = 0x0
    return keccak(buf);
}
} // namespace

std::array<unsigned char, 32> keccak256(const unsigned char *data, const std::size_t size) {
    /// Ethereum keccak256 — OpenSSL ≥3.2 exposes it as "KECCAK-256" (distinct
    /// from SHA3-256, which uses the NIST padding and yields different digests).
    static EVP_MD *md = EVP_MD_fetch(nullptr, "KECCAK-256", nullptr);

    if (md == nullptr) {
        throw std::runtime_error("OpenSSL KECCAK-256 digest unavailable (needs OpenSSL >= 3.2)");
    }

    std::array<unsigned char, 32> out{};
    unsigned int outLen = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 || EVP_DigestUpdate(ctx, data, size) != 1 || EVP_DigestFinal_ex(ctx, out.data(), &outLen) != 1 || outLen != 32) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("keccak256 failed");
    }

    EVP_MD_CTX_free(ctx);
    return out;
}

struct L1Signer::P {
    secp256k1_context *ctx{nullptr};
    std::array<unsigned char, 32> privateKey{};
    std::string source; /// "a" mainnet, "b" testnet
    std::string address;

    ~P() {
        if (ctx != nullptr) {
            secp256k1_context_destroy(ctx);
        }

        /// Best effort: don't leave key material lying around.
        std::memset(privateKey.data(), 0, privateKey.size());
    }
};

L1Signer::L1Signer(const std::string &privateKeyHex, const bool isMainnet) : m_p(std::make_unique<P>()) {
    const auto key = fromHex(privateKeyHex);

    if (key.size() != 32) {
        throw std::runtime_error("Hyperliquid signer: private key must be 32 bytes");
    }

    std::memcpy(m_p->privateKey.data(), key.data(), 32);
    m_p->source = isMainnet ? "a" : "b";
    m_p->ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    if (m_p->ctx == nullptr || secp256k1_ec_seckey_verify(m_p->ctx, m_p->privateKey.data()) != 1) {
        throw std::runtime_error("Hyperliquid signer: invalid secp256k1 private key");
    }

    /// Derive the Ethereum address: keccak256(uncompressed pubkey minus the
    /// 0x04 prefix), last 20 bytes.
    secp256k1_pubkey pubkey;

    if (secp256k1_ec_pubkey_create(m_p->ctx, &pubkey, m_p->privateKey.data()) != 1) {
        throw std::runtime_error("Hyperliquid signer: pubkey derivation failed");
    }

    std::array<unsigned char, 65> uncompressed{};
    std::size_t len = uncompressed.size();
    secp256k1_ec_pubkey_serialize(m_p->ctx, uncompressed.data(), &len, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    const auto hash = keccak256(uncompressed.data() + 1, 64);
    std::string addr = "0x";

    for (std::size_t i = 12; i < 32; ++i) {
        addr += fmt::format("{:02x}", hash[i]);
    }

    m_p->address = addr;
}

L1Signer::~L1Signer() = default;

std::string L1Signer::address() const { return m_p->address; }

EthSignature L1Signer::signL1Action(const nlohmann::ordered_json &action, const std::uint64_t nonce) const {
    /// connectionId = keccak(msgpack(action) + nonce_be64 + 0x00-no-vault).
    /// ordered_json's to_msgpack walks fields in insertion order, matching the
    /// reference SDK's dict serialisation (confirmed by the SDK test vectors).
    std::vector<unsigned char> data = nlohmann::ordered_json::to_msgpack(action);

    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<unsigned char>(nonce >> shift & 0xFF));
    }

    data.push_back(0x00);
    const auto connectionId = keccak(data);

    /// EIP-712 digest over Agent{source, connectionId}.
    static const std::array<unsigned char, 32> domainSeparator = exchangeDomainSeparator();
    static const std::array<unsigned char, 32> agentTypeHash = keccak(std::string("Agent(string source,bytes32 connectionId)"));

    std::vector<unsigned char> structBuf;
    append(structBuf, agentTypeHash);
    append(structBuf, keccak(m_p->source));
    append(structBuf, connectionId);
    const auto structHash = keccak(structBuf);

    std::vector<unsigned char> digestBuf{0x19, 0x01};
    append(digestBuf, domainSeparator);
    append(digestBuf, structHash);
    const auto digest = keccak(digestBuf);

    /// Recoverable ECDSA (libsecp256k1 is deterministic RFC6979 and low-s).
    secp256k1_ecdsa_recoverable_signature signature;

    if (secp256k1_ecdsa_sign_recoverable(m_p->ctx, &signature, digest.data(), m_p->privateKey.data(), nullptr, nullptr) != 1) {
        throw std::runtime_error("Hyperliquid signer: signing failed");
    }

    std::array<unsigned char, 64> compact{};
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(m_p->ctx, compact.data(), &recid, &signature);

    EthSignature out;
    out.r = toMinimalHex(compact.data());
    out.s = toMinimalHex(compact.data() + 32);
    out.v = 27 + recid;
    return out;
}

} // namespace stonky::hyperliquid

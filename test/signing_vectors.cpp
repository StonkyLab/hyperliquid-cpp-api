/**
Signing vector test — validates the C++ L1 signer bit-for-bit against the
official hyperliquid-python-sdk test vectors (tests/signing_test.py:
test_l1_action_signing_order_matches and ..._order_with_cloid_matches).
No network, no credentials; exits non-zero on any mismatch.
*/

#include "stonky/hyperliquid/hyperliquid_signer.h"
#include <spdlog/spdlog.h>

using namespace stonky::hyperliquid;

namespace {
constexpr auto TEST_KEY = "0x0123456789012345678901234567890123456789012345678901234567890123";
int failures = 0;

nlohmann::ordered_json orderAction(const bool withCloid) {
    nlohmann::ordered_json order;
    order["a"] = 1;
    order["b"] = true;
    order["p"] = "100";
    order["s"] = "100";
    order["r"] = false;
    order["t"] = nlohmann::ordered_json{{"limit", nlohmann::ordered_json{{"tif", "Gtc"}}}};

    if (withCloid) {
        order["c"] = "0x00000000000000000000000000000001";
    }

    nlohmann::ordered_json action;
    action["type"] = "order";
    action["orders"] = nlohmann::ordered_json::array({order});
    action["grouping"] = "na";
    return action;
}

void expect(const char *label, const EthSignature &got, const std::string &r, const std::string &s, const int v) {
    const bool ok = got.r == r && got.s == s && got.v == v;

    if (ok) {
        spdlog::info("OK   {}", label);
    } else {
        spdlog::error("FAIL {}\n  got      r={} s={} v={}\n  expected r={} s={} v={}", label, got.r, got.s, got.v, r, s, v);
        ++failures;
    }
}
} // namespace

int main() {
    try {
        const L1Signer mainnet(TEST_KEY, true);
        const L1Signer testnet(TEST_KEY, false);

        spdlog::info("signer address: {}", mainnet.address());

        const auto plain = orderAction(false);
        expect("order mainnet", mainnet.signL1Action(plain, 0), "0xd65369825a9df5d80099e513cce430311d7d26ddf477f5b3a33d2806b100d78e",
               "0x2b54116ff64054968aa237c20ca9ff68000f977c93289157748a3162b6ea940e", 28);
        expect("order testnet", testnet.signL1Action(plain, 0), "0x82b2ba28e76b3d761093aaded1b1cdad4960b3af30212b343fb2e6cdfa4e3d54",
               "0x6b53878fc99d26047f4d7e8c90eb98955a109f44209163f52d8dc4278cbbd9f5", 27);

        const auto withCloid = orderAction(true);
        expect("order+cloid mainnet", mainnet.signL1Action(withCloid, 0), "0x41ae18e8239a56cacbc5dad94d45d0b747e5da11ad564077fcac71277a946e3",
               "0x3c61f667e747404fe7eea8f90ab0e76cc12ce60270438b2058324681a00116da", 27);
        expect("order+cloid testnet", testnet.signL1Action(withCloid, 0), "0xeba0664bed2676fc4e5a743bf89e5c7501aa6d870bdb9446e122c9466c5cd16d",
               "0x7f3e74825c9114bc59086f1eebea2928c190fdfbfde144827cb02b85bbe90988", 28);
    } catch (std::exception &e) {
        spdlog::critical("signing vectors exception: {}", e.what());
        return 1;
    }

    if (failures > 0) {
        spdlog::critical("{} vector(s) FAILED — the signer must not be used live", failures);
        return 1;
    }

    spdlog::info("all signing vectors match the official SDK");
    return 0;
}

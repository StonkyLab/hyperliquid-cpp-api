/**
Read-only Hyperliquid probe: asset contexts + funding ranking (REST) and a live
BBO over the WebSocket. No account/credentials — validates the market-data +
WS transport the execution gateway depends on.
*/

#include "stonky/hyperliquid/hyperliquid_rest_client.h"
#include "stonky/hyperliquid/hyperliquid_ws_stream_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

using namespace stonky::hyperliquid;

int main() {
    spdlog::set_level(spdlog::level::info);

    try {
        const RESTClient rest;

        auto contexts = rest.getAssetContexts();
        spdlog::info("asset contexts: {}", contexts.size());

        for (const auto& ctx: contexts) {
            if (ctx.coin == "ETH") {
                spdlog::info("  ETH assetIndex={} szDecimals={} funding={:+.6f}%/h mid={}", ctx.assetIndex, ctx.szDecimals, ctx.funding * 100.0, ctx.midPx);
            }
        }

        std::erase_if(contexts, [](const AssetContext& c) { return c.isDelisted || c.funding == 0.0; });
        std::ranges::sort(contexts, [](const auto& a, const auto& b) { return a.funding < b.funding; });
        spdlog::info("funded candidates: {}", contexts.size());
        spdlog::info("--- most NEGATIVE hourly funding (would LONG) ---");
        for (std::size_t i = 0; i < std::min<std::size_t>(5, contexts.size()); ++i) {
            spdlog::info("  {} {:+.5f}%", contexts[i].coin, contexts[i].funding * 100.0);
        }
        spdlog::info("--- most POSITIVE (would SHORT) ---");
        for (std::size_t i = 0; i < std::min<std::size_t>(5, contexts.size()); ++i) {
            const auto& e = contexts[contexts.size() - 1 - i];
            spdlog::info("  {} {:+.5f}%", e.coin, e.funding * 100.0);
        }

        spdlog::info("=== WS BBO probe: ETH ===");
        WSStreamManager stream;
        std::atomic<int> ticks{0};
        stream.setBboCallback([&](const std::string& coin, const double bid, const double ask) {
            if (ticks.fetch_add(1) < 3) spdlog::info("  BBO {} bid={} ask={}", coin, bid, ask);
        });
        stream.start();
        stream.subscribeBbo("ETH");

        for (int i = 0; i < 100 && ticks.load() < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("BBO ticks received: {}", ticks.load());
        return ticks.load() > 0 ? 0 : 1;
    } catch (std::exception& e) {
        spdlog::critical("probe exception: {}", e.what());
        return 1;
    }
}

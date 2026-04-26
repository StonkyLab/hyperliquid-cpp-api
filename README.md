# hyperliquid_cpp_api

C++ connector library for the Hyperliquid decentralised perpetuals exchange.

## Features

- REST API client for perpetual futures market data
- Historical candlestick data download with automatic pagination
- Full funding rate history download (from May 2023)
- Automatic discovery of all perpetual assets including delisted ones

## Requirements

- C++20 compiler
- CMake 4.0+
- Boost 1.88+ (ASIO, Beast)
- OpenSSL
- nlohmann_json
- spdlog
- magic_enum

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage Examples

### REST Client — Perpetual Assets

```cpp
#include "stonky/hyperliquid/hyperliquid_rest_client.h"

using namespace stonky::hyperliquid;

RESTClient client;

// Get all active perpetual assets
auto assets = client.getPerpetualAssets();

// Include delisted assets
auto allAssets = client.getPerpetualAssets(true);

for (const auto& asset : assets) {
    std::cout << asset.name << " (max leverage: " << asset.maxLeverage << ")" << std::endl;
}
```

### Historical Candlestick Data

```cpp
#include "stonky/hyperliquid/hyperliquid_rest_client.h"

using namespace stonky::hyperliquid;

RESTClient client;

// Download historical candles with automatic pagination
auto candles = client.getHistoricalPrices(
    "BTC",
    CandleInterval::_1h,
    fromTimestamp,   // ms
    toTimestamp);    // ms

std::cout << "Downloaded " << candles.size() << " candles" << std::endl;

// With callback for streaming large datasets
client.getHistoricalPrices(
    "ETH",
    CandleInterval::_1h,
    fromTimestamp,
    toTimestamp,
    [](const std::vector<Candle>& batch) {
        for (const auto& candle : batch) {
            std::cout << "Time: " << candle.startTime
                      << " O: " << candle.open
                      << " H: " << candle.high
                      << " L: " << candle.low
                      << " C: " << candle.close
                      << " V: " << candle.volume << std::endl;
        }
    });
```

### Funding Rate History

```cpp
auto fundingRates = client.getFundingRates(
    "BTC",
    startTimestamp,
    endTimestamp);

for (const auto& fr : fundingRates) {
    std::cout << "Time: " << fr.time
              << " Rate: " << fr.fundingRate
              << " Premium: " << fr.premium << std::endl;
}
```

## Candle Intervals

The API returns **at most 5 000 candles** per symbol. Available history is therefore
`5 000 × interval duration` rolling back from the current time. For long intervals this
window exceeds the exchange's lifetime and the practical limit becomes the exchange launch
date (real trading began in **March 2023**; daily candles before February 2023 carry zero
volume — synthetic oracle prices only).

Supported intervals:

| Interval   | Enum Value              | 5 000 candles ≈         |
|------------|-------------------------|-------------------------|
| 1 minute   | `CandleInterval::_1m`  | 3.5 days                |
| 3 minutes  | `CandleInterval::_3m`  | 10 days                 |
| 5 minutes  | `CandleInterval::_5m`  | 17 days                 |
| 15 minutes | `CandleInterval::_15m` | 52 days                 |
| 30 minutes | `CandleInterval::_30m` | 104 days                |
| 1 hour     | `CandleInterval::_1h`  | 208 days                |
| 2 hours    | `CandleInterval::_2h`  | 417 days                |
| 4 hours    | `CandleInterval::_4h`  | 833 days                |
| 8 hours    | `CandleInterval::_8h`  | full history            |
| 12 hours   | `CandleInterval::_12h` | full history            |
| 1 day      | `CandleInterval::_1d`  | full history            |
| 3 days     | `CandleInterval::_3d`  | full history            |
| 1 week     | `CandleInterval::_1w`  | full history            |
| 1 month    | `CandleInterval::_1M`  | full history            |

## Data Models

### Candle
```cpp
struct Candle {
    std::int64_t startTime;  // open time (ms)
    std::int64_t closeTime;  // close time (ms)
    std::string coin;        // e.g. "BTC"
    std::string interval;    // e.g. "1h"
    double open;
    double close;
    double high;
    double low;
    double volume;
    int numTrades;
};
```

### FundingRate
```cpp
struct FundingRate {
    std::string coin;
    double fundingRate;
    double premium;
    std::int64_t time;  // ms
};
```

### PerpAsset
```cpp
struct PerpAsset {
    std::string name;    // coin name, e.g. "BTC"
    int szDecimals;      // size decimal places
    int maxLeverage;
    bool isDelisted;
};
```

## Project Structure

```
hyperliquid-cpp-api/
├── include/stonky/hyperliquid/
│   ├── hyperliquid_rest_client.h   # REST API client
│   ├── hyperliquid_http_session.h  # HTTPS session (PIMPL)
│   ├── hyperliquid_models.h        # Data models
│   ├── hyperliquid_enums.h         # Enumerations
│   └── hyperliquid.h               # Utility functions
├── src/
│   ├── hyperliquid_rest_client.cpp
│   ├── hyperliquid_http_session.cpp
│   ├── hyperliquid_models.cpp
│   └── hyperliquid.cpp
├── stonky-cpp-common/              # Common utilities submodule
└── test/
    └── main.cpp
```

## API Documentation

Hyperliquid info endpoint reference: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint

## License

MIT License - see source files for details.

## Author

Vitezslav Kot <vitezslav.kot@stonky.cz>

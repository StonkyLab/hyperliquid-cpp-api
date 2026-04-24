/**
Hyperliquid Enums

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_HYPERLIQUID_ENUMS_H
#define INCLUDE_STONKY_HYPERLIQUID_ENUMS_H

#include "stonky/utils/magic_enum_wrapper.hpp"

namespace stonky::hyperliquid {
enum class CandleInterval : std::int32_t {
    _1m,
    _3m,
    _5m,
    _15m,
    _30m,
    _1h,
    _2h,
    _4h,
    _6h,
    _8h,
    _12h,
    _1d,
    _3d,
    _1w,
    _1M
};
}

template <>
constexpr magic_enum::customize::customize_t magic_enum::customize::enum_name<stonky::hyperliquid::CandleInterval>(
    const stonky::hyperliquid::CandleInterval value) noexcept {
    switch (value) {
    case stonky::hyperliquid::CandleInterval::_1m:
        return "1m";
    case stonky::hyperliquid::CandleInterval::_3m:
        return "3m";
    case stonky::hyperliquid::CandleInterval::_5m:
        return "5m";
    case stonky::hyperliquid::CandleInterval::_15m:
        return "15m";
    case stonky::hyperliquid::CandleInterval::_30m:
        return "30m";
    case stonky::hyperliquid::CandleInterval::_1h:
        return "1h";
    case stonky::hyperliquid::CandleInterval::_2h:
        return "2h";
    case stonky::hyperliquid::CandleInterval::_4h:
        return "4h";
    case stonky::hyperliquid::CandleInterval::_6h:
        return "6h";
    case stonky::hyperliquid::CandleInterval::_8h:
        return "8h";
    case stonky::hyperliquid::CandleInterval::_12h:
        return "12h";
    case stonky::hyperliquid::CandleInterval::_1d:
        return "1d";
    case stonky::hyperliquid::CandleInterval::_3d:
        return "3d";
    case stonky::hyperliquid::CandleInterval::_1w:
        return "1w";
    case stonky::hyperliquid::CandleInterval::_1M:
        return "1M";
    }
    return default_tag;
}

#endif // INCLUDE_STONKY_HYPERLIQUID_ENUMS_H

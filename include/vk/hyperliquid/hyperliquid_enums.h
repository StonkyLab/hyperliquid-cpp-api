/**
Hyperliquid Enums

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_HYPERLIQUID_ENUMS_H
#define INCLUDE_VK_HYPERLIQUID_ENUMS_H

#include "vk/utils/magic_enum_wrapper.hpp"

namespace vk::hyperliquid {
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
constexpr magic_enum::customize::customize_t magic_enum::customize::enum_name<vk::hyperliquid::CandleInterval>(
    const vk::hyperliquid::CandleInterval value) noexcept {
    switch (value) {
    case vk::hyperliquid::CandleInterval::_1m:
        return "1m";
    case vk::hyperliquid::CandleInterval::_3m:
        return "3m";
    case vk::hyperliquid::CandleInterval::_5m:
        return "5m";
    case vk::hyperliquid::CandleInterval::_15m:
        return "15m";
    case vk::hyperliquid::CandleInterval::_30m:
        return "30m";
    case vk::hyperliquid::CandleInterval::_1h:
        return "1h";
    case vk::hyperliquid::CandleInterval::_2h:
        return "2h";
    case vk::hyperliquid::CandleInterval::_4h:
        return "4h";
    case vk::hyperliquid::CandleInterval::_6h:
        return "6h";
    case vk::hyperliquid::CandleInterval::_8h:
        return "8h";
    case vk::hyperliquid::CandleInterval::_12h:
        return "12h";
    case vk::hyperliquid::CandleInterval::_1d:
        return "1d";
    case vk::hyperliquid::CandleInterval::_3d:
        return "3d";
    case vk::hyperliquid::CandleInterval::_1w:
        return "1w";
    case vk::hyperliquid::CandleInterval::_1M:
        return "1M";
    }
    return default_tag;
}

#endif // INCLUDE_VK_HYPERLIQUID_ENUMS_H

/*
 * Copyright (c) 2025, Joe Inman
 *
 * Licensed under the MIT License.
 * You may obtain a copy of the License at:
 *     https://opensource.org/licenses/MIT
 *
 * This file is part of the EmbedDSF project.
 */

#pragma once

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <cstdint>

namespace jsi::embed_dsf
{

enum class EmbedDSFErrorType : std::uint8_t
{
    ParseError = 0,
    EmissionError,
    TypeError,
    ScalarConversionError
};

inline constexpr std::array<std::string_view, 4> k_embed_dsf_error_type_to_string = {"Parse Error",
                                                                                     "Emission Error",
                                                                                     "Type Error",
                                                                                     "Scalar Conversion Error"};

struct EmbedDSFError
{
    EmbedDSFErrorType error_{EmbedDSFErrorType::ParseError};
    std::string       message_;

    [[nodiscard]] std::string to_string() const
    {
        const auto type = k_embed_dsf_error_type_to_string.at(static_cast<std::size_t>(error_));
        if (message_.empty())
        {
            return std::string{type};
        }

        std::string result;
        result.reserve(type.size() + 2 + message_.size());
        result.append(type);
        result.append(": ");
        result.append(message_);
        return result;
    }

    explicit operator std::string() const { return to_string(); }
};

inline std::ostream& operator<<(std::ostream& os, const EmbedDSFError& err)
{
    os << err.to_string();
    return os;
}

}  // namespace jsi::embed_dsf

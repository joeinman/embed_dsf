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

#include <cstdint>
#include <expected>
#include <string>

#include "embed_dsf/error.hpp"
#include "embed_dsf/node.hpp"

namespace EmbedDSF
{

class JSONTranslator
{
public:
    [[nodiscard]] std::expected<Node, EmbedDSFError>        parse(const std::string& input) const;
    [[nodiscard]] std::expected<std::string, EmbedDSFError> emit(const Node& node) const;

private:
    [[nodiscard]] static std::expected<Node, EmbedDSFError> parse_value(const std::string& input, std::size_t& pos);
    [[nodiscard]] static std::expected<Node, EmbedDSFError> parse_object(const std::string& input, std::size_t& pos);
    [[nodiscard]] static std::expected<Node, EmbedDSFError> parse_array(const std::string& input, std::size_t& pos);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> parse_string(std::string_view input,
                                                                                std::size_t&     pos);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> parse_number(std::string_view input,
                                                                                std::size_t&     pos);
    [[nodiscard]] static std::expected<Node, EmbedDSFError> parse_boolean(std::string_view input, std::size_t& pos);
    [[nodiscard]] static std::expected<Node, EmbedDSFError> parse_null(std::string_view input, std::size_t& pos);
    static void skip_whitespace(const std::string& input, std::size_t& pos) noexcept;

    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_node(const Node&  node,
                                                                             std::int32_t indentLevel);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_scalar(const Node& node);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_sequence(const Node&  node,
                                                                                 std::int32_t indentLevel);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_mapping(const Node&  node,
                                                                                std::int32_t indentLevel);
    [[nodiscard]] static std::string                               escape_string(const std::string& str);
    [[nodiscard]] static std::string                               indent_string(std::int32_t indentLevel);
};

}  // namespace EmbedDSF

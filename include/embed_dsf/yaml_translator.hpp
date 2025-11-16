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

extern "C" {
#include <yaml.h>
}

namespace jsi::embed_dsf
{

class YAMLTranslator
{
public:
    [[nodiscard]] std::expected<Node, EmbedDSFError>        parse(const std::string& input) const noexcept;
    [[nodiscard]] std::expected<std::string, EmbedDSFError> emit(const Node& node) const;

private:
    bool        parse_node(yaml_parser_t& parser, Node& node) const noexcept;
    bool        parse_node_from_event(yaml_parser_t& parser, yaml_event_t& event, Node& node) const noexcept;
    static bool parse_scalar_event(yaml_event_t& event, Node& node) noexcept;
    static bool parse_sequence_event(yaml_parser_t&    parser,
                                     yaml_event_t&     event,
                                     Node&             node,
                                     const YAMLTranslator* self) noexcept;
    static bool parse_mapping_event(yaml_parser_t&    parser,
                                    yaml_event_t&     event,
                                    Node&             node,
                                    const YAMLTranslator* self) noexcept;

    [[nodiscard]] std::expected<std::string, EmbedDSFError> emit_node(const Node& node, std::int32_t indentLevel) const;
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_scalar(const Node& node);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_sequence(const Node&       node,
                                                                                 std::int32_t      indentLevel,
                                                                                 const YAMLTranslator* self);
    [[nodiscard]] static std::expected<std::string, EmbedDSFError> emit_mapping(const Node&       node,
                                                                                std::int32_t      indentLevel,
                                                                                const YAMLTranslator* self);

    [[nodiscard]] static std::string indent_string(std::int32_t indentLevel)
    {
        return std::string(indentLevel * 2, ' ');
    }
};

}  // namespace jsi::embed_dsf

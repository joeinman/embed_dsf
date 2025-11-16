/*
 * Copyright (c) 2025, Joe Inman
 *
 * Licensed under the MIT License.
 * You may obtain a copy of the License at:
 *     https://opensource.org/licenses/MIT
 *
 * This file is part of the EmbedDSF project.
 */

#include "embed_dsf/yaml_translator.hpp"

#include <cstring>
#include <memory>
#include <utility>

namespace
{
[[nodiscard]] std::string copy_scalar_value(const yaml_char_t* value, std::size_t length)
{
    std::string result(length, '\0');
    if (length != 0U && value != nullptr)
    {
        std::memcpy(result.data(), value, length);
    }
    return result;
}
}  // namespace

namespace jsi::embed_dsf
{

std::expected<Node, EmbedDSFError> YAMLTranslator::parse(const std::string& input) noexcept
{
    yaml_parser_t parser;
    if (yaml_parser_initialize(&parser) == 0)
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to initialize YAML parser"});
    }
    yaml_parser_set_input_string(&parser,
                                 reinterpret_cast<const unsigned char*>(
                                     input.c_str()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                 input.size());

    yaml_event_t event;
    if (yaml_parser_parse(&parser, &event) == 0)
    {
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to parse stream start"});
    }
    if (event.type != YAML_STREAM_START_EVENT)
    {
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected stream start event"});
    }
    yaml_event_delete(&event);

    if (yaml_parser_parse(&parser, &event) == 0)
    {
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to parse document start"});
    }
    if (event.type != YAML_DOCUMENT_START_EVENT)
    {
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected document start event"});
    }
    yaml_event_delete(&event);

    Node root(NodeType::Map);
    if (!parse_node(parser, root))
    {
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to parse YAML node"});
    }

    if (yaml_parser_parse(&parser, &event) == 0)
    {
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to parse document end"});
    }
    if (event.type != YAML_DOCUMENT_END_EVENT)
    {
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected document end event"});
    }
    yaml_event_delete(&event);

    if (yaml_parser_parse(&parser, &event) == 0)
    {
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Failed to parse stream end"});
    }
    if (event.type != YAML_STREAM_END_EVENT)
    {
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected stream end event"});
    }
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
    return root;
}

std::expected<std::string, EmbedDSFError> YAMLTranslator::emit(const Node& node)
{
    return emit_node(node, 0);
}

bool YAMLTranslator::parse_node(yaml_parser_t& parser, Node& node) noexcept  // NOLINT(misc-no-recursion)
{
    yaml_event_t event;
    if (yaml_parser_parse(&parser, &event) == 0)
    {
        return false;
    }
    return parse_node_from_event(parser, event, node);
}

bool YAMLTranslator::parse_node_from_event(yaml_parser_t& parser,
                                           yaml_event_t&  event,
                                           Node&          node) noexcept  // NOLINT(misc-no-recursion)
{
    switch (event.type)
    {
    case YAML_SCALAR_EVENT:
        return parse_scalar_event(event, node);
    case YAML_SEQUENCE_START_EVENT:
        return parse_sequence_event(parser, event, node);
    case YAML_MAPPING_START_EVENT:
        return parse_mapping_event(parser, event, node);
    default:
        yaml_event_delete(&event);
        return false;
    }
}

bool YAMLTranslator::parse_scalar_event(yaml_event_t& event, Node& node) noexcept
{
    const std::string scalar = copy_scalar_value(event.data.scalar.value, event.data.scalar.length);
    node                     = Node(NodeType::Scalar);
    node                     = scalar;
    yaml_event_delete(&event);
    return true;
}

bool YAMLTranslator::parse_sequence_event(yaml_parser_t& parser,
                                          yaml_event_t&  event,
                                          Node&          node) noexcept  // NOLINT(misc-no-recursion)
{
    node = Node(NodeType::Sequence);
    yaml_event_delete(&event);
    while (true)
    {
        yaml_event_t childEvent;
        if (yaml_parser_parse(&parser, &childEvent) == 0)
        {
            return false;
        }
        if (childEvent.type == YAML_SEQUENCE_END_EVENT)
        {
            yaml_event_delete(&childEvent);
            break;
        }
        Node child(NodeType::Null);
        if (!parse_node_from_event(parser, childEvent, child))
        {
            return false;
        }
        node.as_sequence().emplace_back(std::make_unique<Node>(std::move(child)));
    }
    return true;
}

bool YAMLTranslator::parse_mapping_event(yaml_parser_t& parser,
                                         yaml_event_t&  event,
                                         Node&          node) noexcept  // NOLINT(misc-no-recursion)
{
    node = Node(NodeType::Map);
    yaml_event_delete(&event);
    while (true)
    {
        yaml_event_t keyEvent;
        if (yaml_parser_parse(&parser, &keyEvent) == 0)
        {
            return false;
        }
        if (keyEvent.type == YAML_MAPPING_END_EVENT)
        {
            yaml_event_delete(&keyEvent);
            break;
        }
        Node keyNode(NodeType::Scalar);
        if (!parse_node_from_event(parser, keyEvent, keyNode))
        {
            return false;
        }
        if (!keyNode.is_scalar())
        {
            return false;
        }
        const std::string key = keyNode.as_string();
        Node              valueNode(NodeType::Null);
        if (!parse_node(parser, valueNode))
        {
            return false;
        }
        node.as_map().emplace_back(key, std::make_unique<Node>(std::move(valueNode)));
    }
    return true;
}

std::expected<std::string, EmbedDSFError> YAMLTranslator::emit_node(
    const Node&  node,
    std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    switch (node.get_type())
    {
    case NodeType::Scalar:
        return emit_scalar(node);
    case NodeType::Sequence:
        return emit_sequence(node, indentLevel);
    case NodeType::Map:
        return emit_mapping(node, indentLevel);
    case NodeType::Null:
    default:
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Invalid node type for emission"});
    }
}

std::expected<std::string, EmbedDSFError> YAMLTranslator::emit_scalar(const Node& node)
{
    if (!node.is_scalar())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Invalid scalar node"});
    }
    return node.as_string();
}

std::expected<std::string, EmbedDSFError> YAMLTranslator::emit_sequence(
    const Node&  node,
    std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    if (!node.is_sequence())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Invalid sequence node"});
    }

    std::string       output;
    const std::string indent = YAMLTranslator::indent_string(indentLevel);
    const auto&       seq    = node.as_sequence();
    for (const auto& elementPtr : seq)
    {
        auto res = elementPtr->is_scalar() ? emit_node(*elementPtr, 0) : emit_node(*elementPtr, indentLevel + 1);
        if (!res)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Failed to emit sequence element"});
        }
        if (elementPtr->is_scalar())
        {
            output += indent + "- " + res.value() + "\n";
        }
        else
        {
            output += indent + "-\n" + res.value();
        }
    }
    return output;
}

std::expected<std::string, EmbedDSFError> YAMLTranslator::emit_mapping(
    const Node&  node,
    std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    if (!node.is_map())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Invalid mapping node"});
    }

    std::string       output;
    const std::string indent = YAMLTranslator::indent_string(indentLevel);
    const auto&       map    = node.as_map();
    for (const auto& entry : map)
    {
        auto res = entry.value_->is_scalar() ? emit_node(*entry.value_, 0) : emit_node(*entry.value_, indentLevel + 1);
        if (!res)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Failed to emit mapping element"});
        }
        if (entry.value_->is_scalar())
        {
            output += indent + entry.key_ + ": " + res.value() + "\n";
        }
        else
        {
            output += indent + entry.key_ + ":\n" + res.value();
        }
    }
    return output;
}

}  // namespace jsi::embed_dsf

/*
 * Copyright (c) 2025, Joe Inman
 *
 * Licensed under the MIT License.
 * You may obtain a copy of the License at:
 *     https://opensource.org/licenses/MIT
 *
 * This file is part of the EmbedDSF project.
 */

#include "embed_dsf/json_translator.hpp"

#include <cctype>
#include <cmath>
#include <memory>
#include <string_view>
#include <utility>

namespace EmbedDSF
{
namespace
{

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

[[nodiscard]] constexpr char hex_digit(unsigned int value) noexcept
{
    return static_cast<char>((value < 10U) ? ('0' + value) : ('A' + (value - 10U)));
}

std::expected<char32_t, EmbedDSFError> parse_code_unit(std::string_view input, std::size_t& pos)
{
    if (pos + 4 > input.size())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Incomplete unicode escape"});
    }

    char32_t codepoint = 0;
    for (int i = 0; i < 4; ++i)
    {
        const int value = hex_value(input[pos]);
        ++pos;
        if (value < 0)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid hex digit in unicode escape"});
        }
        codepoint = static_cast<char32_t>((codepoint << 4U) | static_cast<char32_t>(value));
    }

    return std::expected<char32_t, EmbedDSFError>{std::in_place, codepoint};
}

void append_codepoint(std::string& output, char32_t codepoint)
{
    if (codepoint <= 0x7FU)
    {
        output.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FFU)
    {
        output.push_back(static_cast<char>(0xC0U | ((codepoint >> 6U) & 0x1FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else if (codepoint <= 0xFFFFU)
    {
        output.push_back(static_cast<char>(0xE0U | ((codepoint >> 12U) & 0x0FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0U | ((codepoint >> 18U) & 0x07U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

std::expected<char32_t, EmbedDSFError> parse_unicode_escape(std::string_view input, std::size_t& pos)
{
    auto code = parse_code_unit(input, pos);
    if (!code.has_value())
    {
        return std::unexpected(code.error());
    }

    char32_t value = code.value();
    if (value >= 0xD800 && value <= 0xDBFF)
    {
        if (pos + 2 > input.size() || input[pos] != '\\' || input[pos + 1] != 'u')
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid unicode surrogate pair"});
        }
        pos += 2;
        auto low = parse_code_unit(input, pos);
        if (!low.has_value())
        {
            return std::unexpected(low.error());
        }
        if (low.value() < 0xDC00 || low.value() > 0xDFFF)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid unicode surrogate pair"});
        }
        const auto high_surrogate = static_cast<char32_t>(value - 0xD800U);
        const auto low_surrogate  = static_cast<char32_t>(low.value() - 0xDC00U);
        value                     = static_cast<char32_t>(0x10000U + (high_surrogate << 10U) + low_surrogate);
    }
    else if (value >= 0xDC00 && value <= 0xDFFF)
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unexpected unicode low surrogate"});
    }

    return std::expected<char32_t, EmbedDSFError>{std::in_place, value};
}

std::expected<void, EmbedDSFError> append_escape_sequence(std::string_view input, std::size_t& pos, std::string& result)
{
    if (pos >= input.size())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Incomplete escape sequence"});
    }

    const char esc = input[pos];
    ++pos;

    switch (esc)
    {
    case '\"':
        result.push_back('\"');
        break;
    case '\\':
        result.push_back('\\');
        break;
    case '/':
        result.push_back('/');
        break;
    case 'b':
        result.push_back('\b');
        break;
    case 'f':
        result.push_back('\f');
        break;
    case 'n':
        result.push_back('\n');
        break;
    case 'r':
        result.push_back('\r');
        break;
    case 't':
        result.push_back('\t');
        break;
    case 'u':
    {
        auto codepoint = parse_unicode_escape(input, pos);
        if (!codepoint.has_value())
        {
            return std::unexpected(codepoint.error());
        }
        append_codepoint(result, codepoint.value());
        break;
    }
    default:
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unknown escape sequence"});
    }

    return {};
}

bool is_number_literal(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }

    std::size_t idx = 0;
    if (value[idx] == '-')
    {
        ++idx;
        if (idx == value.size())
        {
            return false;
        }
    }

    if (idx >= value.size())
    {
        return false;
    }

    if (value[idx] == '0')
    {
        ++idx;
    }
    else
    {
        if (std::isdigit(static_cast<unsigned char>(value[idx])) == 0)
        {
            return false;
        }
        while (idx < value.size() && std::isdigit(static_cast<unsigned char>(value[idx])) != 0)
        {
            ++idx;
        }
    }

    if (idx < value.size() && value[idx] == '.')
    {
        ++idx;
        if (idx == value.size() || std::isdigit(static_cast<unsigned char>(value[idx])) == 0)
        {
            return false;
        }
        while (idx < value.size() && std::isdigit(static_cast<unsigned char>(value[idx])) != 0)
        {
            ++idx;
        }
    }

    if (idx < value.size() && (value[idx] == 'e' || value[idx] == 'E'))
    {
        ++idx;
        if (idx == value.size())
        {
            return false;
        }
        if (value[idx] == '+' || value[idx] == '-')
        {
            ++idx;
            if (idx == value.size())
            {
                return false;
            }
        }
        if (std::isdigit(static_cast<unsigned char>(value[idx])) == 0)
        {
            return false;
        }
        while (idx < value.size() && std::isdigit(static_cast<unsigned char>(value[idx])) != 0)
        {
            ++idx;
        }
    }

    return idx == value.size();
}

}  // namespace

std::expected<Node, EmbedDSFError> JSONTranslator::parse(
    const std::string& input) const  // NOLINT(readability-convert-member-functions-to-static)
{
    std::size_t pos = 0;
    skip_whitespace(input, pos);
    auto document = parse_value(input, pos);
    if (!document)
    {
        return document;
    }

    skip_whitespace(input, pos);
    if (pos != input.size())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unexpected trailing data"});
    }

    return document;
}

std::expected<std::string, EmbedDSFError> JSONTranslator::emit(
    const Node& node) const  // NOLINT(readability-convert-member-functions-to-static)
{
    return emit_node(node, 0);
}

std::expected<Node, EmbedDSFError> JSONTranslator::parse_value(const std::string& input,
                                                           std::size_t&       pos)  // NOLINT(misc-no-recursion)
{
    skip_whitespace(input, pos);
    if (pos >= input.size())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unexpected end of input"});
    }

    switch (const char ch = input[pos]; ch)
    {
    case '{':
        return parse_object(input, pos);
    case '[':
        return parse_array(input, pos);
    case '"':
    {
        auto scalar = parse_string(input, pos);
        if (!scalar)
        {
            return std::unexpected(scalar.error());
        }
        Node node(NodeType::Scalar);
        node = std::move(scalar.value());
        return node;
    }
    case 't':
    case 'f':
        return parse_boolean(input, pos);
    case 'n':
        return parse_null(input, pos);
    default:
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0)
        {
            auto number = parse_number(input, pos);
            if (!number)
            {
                return std::unexpected(number.error());
            }
            Node node(NodeType::Scalar);
            node = std::move(number.value());
            return node;
        }
        break;
    }

    return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid value"});
}

std::expected<Node, EmbedDSFError> JSONTranslator::parse_object(const std::string& input,
                                                            std::size_t&       pos)  // NOLINT(misc-no-recursion)
{
    if (input[pos] != '{')
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected '{'"});
    }
    ++pos;

    Node node(NodeType::Map);
    skip_whitespace(input, pos);
    if (pos < input.size() && input[pos] == '}')
    {
        ++pos;
        return node;
    }

    while (pos < input.size())
    {
        skip_whitespace(input, pos);
        if (pos >= input.size() || input[pos] != '"')
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected string key"});
        }

        auto key = parse_string(input, pos);
        if (!key)
        {
            return std::unexpected(key.error());
        }

        skip_whitespace(input, pos);
        if (pos >= input.size() || input[pos] != ':')
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected ':' after key"});
        }
        ++pos;

        auto value = parse_value(input, pos);
        if (!value)
        {
            return value;
        }

        auto child = std::make_unique<Node>(std::move(value.value()));
        node.as_map().emplace_back(std::move(key.value()), std::move(child));

        skip_whitespace(input, pos);
        if (pos >= input.size())
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unterminated object"});
        }

        if (input[pos] == ',')
        {
            ++pos;
            continue;
        }
        if (input[pos] == '}')
        {
            ++pos;
            break;
        }

        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected ',' or '}'"});
    }

    return node;
}

std::expected<Node, EmbedDSFError> JSONTranslator::parse_array(const std::string& input,
                                                           std::size_t&       pos)  // NOLINT(misc-no-recursion)
{
    if (input[pos] != '[')
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected '['"});
    }
    ++pos;

    Node node(NodeType::Sequence);
    skip_whitespace(input, pos);
    if (pos < input.size() && input[pos] == ']')
    {
        ++pos;
        return node;
    }

    while (pos < input.size())
    {
        auto value = parse_value(input, pos);
        if (!value)
        {
            return value;
        }

        node.as_sequence().emplace_back(std::make_unique<Node>(std::move(value.value())));

        skip_whitespace(input, pos);
        if (pos >= input.size())
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unterminated array"});
        }
        if (input[pos] == ',')
        {
            ++pos;
            continue;
        }
        if (input[pos] == ']')
        {
            ++pos;
            break;
        }
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected ',' or ']'"});
    }

    return node;
}

std::expected<std::string, EmbedDSFError> JSONTranslator::parse_string(std::string_view input, std::size_t& pos)
{
    if (input[pos] != '"')
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Expected string"});
    }
    ++pos;

    std::string result;
    while (pos < input.size())
    {
        const char ch = input[pos];
        ++pos;
        if (ch == '"')
        {
            return result;
        }
        if (ch == '\\')
        {
            if (auto escape_result = append_escape_sequence(input, pos, result); !escape_result.has_value())
            {
                return std::unexpected(escape_result.error());
            }
            continue;
        }

        if (static_cast<unsigned char>(ch) < 0x20)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid control character in string"});
        }

        result.push_back(ch);
    }

    return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Unterminated string"});
}

std::expected<std::string, EmbedDSFError> JSONTranslator::parse_number(std::string_view input, std::size_t& pos)
{
    const std::size_t start = pos;
    if (input[pos] == '-')
    {
        ++pos;
    }

    if (pos >= input.size())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid number"});
    }

    if (input[pos] == '0')
    {
        ++pos;
    }
    else
    {
        if (std::isdigit(static_cast<unsigned char>(input[pos])) == 0)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid number"});
        }
        while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos])) != 0)
        {
            ++pos;
        }
    }

    if (pos < input.size() && input[pos] == '.')
    {
        ++pos;
        if (pos == input.size() || std::isdigit(static_cast<unsigned char>(input[pos])) == 0)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid fraction"});
        }
        while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos])) != 0)
        {
            ++pos;
        }
    }

    if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E'))
    {
        ++pos;
        if (pos == input.size())
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid exponent"});
        }
        if (input[pos] == '+' || input[pos] == '-')
        {
            ++pos;
            if (pos == input.size())
            {
                return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid exponent"});
            }
        }
        if (std::isdigit(static_cast<unsigned char>(input[pos])) == 0)
        {
            return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid exponent"});
        }
        while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos])) != 0)
        {
            ++pos;
        }
    }

    return std::string(input.substr(start, pos - start));
}

std::expected<Node, EmbedDSFError> JSONTranslator::parse_boolean(std::string_view input, std::size_t& pos)
{
    if (input.compare(pos, 4, "true") == 0)
    {
        pos += 4;
        Node node(NodeType::Scalar);
        node = std::string{"true"};
        return node;
    }
    if (input.compare(pos, 5, "false") == 0)
    {
        pos += 5;
        Node node(NodeType::Scalar);
        node = std::string{"false"};
        return node;
    }
    return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid boolean"});
}

std::expected<Node, EmbedDSFError> JSONTranslator::parse_null(std::string_view input, std::size_t& pos)
{
    if (input.compare(pos, 4, "null") != 0)
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::ParseError, "Invalid null literal"});
    }
    pos += 4;
    Node node(NodeType::Null);
    return node;
}

void JSONTranslator::skip_whitespace(const std::string& input, std::size_t& pos) noexcept
{
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos])) != 0)
    {
        ++pos;
    }
}

std::expected<std::string, EmbedDSFError> JSONTranslator::emit_node(const Node&  node,
                                                                std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    switch (node.get_type())
    {
    case NodeType::Null:
        return std::string{"null"};
    case NodeType::Scalar:
        return emit_scalar(node);
    case NodeType::Sequence:
        return emit_sequence(node, indentLevel);
    case NodeType::Map:
        return emit_mapping(node, indentLevel);
    }
    return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Unknown node type"});
}

std::expected<std::string, EmbedDSFError> JSONTranslator::emit_scalar(const Node& node)
{
    if (!node.is_scalar())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Expected scalar node"});
    }

    const auto& scalar = node.as_string();
    if (scalar == "true" || scalar == "false" || is_number_literal(scalar))
    {
        return scalar;
    }

    std::string output = "\"";
    output.append(escape_string(scalar));
    output.push_back('\"');
    return output;
}

std::expected<std::string, EmbedDSFError> JSONTranslator::emit_sequence(
    const Node&  node,
    std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    if (!node.is_sequence())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Expected sequence"});
    }

    const auto& seq    = node.as_sequence();
    std::string output = "[";
    if (seq.empty())
    {
        output.push_back(']');
        return output;
    }

    output.push_back('\n');
    for (std::size_t i = 0; i < seq.size(); ++i)
    {
        output.append(indent_string(indentLevel + 1));
        auto child = emit_node(*seq[i], indentLevel + 1);
        if (!child)
        {
            return child;
        }
        output.append(child.value());
        if (i + 1 < seq.size())
        {
            output.push_back(',');
        }
        output.push_back('\n');
    }
    output.append(indent_string(indentLevel));
    output.push_back(']');
    return output;
}

std::expected<std::string, EmbedDSFError> JSONTranslator::emit_mapping(
    const Node&  node,
    std::int32_t indentLevel)  // NOLINT(misc-no-recursion)
{
    if (!node.is_map())
    {
        return std::unexpected(EmbedDSFError{EmbedDSFErrorType::EmissionError, "Expected mapping"});
    }

    const auto& map    = node.as_map();
    std::string output = "{";
    if (map.empty())
    {
        output.push_back('}');
        return output;
    }

    output.push_back('\n');
    for (std::size_t i = 0; i < map.size(); ++i)
    {
        output.append(indent_string(indentLevel + 1));
        output.push_back('\"');
        output.append(escape_string(map[i].key_));
        output.append("\": ");

        auto child = emit_node(*map[i].value_, indentLevel + 1);
        if (!child)
        {
            return child;
        }
        output.append(child.value());
        if (i + 1 < map.size())
        {
            output.push_back(',');
        }
        output.push_back('\n');
    }
    output.append(indent_string(indentLevel));
    output.push_back('}');
    return output;
}

std::string JSONTranslator::escape_string(const std::string& str)
{
    std::string escaped;
    escaped.reserve(str.size());
    for (const char ch : str)
    {
        switch (ch)
        {
        case '\"':
            escaped.append(R"(\")");
            break;
        case '\\':
            escaped.append(R"(\\)");
            break;
        case '\b':
            escaped.append(R"(\b)");
            break;
        case '\f':
            escaped.append(R"(\f)");
            break;
        case '\n':
            escaped.append(R"(\n)");
            break;
        case '\r':
            escaped.append(R"(\r)");
            break;
        case '\t':
            escaped.append(R"(\t)");
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                escaped.append(R"(\u00)");
                const auto code_unit   = static_cast<unsigned int>(static_cast<unsigned char>(ch));
                const auto high_nibble = (code_unit >> 4U) & 0x0FU;
                const auto low_nibble  = code_unit & 0x0FU;
                escaped.push_back(hex_digit(high_nibble));
                escaped.push_back(hex_digit(low_nibble));
            }
            else
            {
                escaped.push_back(ch);
            }
            break;
        }
    }
    return escaped;
}

std::string JSONTranslator::indent_string(std::int32_t indentLevel)
{
    if (indentLevel <= 0)
    {
        return {};
    }
    return std::string(static_cast<std::size_t>(indentLevel) * 2, ' ');  // NOLINT(modernize-return-braced-init-list)
}

}  // namespace EmbedDSF

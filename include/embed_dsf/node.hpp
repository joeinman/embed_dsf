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

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "embed_dsf/error.hpp"

namespace jsi::embed_dsf
{

class Node;

struct MapEntry
{
    std::string           key_;
    std::unique_ptr<Node> value_;

    MapEntry(std::string k, std::unique_ptr<Node> v) noexcept;
    ~MapEntry() noexcept;

    MapEntry(MapEntry&&) noexcept;
    MapEntry& operator=(MapEntry&&) noexcept;

    MapEntry(const MapEntry&)            = delete;
    MapEntry& operator=(const MapEntry&) = delete;
};

using MapType      = std::vector<MapEntry>;
using NullType     = std::monostate;
using ScalarType   = std::string;
using SequenceType = std::vector<std::unique_ptr<Node>>;
using NodeVariant  = std::variant<NullType, ScalarType, SequenceType, MapType>;

enum class NodeType : std::uint8_t
{
    Null = 0,
    Scalar,
    Sequence,
    Map
};

namespace detail
{
template <typename T>
struct dependent_false : std::false_type
{};

template <typename T>
[[nodiscard]] std::string to_scalar_string(T&& value)
{
    using Decayed = std::decay_t<T>;

    if constexpr (std::is_same_v<Decayed, std::string>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (std::is_same_v<Decayed, std::string_view> || std::is_same_v<Decayed, const char*>)
    {
        return std::string{std::forward<T>(value)};
    }
    else if constexpr (std::is_same_v<Decayed, bool>)
    {
        return std::forward<T>(value) ? "true" : "false";
    }
    else if constexpr (std::is_arithmetic_v<Decayed>)
    {
        std::ostringstream oss;
        oss << std::forward<T>(value);
        return oss.str();
    }
    else
    {
        static_assert(dependent_false<T>::value, "Unsupported scalar conversion");
    }
}
}  // namespace detail

class Node
{
public:
    Node() noexcept;
    explicit Node(NodeType type) noexcept;

    Node(const Node&)            = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&&) noexcept            = default;
    Node& operator=(Node&&) noexcept = default;

    [[nodiscard]] bool     is_null() const noexcept { return type_ == NodeType::Null; }
    [[nodiscard]] bool     is_scalar() const noexcept { return type_ == NodeType::Scalar; }
    [[nodiscard]] bool     is_sequence() const noexcept { return type_ == NodeType::Sequence; }
    [[nodiscard]] bool     is_map() const noexcept { return type_ == NodeType::Map; }
    [[nodiscard]] NodeType get_type() const noexcept { return type_; }

    template <typename T>
    [[nodiscard]] std::expected<T, EmbedDSFError> as() const noexcept
    {
        using Decayed = std::decay_t<T>;

        if constexpr (std::is_same_v<Decayed, std::string> || std::is_same_v<Decayed, std::string_view>)
        {
            if (!is_scalar())
            {
                return std::unexpected(EmbedDSFError{EmbedDSFErrorType::TypeError, "Node is not a scalar"});
            }

            if constexpr (std::is_same_v<Decayed, std::string>)
            {
                return as_string();
            }
            else
            {
                return std::string_view{as_string()};
            }
        }
        else if constexpr (std::is_same_v<Decayed, bool>)
        {
            if (!is_scalar())
            {
                return std::unexpected(EmbedDSFError{EmbedDSFErrorType::TypeError, "Node is not a scalar"});
            }
            const auto& str = as_string();
            if (str == "true")
            {
                return true;
            }
            if (str == "false")
            {
                return false;
            }
            return std::unexpected(
                EmbedDSFError{EmbedDSFErrorType::ScalarConversionError, "Scalar cannot convert to bool"});
        }
        else if constexpr (std::is_integral_v<Decayed>)
        {
            if (!is_scalar())
            {
                return std::unexpected(EmbedDSFError{EmbedDSFErrorType::TypeError, "Node is not a scalar"});
            }
            const auto& str = as_string();
            Decayed     result{};
            const char* begin = str.c_str();
            const auto  count = static_cast<std::ptrdiff_t>(std::ssize(str));
            const char* end   = begin;
            std::advance(end, count);
            auto [ptr, ec] = std::from_chars(begin, end, result);
            if (ec != std::errc{} || ptr != end)
            {
                return std::unexpected(
                    EmbedDSFError{EmbedDSFErrorType::ScalarConversionError, "Scalar cannot convert to integral"});
            }
            return result;
        }
        else if constexpr (std::is_floating_point_v<Decayed>)
        {
            if (!is_scalar())
            {
                return std::unexpected(EmbedDSFError{EmbedDSFErrorType::TypeError, "Node is not a scalar"});
            }
            const auto& str = as_string();
            Decayed     result{};
            const char* begin = str.c_str();
            const auto  count = static_cast<std::ptrdiff_t>(std::ssize(str));
            const char* end   = begin;
            std::advance(end, count);
            auto [ptr, ec] = std::from_chars(begin, end, result);
            if (ec != std::errc{} || ptr != end)
            {
                return std::unexpected(
                    EmbedDSFError{EmbedDSFErrorType::ScalarConversionError, "Scalar cannot convert to floating point"});
            }
            return result;
        }
        else
        {
            static_assert(detail::dependent_false<T>::value, "Unsupported conversion");
        }
    }

    [[nodiscard]] Node*       find(const std::string& key) noexcept;
    [[nodiscard]] const Node* find(const std::string& key) const noexcept;

    Node&       operator[](const std::string& key);
    const Node& operator[](const std::string& key) const;

    Node&       operator[](std::size_t index);
    const Node& operator[](std::size_t index) const;

    [[nodiscard]] Node&       at(std::size_t index);
    [[nodiscard]] const Node& at(std::size_t index) const;

    [[nodiscard]] std::size_t size() const noexcept;

    template <typename T>
    Node& emplace_back(T&& value)
    {
        ensure_type(NodeType::Sequence);
        auto node = std::make_unique<Node>();
        *node     = std::forward<T>(value);
        Node& ref = *node;
        as_sequence().emplace_back(std::move(node));
        return ref;
    }

    template <typename T,
              typename = std::enable_if_t<
                  std::is_arithmetic_v<std::decay_t<T>> || std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, std::string_view> || std::is_convertible_v<T, std::string_view>>>
    Node& operator=(T&& value)
    {
        ensure_type(NodeType::Scalar);
        as_string() = detail::to_scalar_string(std::forward<T>(value));
        return *this;
    }

private:
    friend class JSONTranslator;
    friend class YAMLTranslator;

    void ensure_type(NodeType type) noexcept;

    [[nodiscard]] ScalarType&       as_string();
    [[nodiscard]] const ScalarType& as_string() const;

    [[nodiscard]] SequenceType&       as_sequence();
    [[nodiscard]] const SequenceType& as_sequence() const;

    [[nodiscard]] MapType&       as_map();
    [[nodiscard]] const MapType& as_map() const;

    NodeType    type_{NodeType::Null};
    NodeVariant value_{};
};

}  // namespace jsi::embed_dsf

namespace jsi::embed_dsf
{

inline MapEntry::MapEntry(std::string k, std::unique_ptr<Node> v) noexcept : key_(std::move(k)), value_(std::move(v)) {}

inline MapEntry::~MapEntry() noexcept = default;

inline MapEntry::MapEntry(MapEntry&&) noexcept = default;

inline MapEntry& MapEntry::operator=(MapEntry&&) noexcept = default;

inline Node::Node() noexcept = default;

inline Node::Node(NodeType type) noexcept : value_(NullType{})
{
    ensure_type(type);
}

inline void Node::ensure_type(NodeType type) noexcept
{
    if (type_ == type)
    {
        return;
    }

    type_ = type;
    switch (type_)
    {
    case NodeType::Null:
        value_.emplace<NullType>();
        break;
    case NodeType::Scalar:
        value_.emplace<ScalarType>();
        break;
    case NodeType::Sequence:
        value_.emplace<SequenceType>();
        break;
    case NodeType::Map:
        value_.emplace<MapType>();
        break;
    }
}

inline ScalarType& Node::as_string()
{
    assert(is_scalar());
    return std::get<ScalarType>(value_);
}

inline const ScalarType& Node::as_string() const
{
    assert(is_scalar());
    return std::get<ScalarType>(value_);
}

inline SequenceType& Node::as_sequence()
{
    assert(is_sequence());
    return std::get<SequenceType>(value_);
}

inline const SequenceType& Node::as_sequence() const
{
    assert(is_sequence());
    return std::get<SequenceType>(value_);
}

inline MapType& Node::as_map()
{
    assert(is_map());
    return std::get<MapType>(value_);
}

inline const MapType& Node::as_map() const
{
    assert(is_map());
    return std::get<MapType>(value_);
}

inline Node* Node::find(const std::string& key) noexcept
{
    if (!is_map())
    {
        return nullptr;
    }

    auto& map = as_map();
    auto  it  = std::find_if(map.begin(), map.end(), [&key](const MapEntry& entry) { return entry.key_ == key; });
    if (it == map.end())
    {
        return nullptr;
    }
    return it->value_.get();
}

inline const Node* Node::find(const std::string& key) const noexcept
{
    if (!is_map())
    {
        return nullptr;
    }

    const auto& map = as_map();
    auto        it  = std::find_if(map.begin(), map.end(), [&key](const MapEntry& entry) { return entry.key_ == key; });
    if (it == map.end())
    {
        return nullptr;
    }
    return it->value_.get();
}

inline Node& Node::operator[](const std::string& key)
{
    ensure_type(NodeType::Map);
    auto& map = as_map();
    for (auto& entry : map)
    {
        if (entry.key_ == key)
        {
            return *entry.value_;
        }
    }

    auto  node = std::make_unique<Node>();
    Node& ref  = *node;
    map.emplace_back(key, std::move(node));
    return ref;
}

inline const Node& Node::operator[](const std::string& key) const
{
    const Node* found = find(key);
    assert(found && "Key not found in const Node::operator[]");
    return *found;
}

inline Node& Node::operator[](std::size_t index)
{
    ensure_type(NodeType::Sequence);
    auto& seq = as_sequence();
    if (index >= seq.size())
    {
        seq.resize(index + 1);
        for (auto& entry : seq)
        {
            if (!entry)
            {
                entry = std::make_unique<Node>();
            }
        }
    }
    return *seq[index];
}

inline const Node& Node::operator[](std::size_t index) const
{
    assert(is_sequence());
    const auto& seq = as_sequence();
    assert(index < seq.size());
    return *seq[index];
}

inline Node& Node::at(std::size_t index)
{
    assert(is_sequence());
    auto& seq = as_sequence();
    assert(index < seq.size());
    return *seq[index];
}

inline const Node& Node::at(std::size_t index) const
{
    assert(is_sequence());
    const auto& seq = as_sequence();
    assert(index < seq.size());
    return *seq[index];
}

inline std::size_t Node::size() const noexcept
{
    switch (type_)
    {
    case NodeType::Sequence:
        return as_sequence().size();
    case NodeType::Map:
        return as_map().size();
    default:
        return 0U;
    }
}

}  // namespace jsi::embed_dsf

/*
 * Copyright (c) 2025, Joe Inman
 *
 * Licensed under the MIT License.
 * You may obtain a copy of the License at:
 *     https://opensource.org/licenses/MIT
 *
 * This file is part of the EmbedDSF project.
 */

#include <embed_dsf/yaml_translator.hpp>

#include <iostream>
#include <string>

int main()
{
    jsi::embed_dsf::Node node(jsi::embed_dsf::NodeType::Map);
    node["person"] = jsi::embed_dsf::Node(jsi::embed_dsf::NodeType::Sequence);
    node["person"].emplace_back("Name 1");
    node["person"].emplace_back("Name 2");
    node["other"]        = jsi::embed_dsf::Node(jsi::embed_dsf::NodeType::Map);
    node["other"]["key"] = 42;

    auto result = jsi::embed_dsf::YAMLTranslator::emit(node);
    if (!result)
    {
        std::cerr << "Error emitting YAML: " << result.error() << std::endl;
        return 1;
    }

    std::cout << result.value();
    return 0;
}

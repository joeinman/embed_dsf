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
    EmbedDSF::YAMLTranslator emitter;

    EmbedDSF::Node node(EmbedDSF::NodeType::Map);
    node["person"] = EmbedDSF::Node(EmbedDSF::NodeType::Sequence);
    node["person"].emplace_back("Name 1");
    node["person"].emplace_back("Name 2");
    node["other"]        = EmbedDSF::Node(EmbedDSF::NodeType::Map);
    node["other"]["key"] = 42;

    auto result = emitter.emit(node);
    if (!result)
    {
        std::cerr << "Error emitting YAML: " << result.error() << std::endl;
        return 1;
    }

    std::cout << result.value();
    return 0;
}

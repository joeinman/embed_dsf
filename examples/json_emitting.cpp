/*
 * Copyright (c) 2025, Joe Inman
 *
 * Licensed under the MIT License.
 * You may obtain a copy of the License at:
 *     https://opensource.org/licenses/MIT
 *
 * This file is part of the EmbedDSF project.
 */

#include <embed_dsf/json_translator.hpp>

#include <iostream>
#include <string>

int main()
{
    jsi::embed_dsf::Node root(jsi::embed_dsf::NodeType::Map);

    auto& people = root["person"];
    people.emplace_back(std::string{"Name 1"});
    people.emplace_back(std::string{"Name 2"});

    auto& other  = root["other"];
    other["key"] = 42;

    auto emitted = jsi::embed_dsf::JSONTranslator::emit(root);
    if (!emitted)
    {
        std::cerr << "Failed to emit JSON: " << emitted.error() << '\n';
        return 1;
    }

    std::cout << emitted.value() << '\n';
    return 0;
}

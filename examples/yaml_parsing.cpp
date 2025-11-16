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
#include <type_traits>

int main()
{
    std::string yamlString = "person:\n"
                             "  name: John Doe\n"
                             "  age: 31\n"
                             "  email: john.doe@example.com\n"
                             "  address:\n"
                             "    street: 123 Main St\n"
                             "    city: Springfield\n"
                             "    zip: 12345\n";

    EmbedDSF::YAMLTranslator parser;
    auto                 document = parser.parse(yamlString);
    if (!document)
    {
        std::cerr << "Error parsing YAML: " << document.error() << std::endl;
        return 1;
    }

    const auto& root   = document.value();
    const auto* person = root.find("person");
    if (!person)
    {
        std::cerr << "YAML document is missing a 'person' entry\n";
        return 1;
    }

    const auto readScalar = []<typename T>(const EmbedDSF::Node* node, T defaultValue) {
        using ValueType = std::decay_t<T>;
        if (!node)
        {
            return defaultValue;
        }
        auto converted = node->as<ValueType>();
        if (!converted)
        {
            return defaultValue;
        }
        return converted.value();
    };

    const auto* address = person->find("address");

    std::cout << "Name: " << readScalar(person->find("name"), std::string{"N/A"}) << std::endl;
    std::cout << "Age: " << readScalar(person->find("age"), 0) << std::endl;
    std::cout << "Email: " << readScalar(person->find("email"), std::string{"N/A"}) << std::endl;
    std::cout << "Street: " << readScalar(address ? address->find("street") : nullptr, std::string{"N/A"}) << std::endl;
    std::cout << "City: " << readScalar(address ? address->find("city") : nullptr, std::string{"N/A"}) << std::endl;
    std::cout << "Zip: " << readScalar(address ? address->find("zip") : nullptr, 0) << std::endl;

    return 0;
}

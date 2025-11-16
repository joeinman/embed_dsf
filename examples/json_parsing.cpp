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
#include <type_traits>

int main()
{
    const std::string json = R"JSON(
{
  "person": {
    "name": "John Doe",
    "age": 31,
    "email": "john.doe@example.com",
    "address": {
      "street": "123 Main St",
      "city": "Springfield",
      "zip": 12345
    }
  }
}
)JSON";

    auto document = jsi::embed_dsf::JSONTranslator::parse(json);
    if (!document)
    {
        std::cerr << "Failed to parse JSON: " << document.error() << '\n';
        return 1;
    }

    const jsi::embed_dsf::Node& root   = document.value();
    const jsi::embed_dsf::Node* person = root.find("person");
    if (!person || !person->is_map())
    {
        std::cerr << "Missing 'person' object" << '\n';
        return 1;
    }

    auto readScalar = []<typename T>(const jsi::embed_dsf::Node* node, T defaultValue) {
        using ValueType = std::decay_t<T>;
        if (!node)
        {
            return defaultValue;
        }
        auto value = node->template as<ValueType>();
        if (!value)
        {
            return defaultValue;
        }
        return value.value();
    };

    const auto*                 address    = person->find("address");
    const jsi::embed_dsf::Node* streetNode = address ? address->find("street") : nullptr;
    const jsi::embed_dsf::Node* cityNode   = address ? address->find("city") : nullptr;
    const jsi::embed_dsf::Node* zipNode    = address ? address->find("zip") : nullptr;

    const auto name   = readScalar(person->find("name"), std::string{"unknown"});
    const auto age    = readScalar(person->find("age"), 0);
    const auto email  = readScalar(person->find("email"), std::string{"n/a"});
    const auto street = readScalar(streetNode, std::string{"n/a"});
    const auto city   = readScalar(cityNode, std::string{"n/a"});
    const auto zip    = readScalar(zipNode, 0);

    std::cout << "Name: " << name << '\n';
    std::cout << "Age: " << age << '\n';
    std::cout << "Email: " << email << '\n';
    std::cout << "Street: " << street << '\n';
    std::cout << "City: " << city << '\n';
    std::cout << "ZIP: " << zip << '\n';

    return 0;
}

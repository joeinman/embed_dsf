cmake_minimum_required(VERSION 3.28)

add_subdirectory(external/libyaml)

project(embed_dsf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/json_parser.cpp
        src/yaml_parser.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        yaml
)

option(EMBEDDSF_BUILD_EXAMPLES "Build EmbedDSF example executables" ON)

if(EMBEDDSF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

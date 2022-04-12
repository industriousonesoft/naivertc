cmake_minimum_required(VERSION 3.8)

project(signaling VERSION 0.01 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    # Header files
    src/utils/url_parts.hpp
    src/components/websocket.hpp
    src/channels/signaling_channel.hpp
    src/channels/signaling_client.hpp
   

    # Cpp files
    src/components/websocket.cpp
    src/channels/signaling_channel.cpp
    src/channels/signaling_client.cpp
)

# gcc -Wall: enables all compiler's warning messages
# gcc -Wextra: enables extra warning messages
# gcc -Wpedantic: 当gcc编译不符合ANSI/ISO C标准的代码时，将产生相应的警告信息
set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/../../deps/json/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json
    naivertc
)


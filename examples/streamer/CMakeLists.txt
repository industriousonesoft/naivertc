cmake_minimum_required(VERSION 3.8)

project(streamer VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include files
set(HEADER_FILES
    src/client.hpp
    # stream
    src/stream/media_stream_source.hpp
    src/stream/media_file_stream_source.hpp
    src/stream/h264_file_stream_source.hpp
)

set(SOURCE_FILES
    # src files
    src/main.cpp
    src/client.cpp
    src/client_signaling_delegate.cpp
    src/client_peer_connection.cpp

    # stream
    src/stream/media_file_stream_source.cpp
    src/stream/h264_file_stream_source.cpp
)

# gcc -Wall: enables all compiler's warning messages
# gcc -Wextra: enables extra warning messages
# gcc -Wpedantic: 当gcc编译不符合ANSI/ISO C标准的代码时，将产生相应的警告信息
set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    naivertc
    signaling
)





cmake_minimum_required(VERSION 3.16)
project(emcli_standalone
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../" "cmake-build-debug")


add_executable(${PROJECT_NAME} main.cpp)

target_sources(${PROJECT_NAME}
    PRIVATE
        xmodem/xmodem.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        xmodem/
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        emcli_lib
)

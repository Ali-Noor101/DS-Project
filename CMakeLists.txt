cmake_minimum_required(VERSION 3.10)
project(Xonix)

# Set CMAKE_PREFIX_PATH for Homebrew's keg-only SFML on macOS
if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/sfml@2" ${CMAKE_PREFIX_PATH})
endif()

find_package(SFML 2 REQUIRED COMPONENTS network audio graphics window system)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/images" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/fonts" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

add_executable(xonix Source.cpp)

target_link_libraries(xonix PRIVATE sfml-system sfml-window sfml-graphics sfml-network sfml-audio)
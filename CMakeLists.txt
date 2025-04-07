cmake_minimum_required(VERSION 3.29)
project(client-server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(socket)
add_subdirectory(client)
add_subdirectory(server)

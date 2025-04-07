add_executable(client src/main.cpp src/Client.cpp src/ClientCLI.cpp)
target_link_libraries(client PRIVATE socket)
target_include_directories(client PRIVATE .${CMAKE_CURRENT_SOURCE_DIR}/socket ${CMAKE_CURRENT_SOURCE_DIR}/include)
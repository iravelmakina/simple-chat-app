add_executable(server src/main.cpp src/Server.cpp src/ThreadPool.cpp src/Room.cpp)
target_link_libraries(server PRIVATE socket)
target_include_directories(server PRIVATE .${CMAKE_CURRENT_SOURCE_DIR}/socket/include ${CMAKE_CURRENT_SOURCE_DIR}/include)

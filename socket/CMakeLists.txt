add_library(socket STATIC src/Socket.cpp)
target_include_directories(socket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

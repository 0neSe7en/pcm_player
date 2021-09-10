cmake_minimum_required(VERSION 3.20)
project(pcm_player)

set(CMAKE_CXX_STANDARD 20)
include_directories(ebur128/queue)
add_executable(pcm_player main.cpp includes/SPSCQueue.h ebur128/ebur128.c)
target_link_libraries(pcm_player -lSDL2)



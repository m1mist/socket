cmake_minimum_required(VERSION 3.28)
project(socket C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(socket
        client.c
        server.c)

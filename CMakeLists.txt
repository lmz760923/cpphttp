cmake_minimum_required(VERSION 3.10)
project(CPPServ)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

add_executable(cppserv main.cpp)

target_link_libraries(cppserv pthread stdc++fs)
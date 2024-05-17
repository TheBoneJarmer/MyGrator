cmake_minimum_required(VERSION 3.16.3)
project(mygrator)

set(CMAKE_CXX_STANDARD 17)

# Mysql migrations
add_executable(mygrator src/main.cpp)
target_include_directories(mygrator PRIVATE include)
target_link_directories(mygrator PRIVATE lib)
target_link_libraries(mygrator mysqlcppconn -Wl,-rpath='$ORIGIN:$ORIGIN/lib')

cmake_minimum_required(VERSION 3.16.3)
project(mygrator)

set(CMAKE_CXX_STANDARD 17)

# Mysql migrations
add_executable(mygrator src/mygrator/main.cpp)
target_include_directories(mygrator PRIVATE include)

if (WIN32)
	target_link_directories(mygrator PRIVATE lib/win-x64/vs14)
	target_link_libraries(mygrator mysqlcppconn)
endif (WIN32)

if (UNIX)
	target_link_directories(mygrator PRIVATE lib/linux-x64)
	target_link_libraries(mygrator mysqlcppconn -Wl,-rpath='$ORIGIN:$ORIGIN/lib')
endif (UNIX)

cmake_minimum_required(VERSION 3.22)
project(file_system)

set(CMAKE_CXX_STANDARD 14)

#boost库所在的根目录
set(BOOST_ROOT "/usr/local/include/boost")
#添加头文件搜索路径
include_directories(/usr/local/include)
#添加库文件搜索路径
link_directories(/usr/local/lib)


add_executable(file_system file_system.cpp)
add_executable(shell shell.cpp)
add_executable(shell2 shell.cpp)

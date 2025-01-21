cmake_minimum_required(VERSION 3.1)  # 指定最低版本要求

# 设置项目名称
project(MyProject) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 确保使用C++17标准

# 检查是否已经指定了编译器
if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER gcc)
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER g++)
endif()

# 打印当前编译工具
message("当前编译工具: ${CMAKE_CXX_COMPILER}")
message("当前架构类型: ${COMPILE_TYPE}")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # 可执行文件输出到 bin 目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/plug)  # 生成的库文件输出到 lib 目录

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

# 递归添加源文件
file(GLOB_RECURSE SOURCES 
	${CMAKE_SOURCE_DIR}/src/code/*.hpp
	${CMAKE_SOURCE_DIR}/src/code/*.cpp
	${CMAKE_SOURCE_DIR}/src/code/*.c
)

# 递归添加头文件，用于安装到指定目录
file(GLOB_RECURSE INCLUDE
	${CMAKE_SOURCE_DIR}/src/code/*.hpp
	${CMAKE_SOURCE_DIR}/src/code/*.h
)

# 添加头文件搜索路径
include_directories(
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/build/tmp
)

# 生成库文件
add_library(MessageQueue SHARED ${SOURCES})

# 链接库文件
target_link_libraries(MessageQueue PRIVATE pthread)

# 安装库文件到 lib 目录
install(TARGETS MessageQueue DESTINATION lib)
 
# 安装头文件到 include 目录
install(FILES ${INCLUDE} DESTINATION include)

if(TEST_RESULT EQUAL 1)
	add_subdirectory(test/test1)
	add_subdirectory(test/test2)
	# 检查是否存在protobuf库, 有protobuf库才能编译
	if(EXISTS /opt/protobuf/)
		add_subdirectory(test/test3)
		add_subdirectory(test/test4)
	endif()
endif()

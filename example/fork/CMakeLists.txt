# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/fork)  # 可执行文件输出到 bin 目录

# 递归库文件
file(GLOB_RECURSE LIBRARIES 
	${CMAKE_SOURCE_DIR}/plug/*.so
)

# 递归添加源文件
file(GLOB_RECURSE SOURCES 
	${CMAKE_SOURCE_DIR}/example/fork/*.cpp
	${CMAKE_SOURCE_DIR}/src/code/*.cpp
)

# 添加头文件搜索路径
include_directories(
	${CMAKE_SOURCE_DIR}/example/fork
	${CMAKE_SOURCE_DIR}/build/tmp
)

# 生成可执行文件
add_executable(test1 ${SOURCES} ${CMAKE_SOURCE_DIR}/example/fork/main.cpp)

target_link_libraries(test1 pthread ${LIBRARIES})
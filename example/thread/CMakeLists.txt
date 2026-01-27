# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/thread)  # 可执行文件输出到 bin 目录

# 递归库文件
file(GLOB_RECURSE LIBRARIES 
	${CMAKE_SOURCE_DIR}/plug/*.so
)

# 递归添加源文件
file(GLOB_RECURSE SOURCES 
	${CMAKE_SOURCE_DIR}/example/thread/*.cpp
	${CMAKE_SOURCE_DIR}/src/code/*.cpp
)

# 添加头文件搜索路径
include_directories(
	${CMAKE_SOURCE_DIR}/example/thread
	${CMAKE_SOURCE_DIR}/build/tmp
)

# 生成可执行文件T
add_executable(subTest ${SOURCES} ${CMAKE_SOURCE_DIR}/example/thread/main.cpp)

target_link_libraries(subTest pthread ${LIBRARIES})
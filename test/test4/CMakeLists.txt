# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/test4)  # 可执行文件输出到 bin 目录

# 递归库文件
file(GLOB_RECURSE LIBRARIES 
	${CMAKE_SOURCE_DIR}/plug/*.so
	/opt/protobuf/lib/*.so
)

# 递归添加源文件
file(GLOB_RECURSE SOURCES 
	${CMAKE_SOURCE_DIR}/test/test4/protoMsg/*.cc
	${CMAKE_SOURCE_DIR}/src/code/*.cpp
)

# 添加头文件搜索路径
include_directories(
	${CMAKE_SOURCE_DIR}/test/test4/protoMsg/include
	/opt/protobuf/include
	${CMAKE_SOURCE_DIR}/build/tmp
)

# 生成可执行文件
add_executable(spub ${SOURCES} ${CMAKE_SOURCE_DIR}/test/test4/pub.cpp)
add_executable(ssub ${SOURCES} ${CMAKE_SOURCE_DIR}/test/test4/sub.cpp)

target_link_libraries(spub pthread rt ${LIBRARIES})
target_link_libraries(ssub pthread rt ${LIBRARIES})

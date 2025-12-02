#!/bin/bash

# 脚本名称和路径
CALLED_SCRIPT_PATH="script/make2.sh"

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# 检查是否至少有一个参数被传递
if [ "$#" -gt 1 ]; then
    echo "用法: $0 <可选参数1>"
    echo "注意: 本脚本只接受最多一个参数。"
    exit 1
fi

# 获取传入的两个参数
PARAM1="$1"
PARAM2="$2"

# 检查被调用脚本是否存在
if [ ! -f "$CALLED_SCRIPT_PATH" ]; then
    echo "错误: 脚本 $CALLED_SCRIPT_PATH 不存在"
    exit 1
fi

# 如果脚本存在且有执行权限，则调用它并传递参数
if [ -x "$CALLED_SCRIPT_PATH" ]; then
    "$CALLED_SCRIPT_PATH" "$SCRIPT_DIR" "$PARAM1" "$PARAM2"
else
    echo "错误: 脚本 $CALLED_SCRIPT_PATH 没有执行权限"
    exit 1
fi

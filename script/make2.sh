#!/bin/bash

# 检查脚本参数
if [ "$2" != "" ] && 
   [ "$2" != "clean" ] && 
   [ "$2" != "install" ]; then
  echo "请输入正确参数, 参数clean清除构建, 参数install安装到指定目录"
  exit 1
fi

# 获取脚本所在目录
script_dir="$1"
install_dir=""
compiler=""
hostsystem=""
config_file=$script_dir/config/config.properties
# 从配置文件中读取安装目录
if [ -f "$config_file" ]; then
    . $config_file
    echo "Using prefix: $PREFIX"
    install_dir=$PREFIX
    echo "Using CXX: $CXX"
    compiler=$CXX
    echo "Using HOST: $HOST"
    hostsystem=$HOST
else
    echo "Config file not found: $config_file"
    exit 1
fi

if [ ! -d "$script_dir/build" ]; then
  mkdir "$script_dir/build"
fi

# 进入build目录
cd "$script_dir/build"

# 根据输入的参数选择操作
if [ -z "$2" ]; then
  if [ -z "$compiler" ]; then
    # 进行cmake构建
    cmake -DCMAKE_INSTALL_PREFIX=$install_dir -DCOMPILE_TYPE="$2" ..

    # 检查cmake是否成功
    if [ $? -eq 0 ]; then
      # 使用make编译程序
      make -j2
    else
      echo "cmake构建失败"
      exit 1
    fi
  elif [ $compiler = "aarch64-linux-gnu-g++" ] || 
      [ $compiler = "arm-linux-gnueabihf-g++" ]; then
    # 使用交叉编译工具链进行编译
    # 确保交叉编译工具链已安装并位于正确的路径下
    if [ $compiler = "aarch64-linux-gnu-g++" ]; then
      export CXX=$compiler
    elif [ $compiler = "arm-linux-gnueabihf-g++" ]; then
      export CXX=$compiler
    fi

    # 进行cmake构建
    cmake -DCMAKE_INSTALL_PREFIX=$install_dir -DCOMPILE_TYPE="$hostsystem" ..

    # 检查cmake是否成功
    if [ $? -eq 0 ]; then
      # 使用make编译程序
      make -j2
    else
      echo "cmake构建失败"
      exit 1
    fi
  else
    echo "编译器错误"
    exit 1
  fi
elif [ $2 = "clean" ]; then
  # 清除构建
  sudo rm -rf *
  # 检查本地build清除是否成功
  if [ $? -eq 0 ]; then
    echo "本地清除成功"
  else
    echo "本地清除失败"
    exit 1
  fi

  sudo rm -rf ../plug/*
  # 检查动态库清除是否成功
  if [ $? -eq 0 ]; then
    echo "动态库清除成功"
  else
    echo "动态库清除失败"
    exit 1
  fi

  sudo rm -rf $install_dir
  # 检查安装包清除是否成功
  if [ $? -eq 0 ]; then
    echo "安装清除成功"
  else
    echo "安装清除失败"
    exit 1
  fi
  echo "构建已清除"
elif [ $2 = "install" ]; then
  sudo make install
  # 检查install是否成功
  if [ $? -eq 0 ]; then
    echo "安装成功"
  else
    echo "安装失败"
    exit 1
  fi
else
  echo "参数错误"
  exit 1
fi


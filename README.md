# zspaceMQ
## 引言
这是一款用C++实现的轻量级的消息队列，原本应用在嵌入式系统上，所以这款消息队列中间件尽可能设计简单易用移植性强。至于功能性上的不足，欢迎留言评论补充，日后持续改进。

## 功能
* 利用线程池，实现线程间消息队列广播通信
* 利用线程池，实现线程间服务队列点对点通信
* 进程间消息队列广播通信（传递序列化数据）
* 进程间服务队列点对点通信（传递序列化数据）
* 利用linux下管道和消息队列，实现的进程通信管理和初始化

## 环境要求
* Linux
* C++11
* Protobuf（非硬性要求，所有可以序列化的库或接口都可以）

## 目录树
```
├── build               cmake编译生成目录
├── CMakeLists.txt
├── config              编译环境配置文件目录
├── configure
├── LICENSE
├── plug                编译生成的动态库存放目录
├── bin                 编译生成的可执行文件存放目录
├── test                测试程序源码
├── README.md
├── script              编译脚本目录
│   └── make2.sh
└── src
    └── code            源码目录
```

## 编译环境配置
通过执行根目录下configure进行编译脚本生成和编译配置项生成
```bash
./configure
```
直接执行脚本会有默认配置
```bash
PREFIX=/opt/zspaceMQ
CXX=g++
HOST=linux
```
可以根据自己需求配置编译环境，配置项如下:
```bash
  --prefix=<install_prefix>
    配置安装路径,默认/opt/zspaceMQ

  --host=<host_system>
    配置编译平台,默认linux

  CXX=<compiler>
    配置编译工具链,默认g++

  --no-prefix
    不设置安装路径

  --no-tests
    不编译测试程序

  clean
    将配置文件和编译脚本清除
```
示例:
```bash
./configure CXX=aarch64-linux-gnu-g++ --host=aarch64-linux-gnu --prefix=/opt/zspaceMQ/ --no-tests
```

## 运行编译脚本编译
配置脚本执行成功后，会生成make.sh的编译脚本，直接执行编译脚本即可进行编译
```bash
./make.sh
```
安装编译好的程序到指定路径
```bash
./make.sh install
```
清除构建编译和安装
```bash
./make.sh clean
```
## 致谢
Linux下C++消息队列zspaceMQ，沈义函
问题反馈邮箱：907184203@qq.com

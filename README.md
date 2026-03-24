# S2服务端插件

### Linux Cmake环境安装

```shell

# 安装cmake依赖
yum install -y gcc gcc-c++ glibc-devel.i686 libgcc.i686 libstdc++.i686 gcc-c++.i686

# 安装cmake
yum install -y wget
cd /usr/local
wget https://github.com/Kitware/CMake/releases/download/v3.22.6/cmake-3.22.6-linux-x86_64.tar.gz

# 或者手动上传至 /usr/local/cmake-3.22.6-linux-x86_64.tar.gz
tar -zxvf cmake-3.22.6-linux-x86_64.tar.gz
ln -sf /usr/local/cmake-3.22.6-linux-x86_64/bin/cmake /usr/bin/cmake

# 验证cmake版本
cmake --version

```

### Linux Cmake编译服务端插件

```shell

# 1.进入服务端项目
cd /Server/monitor
# cd /Server/server

# 2.修复 CMakeLists.txt 字符问题(如果有问题)
sed -i '1s/^.//' CMakeLists.txt

# 3.进入编译目录
rm -rf build && mkdir build && cd build

# 4.生成32位工程
# rm -rf ./*
cmake .. -DCMAKE_C_FLAGS="-m32" -DCMAKE_CXX_FLAGS="-m32"

# 5.执行编译
make -j4
```

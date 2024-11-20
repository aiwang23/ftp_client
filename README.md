# 基于 libcurl 的 ftp ftps sftp客户端 ---（名字还没想好）

## 功能
目前支持上传、下载、删除、重命名、新建文件夹、文件列表
后面会加gui，应该会用Qt6，而且会在debian12上试下

### 2024/11/11
使用了Qt6, 添加了GUI的上传和下载以及跳转功能

## 环境
我这里是 win11 vc17 cmake3.27 clion2024.1 Qt6.4

## 如何编译
1. ***src\gui\CMakeLists.txt*** 把这里修改成自己的Qt6安装目录
```cmake
set(CMAKE_PREFIX_PATH "C:/Qt/6.4.3/msvc2019_64/lib/cmake")
```

2. 在项目的根目录，创建***build***文件夹，进入 ***build***
```bash
mkdir build
cd build 
```

3. 编译
```bash
cmake -G "Visual Studio 17 2022" ..
cmake --build .
```
> 编译出来的**可执行文件**在 bin/

### curl版本信息
> curl 8.9.0-DEV (x86_64-pc-win32) libcurl/8.9.0-DEV OpenSSL/1.1.1i zlib/1.3.1.1-motley WinIDN libssh2/1.11.1_DEV
> Release-Date: [unreleased]
> Protocols: dict file ftp ftps gopher gophers http https imap imaps ipfs ipns ldap ldaps mqtt pop3 pop3s rtsp scp sftp smb smbs smtp smtps telnet tftp 
> Features: alt-svc AsynchDNS HSTS HTTPS-proxy IDN IPv6 Kerberos Largefile libz NTLM SPNEGO SSL SSPI threadsafe UnixSockets


# Reactor 高性能网络模型（Echo 服务器）

基于 PDF 考核要求实现的一个 **单 Reactor 多线程** Echo 服务器，用 C++17 手写完成。

- 核心：`epoll`（可切 `select`）多路复用 + 事件分发 + 事件处理器（Handler）
- 加分项：线程池（业务处理与 IO 分离）、定时器（周期任务）
- 教程/演示页（GitHub Pages）：<https://zhxsdxin.github.io/Reactor/>

## 目录结构

```
include/   10 个头文件（EventHandler、Epoll/Select 多路复用器、Reactor、Handler 等）
src/       9 个源文件（实现 + main.cpp）
demo.sh    10 客户端并发演示脚本
CMakeLists.txt   # 构建配置（CMake / g++ 二选一）
index.html       # 逐文件讲解 + 数据流教学页
```

## 编译运行（Linux）

```bash
g++ -std=c++17 -o server src/*.cpp -Iinclude -lpthread
./server 8080
```

另开终端测试：

```bash
nc localhost 8080        # 输入任意文字，回车即回显
```

## Windows 上用 WSL 跑

1. 管理员打开 PowerShell 安装 WSL：`wsl --install`，重启后设置 Ubuntu 用户。
2. 进 Ubuntu 装工具：
   ```bash
   sudo apt update && sudo apt install -y g++ netcat-openbsd
   ```
3. 进入项目目录跑演示（WSL 可访问 Windows 盘，路径为 `/mnt/c/...`）：
   ```bash
   cd /mnt/c/Users/v_shinzhang/Downloads/Reactor
   bash demo.sh
   ```

## 10 客户端并发演示

```bash
bash demo.sh
```

脚本自动完成：编译 → 启动服务器 → 起 10 个客户端并发发消息 → 检查回显 → 停服务器。期望输出 10 行 `[✓] 客户端 #N: 发送=接收 → 正确`。

## 手动演示（3 个终端）

```bash
# 终端 1：启动服务器
./server 8080

# 终端 2/3：两个客户端各自连入
nc localhost 8080
```

两个客户端互相独立：A 输入 `hello` 回显 `hello`，B 输入 `world` 回显 `world`，互不干扰。
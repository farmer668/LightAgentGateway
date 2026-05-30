# Debug Log

## Debug 1: Port 80 Permission and Invalid FD

### 问题现象

普通用户运行 `./WebServer` 报：

```text
set socket non block failed: Bad file descriptor
```

### 触发命令

```bash
./WebServer
```

### 报错信息或异常返回

```text
set socket non block failed: Bad file descriptor
```

### 原因分析

服务默认监听 80 端口。普通用户权限不足，socket/bind 失败后继续设置
非阻塞，导致 fd 无效。

### 修复方式

临时使用 root 权限运行：

```bash
sudo ./WebServer
```

后续开发环境建议改为 8080 端口，或者增强 socket/bind 错误检查，避免
无效 fd 继续流入 `setSocketNonBlocking`。

### 验证命令

```bash
sudo ./WebServer
curl -i http://127.0.0.1/api/health
```

### 验证结果

服务可以启动，API 返回 JSON。

## Debug 2: sudo Does Not Preserve GEMINI_API_KEY

### 问题现象

执行 `export GEMINI_API_KEY=...` 后再运行 `sudo ./WebServer`，
`/api/health` 仍然显示：

```json
"gemini_api_key_configured": false
```

### 触发命令

```bash
export GEMINI_API_KEY="test_key_123"
sudo ./WebServer
curl -i http://127.0.0.1/api/health
```

### 报错信息或异常返回

```json
"gemini_api_key_configured": false
```

### 原因分析

`sudo` 默认不保留普通用户环境变量。`GatewayConfig` 在服务进程内读取环境变量，
但该变量没有传入 root 进程环境。

### 修复方式

直接把变量传给 `sudo` 启动命令：

```bash
sudo GEMINI_API_KEY="test_key_123" ./WebServer
```

或者开发时使用非特权端口：

```bash
GEMINI_API_KEY="test_key_123" ./WebServer -p 8080 -l /tmp/WebServer.log
```

### 验证命令

```bash
sudo GEMINI_API_KEY="test_key_123" ./WebServer
curl -i http://127.0.0.1/api/health
```

### 验证结果

`/api/health` 返回：

```json
"gemini_api_key_configured": true
```

结论：`GatewayConfig` 环境变量读取正常，该问题属于启动方式问题。

## Debug 3: Windows MinGW Build Fails on Linux/POSIX APIs

### 问题现象

在 Windows/MinGW 环境中可以完成 CMake 配置，但完整构建失败。

### 触发命令

```powershell
cmake -S . -B build-codex -G "MinGW Makefiles"
cmake --build build-codex --target WebServer -j 4
```

### 报错信息或异常返回

```text
fatal error: sys/syscall.h: No such file or directory
error: 'setbuffer' was not declared in this scope
error: 'fwrite_unlocked' was not declared in this scope
```

### 原因分析

原 LinYa/WebServer 依赖 Linux/POSIX API，例如 `epoll`、`sys/syscall.h`、
`setbuffer` 和 `fwrite_unlocked`。这些接口在当前 MinGW 环境不可用。

### 修复方式

本阶段不修改底层 Reactor/日志模块以适配 Windows。继续以 Linux 作为运行
和集成测试环境。

### 验证命令

```powershell
g++ -std=c++17 -Wall -I. -fsyntax-only GatewayConfig.cpp LlmProvider.cpp MockProvider.cpp GeminiProvider.cpp ProviderFactory.cpp LightAgentGateway.cpp
```

### 验证结果

新增 Stage 3 业务层通过 C++17 语法检查；完整服务构建需在 Linux 环境验证。

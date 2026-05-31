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

## Stage 4 Potential Issues

### Issue 1: libcurl Not Installed

#### 问题现象

`make` 或 CMake 构建失败，提示找不到 curl 头文件或链接库。

#### 触发命令

```bash
make
```

或：

```bash
cmake -S . -B build
cmake --build build --target WebServer -j 4
```

#### 报错信息或异常返回

```text
fatal error: curl/curl.h: No such file or directory
```

或：

```text
cannot find -lcurl
```

#### 原因分析

Stage 4 新增 `HttpClient`，使用 libcurl 实现 HTTPS POST。系统缺少
libcurl 开发包时会编译或链接失败。

#### 修复方式

```bash
sudo apt update
sudo apt install -y libcurl4-openssl-dev
```

#### 验证命令

```bash
make clean
make
```

#### 验证结果

安装依赖后，curl 头文件和 `-lcurl` 链接库可被找到。

### Issue 2: Gemini API Key Not Detected Under sudo

#### 问题现象

`export GEMINI_API_KEY=...` 后执行 `sudo ./WebServer`，`/api/health` 仍显示：

```json
"gemini_api_key_configured": false
```

#### 触发命令

```bash
export GEMINI_API_KEY="xxx"
sudo LIGHTAGENT_DEFAULT_PROVIDER="gemini" ./WebServer
curl http://127.0.0.1/api/health
```

#### 原因分析

`sudo` 默认不保留普通用户环境变量。

#### 修复方式

```bash
sudo GEMINI_API_KEY="xxx" LIGHTAGENT_DEFAULT_PROVIDER="gemini" ./WebServer
```

或使用 8080 端口避免 sudo：

```bash
GEMINI_API_KEY="xxx" LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
  ./WebServer -p 8080 -l /tmp/WebServer.log
```

#### 验证命令

```bash
curl http://127.0.0.1/api/health
```

#### 验证结果

返回：

```json
"gemini_api_key_configured": true
```

### Issue 3: Gemini HTTP Non-2xx Response

#### 问题现象

`POST /api/chat` 返回：

```json
"success": false
```

#### 触发命令

```bash
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

#### 报错信息或异常返回

可能包含：

```json
"error_message": "Gemini HTTP request failed (status 400): HTTP non-2xx status: 400"
```

#### 原因分析

可能是 API Key 错误、模型名错误、额度不足、网络异常、代理或 DNS 问题。

#### 修复方式

检查：

- `GEMINI_API_KEY`
- `GEMINI_MODEL`
- `GEMINI_API_BASE`
- 虚拟机网络连通性
- Google API 可访问性

#### 验证命令

```bash
curl http://127.0.0.1/api/metrics
```

#### 验证结果

`last_provider` 为 `gemini`，`last_chat_success` 为 `false`，`last_chat_error`
包含简短错误，不包含 API Key。

### Issue 4: Gemini JSON Parse Failure

#### 问题现象

Gemini HTTP 请求成功，但解析不到 answer。

#### 报错信息或异常返回

```json
"success": false,
"error_message": "failed to parse Gemini response text"
```

#### 原因分析

Stage 4 使用轻量 JSON 提取逻辑，保守搜索 Gemini 返回中的第一个 `text`
字符串字段。如果 Gemini 返回结构变化、被安全策略拦截或没有文本候选，解析会失败。

#### 修复方式

后续可引入完整 JSON parser，或增强对 Gemini `candidates`、`promptFeedback`
和 safety block 的解析。

#### 验证命令

```bash
curl http://127.0.0.1/api/metrics
```

#### 验证结果

WebServer 不崩溃，`last_chat_success=false`，`last_chat_error` 记录简短解析错误。

### Issue 5: VMware Ubuntu Cannot Reach Google / Gemini HTTPS 443

#### 问题现象

- `/api/health` 显示 `provider=gemini` 且 `gemini_api_key_configured=true`。
- `POST /api/chat` 返回 `success=false`。
- `error_message` 为：

```json
"Gemini HTTP request failed: Timeout was reached"
```

- 在虚拟机中直接执行以下命令：

```bash
curl -I https://www.google.com
curl -I https://generativelanguage.googleapis.com
```

均返回类似：

```text
Failed to connect ... port 443: 拒绝连接
```

#### 触发命令

```bash
sudo GEMINI_API_KEY="xxx" LIGHTAGENT_DEFAULT_PROVIDER="gemini" ./WebServer
curl -i http://127.0.0.1/api/health
curl -i -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello gemini"}'
curl -I https://www.google.com
curl -I https://generativelanguage.googleapis.com
```

#### 报错信息或异常返回

`/api/chat` 返回结构化错误 JSON，核心字段为：

```json
{
  "success": false,
  "provider": "gemini",
  "error_message": "Gemini HTTP request failed: Timeout was reached"
}
```

直接 curl Google / Gemini API 域名失败：

```text
Failed to connect ... port 443: 拒绝连接
```

#### 原因分析

- `GeminiProvider` 已经读取到 API Key，并进入真实 HTTP 调用分支。
- 失败原因不是 API Key 未配置，也不是 `ProviderFactory` 路由错误。
- 当前 VMware Ubuntu 无法访问 Google / Gemini API 的 HTTPS 443 端口，因此真实 Gemini 调用无法完成。

#### 临时解决

- 换到可访问 Google API 的网络环境。
- 或在宿主机配置代理，并让虚拟机走代理。
- 或后续使用 Ollama 本地模型作为 fallback。
- 或用 mock provider 完成无外网演示。

#### 验证命令

```bash
curl -i http://127.0.0.1/api/health
curl -i -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello gemini"}'
curl -I https://www.google.com
curl -I https://generativelanguage.googleapis.com
```

#### 验证结果

- `/api/health` 能返回 `phase-4`、`provider=gemini`、`gemini_api_key_configured=true`。
- `/api/chat` 能返回结构化错误 JSON，WebServer 不崩溃。
- `curl` 直接访问 Google API 域名失败，证明是网络环境问题。

#### 结论

- Stage 4 的 HTTP 调用链、配置读取、错误处理已经验证。
- 真实 Gemini answer 需要在能访问 `generativelanguage.googleapis.com` 的网络环境中进一步验证。

## Stage 5 Debug Notes

### Issue 1: Ollama Not Installed

#### 问题现象

执行 `ollama` 命令时提示命令不存在。

#### 触发命令

```bash
ollama serve
```

#### 报错信息或异常返回

```text
ollama: command not found
```

#### 原因分析

当前机器没有安装 Ollama。

#### 修复方式

安装 Ollama。安装完成后重新打开终端或确认 `ollama` 已加入 `PATH`。

#### 验证命令

```bash
ollama --version
```

#### 验证结果

能够输出 Ollama 版本。

### Issue 2: Ollama Service Not Running

#### 问题现象

`/api/chat` 返回 `success=false`，错误中包含 connection refused。

#### 触发命令

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" ./WebServer
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello ollama"}'
```

#### 报错信息或异常返回

```json
"error_message": "Ollama request failed: Couldn't connect to server"
```

#### 原因分析

Ollama 本地服务没有启动，`http://127.0.0.1:11434/api/generate` 无法连接。

#### 修复方式

```bash
ollama serve
```

#### 验证命令

```bash
curl http://127.0.0.1:11434/api/tags
```

#### 验证结果

Ollama 返回本地模型列表 JSON。

### Issue 3: Ollama Model Not Found

#### 问题现象

`/api/chat` 返回 `success=false`，Ollama 提示模型不存在。

#### 触发命令

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
  OLLAMA_MODEL="qwen2.5:0.5b" \
  ./WebServer
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

#### 原因分析

本地还没有拉取配置中的 Ollama 模型。

#### 修复方式

```bash
ollama pull qwen2.5:0.5b
```

#### 验证命令

```bash
ollama list
```

#### 验证结果

模型列表中出现 `qwen2.5:0.5b`。

### Issue 4: Gemini Network Unavailable, Fallback to Ollama

#### 问题现象

Gemini 返回 timeout 或 connection failed，但 `/api/chat` 继续尝试 Ollama。

#### 触发命令

```bash
sudo GEMINI_API_KEY="test_or_real_key" \
  LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
  LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
  OLLAMA_MODEL="qwen2.5:0.5b" \
  ./WebServer
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"用一句话介绍一下什么是 RAG"}'
```

#### 原因分析

当前 VMware Ubuntu 无法访问 `google.com` /
`generativelanguage.googleapis.com`，Gemini HTTP 调用失败。

#### 处理方式

当 `enable_ollama_fallback=true` 且 `fallback_provider=ollama` 时，
Gateway 自动尝试 `OllamaProvider`。

#### 验证命令

```bash
curl http://127.0.0.1/api/metrics
```

#### 验证结果

- Gemini 网络失败但 Ollama 正常时，最终响应 `provider` 为 `ollama`。
- `/api/chat` JSON 中出现 `fallback_from="gemini"` 和 `fallback_to="ollama"`。
- `/api/metrics` 中 `fallback_count` 增加。

#### 结论

可以在无 Google 网络环境下继续演示本地 AI Gateway 能力。

## Stage 6 Debug Notes

### Issue 1: knowledge_base Directory Not Found

#### 问题现象

首次启动 Stage 6 后，`POST /api/rag/query` 可能返回没有检索到内容，或者本地目录中还看不到知识库文件。

#### 触发命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"What is RAG?"}'
```

#### 报错信息或异常返回

```json
{
  "success": false,
  "error_message": "no relevant chunks found",
  "chunks": []
}
```

#### 原因分析

`KnowledgeBase::load()` 会按 `knowledge_base_dir` 扫描本地 `.txt` / `.md` 文件。目录不存在时会尝试创建目录，但空目录没有可检索内容。

#### 修复方式

在 `knowledge_base/` 下放入 `.txt` 或 `.md` 文件，或通过 `LIGHTAGENT_KB_DIR` / `config.json` 指向已有知识库目录。

#### 验证命令

```bash
ls knowledge_base
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 是什么？","top_k":3}'
```

#### 验证结果

响应 JSON 中 `chunks` 数组不为空，`retrieved_chunks` 计数大于 0。

### Issue 2: No Chunks Retrieved

#### 问题现象

知识库目录存在，但 `/api/rag/query` 返回 `success=false`，并提示 `no relevant chunks found`。

#### 触发命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"完全不相关的问题","top_k":3}'
```

#### 报错信息或异常返回

```json
"error_message": "no relevant chunks found"
```

#### 原因分析

Stage 6 只实现轻量 keyword matching，没有 embedding、向量数据库或语义召回。如果问题关键词没有出现在本地文档 chunk 中，就不会召回内容。

#### 修复方式

调整问题中的关键词，补充知识库文档，或后续 Stage 接入 embedding / vector store。

#### 验证命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"RAG retrieval context","top_k":3}'
```

#### 验证结果

返回的 `chunks` 中包含命中的文件路径、score、start_offset 和 preview。

### Issue 3: Ollama Not Running During RAG

#### 问题现象

`rag_provider=ollama` 或 `default_provider=ollama` 时，RAG 能召回 chunks，但最终回答失败。

#### 触发命令

```bash
LIGHTAGENT_DEFAULT_PROVIDER="ollama" ./WebServer
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？"}'
```

#### 报错信息或异常返回

```json
"error_message": "Ollama request failed: Couldn't connect to server"
```

#### 原因分析

`RagEngine` 已经完成检索并调用 `OllamaProvider`，但本地 `ollama serve` 没有启动，`http://127.0.0.1:11434/api/generate` 无法连接。

#### 修复方式

启动 Ollama 服务并确认模型已拉取。

```bash
ollama serve
ollama pull qwen2.5:0.5b
```

#### 验证命令

```bash
curl http://127.0.0.1:11434/api/tags
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 LightAgent Gateway？"}'
```

#### 验证结果

Ollama 正常时，`/api/rag/query` 返回 `success=true`，`provider="ollama"`，并保留 `chunks`。

### Issue 4: Gemini Unavailable During RAG, Fallback to Ollama

#### 问题现象

`rag_provider=gemini` 或 `default_provider=gemini` 时，Gemini 网络不可用，但希望本地 Ollama 接管回答。

#### 触发命令

```bash
GEMINI_API_KEY="test_or_real_key" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
./WebServer

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？"}'
```

#### 报错信息或异常返回

Gemini 网络失败时可能出现 timeout、connection refused 或 DNS/SSL 相关错误。

#### 原因分析

`RagEngine` 会先完成本地检索，再按 provider 调用 LLM。Gemini 失败且 `enable_ollama_fallback=true`、`fallback_provider=ollama` 时，会继续尝试 `OllamaProvider`。

#### 修复方式

确保 Ollama 本地服务可用，或切回 `mock` provider 做无外网演示。

#### 验证命令

```bash
curl http://127.0.0.1/api/metrics
```

#### 验证结果

RAG 响应中出现 `fallback_from="gemini"`、`fallback_to="ollama"`；metrics 中 `fallback_count` 增加，`last_rag_provider` 为 `ollama`。

### Issue 5: Oversized Document Skipped

#### 问题现象

某些知识库文件没有出现在 `/api/rag/query` 的 `chunks` 中。

#### 触发命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"大文件中的关键词"}'
```

#### 报错信息或异常返回

通常表现为 `chunks` 为空或缺少预期文件，不一定有显式错误。

#### 原因分析

Stage 6 为了避免一次性读取过大的本地文件，`KnowledgeBase` 会跳过超过 1MB 的文档；RAG prompt 还会受 `rag_max_context_chars` 限制，超出部分不会进入上下文。

#### 修复方式

将大文档拆成多个较小的 `.md` / `.txt` 文件，或后续优化为流式读取和更细粒度分块。

#### 验证命令

```bash
find knowledge_base -type f -size +1M
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"RAG","top_k":5}'
```

#### 验证结果

拆分后的文档可以被召回，响应中出现对应文件路径和 chunk preview。

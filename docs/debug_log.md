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

### Issue 6: Chinese Query With English Technical Term Misses RAG Chunks

#### 问题现象

Stage 6 初版 `/api/rag/query` 对英文关键词查询有效，例如
`RAG Retrieval Augmented Generation` 可以命中知识库；但中文问题
`什么是 RAG？` 返回空召回结果。

#### 触发命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'
```

#### 报错信息或异常返回

```json
{
  "success": false,
  "retrieved_chunks": [],
  "error_message": "no relevant chunks found"
}
```

#### 原因分析

初版检索逻辑主要依赖简单字符串匹配和空格切词。中文问题通常没有稳定空格；
`RAG？` 还带中文问号，导致 query term 无法直接匹配知识库中的 `RAG`。

#### 修复方式

- 增加 `normalizeForSearch()`，统一小写英文并移除中英文标点。
- 增加 `extractSearchTerms()`，从中文句子中额外提取连续英文/数字关键词。
- 对 chunk content、file_path、title 统一 normalize。
- 对 `rag`、`lightagent`、`gateway`、`provider`、`gemini`、`ollama`
  等技术词提高命中分数。
- 只返回 `score > 0` 的 chunk，并按 score 降序排序。

#### 验证命令

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'
```

#### 验证结果

- `什么是 RAG？` 可以命中 `rag_intro.md`。
- `LightAgent Gateway 支持哪些 Provider？` 可以命中
  `lightagent_gateway.md`。
- `/api/rag/query` 返回 `success=true`、`retrieved_chunks` 非空，并继续调用
  configured provider 生成 answer；当 provider 为 Ollama 且服务正常时，会返回
  Ollama 生成的 answer。

### Issue 7: RAG Gemini Timeout Triggers Ollama Fallback But Final Result Fails

#### 问题现象

Stage 6 中，`default_provider=ollama` 时 RAG 可以成功检索并调用 Ollama；
但 `default_provider=gemini` 且 Gemini timeout 后，RAG fallback 到 Ollama 的
响应里出现：

- `fallback_from="gemini"`
- `fallback_to="ollama"`
- `provider="ollama"`
- `success=false`
- `error_message` 中显示 Gemini timeout 后 Ollama fallback failed

#### 触发命令

```bash
sudo GEMINI_API_KEY="test_key_123" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
OLLAMA_MODEL="qwen2.5:0.5b" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
LIGHTAGENT_REQUEST_TIMEOUT_MS="5000" \
./WebServer

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'
```

#### 报错信息或异常返回

```json
{
  "success": false,
  "provider": "ollama",
  "fallback_from": "gemini",
  "fallback_to": "ollama",
  "error_message": "Gemini failed: ...; Ollama fallback failed: ..."
}
```

#### 原因分析

排查 `RagEngine` 和 `ProviderFactory` 后确认：

- RAG fallback 分支确实被触发。
- fallback 分支调用的是同一个 `OllamaProvider`。
- fallback 分支复用了已经构造好的 RAG `ChatRequest`，没有把 Gemini 错误信息当成 prompt。
- `ChatRequest.message` 仍然是原始用户问题。
- `ChatRequest.system_prompt` 仍然是包含 retrieved chunks 的 RAG prompt。
- `ollama_model` 仍然来自配置中的 `qwen2.5:0.5b`。
- Ollama response 仍然按 `response` 字段解析。

实际问题是 timeout 配置复用：测试时为了让 Gemini 快速失败，设置了
`LIGHTAGENT_REQUEST_TIMEOUT_MS=5000`。初版 `OllamaProvider` 也使用同一个
`request_timeout_ms`，导致 RAG fallback 给 Ollama 的长 prompt 也只有 5 秒生成
时间。direct Ollama 使用默认 30000ms 时可以成功，但 fallback 测试场景下被
5 秒 timeout 截断。

#### 修复方式

- 新增 `gemini_request_timeout_ms` 和 `ollama_request_timeout_ms`。
- 新增环境变量：
  - `LIGHTAGENT_GEMINI_REQUEST_TIMEOUT_MS`
  - `LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS`
  - `GEMINI_REQUEST_TIMEOUT_MS`
  - `OLLAMA_REQUEST_TIMEOUT_MS`
- `GeminiProvider` 优先使用 `gemini_request_timeout_ms`，否则使用
  `request_timeout_ms`。
- `OllamaProvider` 优先使用 `ollama_request_timeout_ms`，默认 30000ms。
- `/api/health` 输出 effective Gemini/Ollama timeout，便于现场确认。
- Ollama fallback 失败时，错误信息增加 safe diagnostics：`base_url`、
  `model`、`timeout_ms`、HTTP status 和 HttpClient error message，不包含
  Gemini API Key。

#### 验证命令

```bash
cd ~/LightAgentGateway
make clean
make

systemctl status ollama
ollama list
curl http://127.0.0.1:11434/api/tags

sudo pkill WebServer

sudo GEMINI_API_KEY="test_key_123" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
OLLAMA_MODEL="qwen2.5:0.5b" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
LIGHTAGENT_REQUEST_TIMEOUT_MS="5000" \
LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS="30000" \
./WebServer
```

Open another VMware Ubuntu terminal:

```bash
curl http://127.0.0.1/api/health

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'

curl http://127.0.0.1/api/metrics
```

#### 验证结果预期

- `/api/health` 中 `request_timeout_ms=5000`，`ollama_request_timeout_ms=30000`。
- `/api/rag/query` 返回 `success=true`。
- `/api/rag/query` 返回 `provider="ollama"`。
- `/api/rag/query` 返回 `fallback_from="gemini"` 和 `fallback_to="ollama"`。
- `/api/rag/query` 返回非空 `retrieved_chunks` 和非空 `answer`。
- `/api/metrics` 中 `last_rag_success=true`，`last_rag_provider="ollama"`。

## Stage 7 Debug Notes

### Issue 1: Pseudo Stream vs Real Stream

#### 问题现象

Stage 7 需要新增 `/api/chat/stream` 和 `/api/rag/query/stream`，但当前
Reactor WebServer 的响应封装仍以一次性 `Content-Length` body 返回为主。

#### 原因分析

真实 token stream 需要 Provider 层支持 Ollama/Gemini 的 streaming API，并且
HTTP 层需要更细粒度 flush 或 chunked transfer 处理。当前阶段目标只是提供
SSE-like 接口形态，不重构底层网络架构。

#### 修复方式

先实现 pseudo streaming：复用 `/api/chat` 和 `/api/rag/query` 的完整生成逻辑，
拿到完整 answer 后再拆成多个 `data: {...}\n\n` 事件返回。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"用一句话介绍一下什么是 RAG"}'
```

#### 验证结果预期

返回多行 `data: {...}`，最后一行包含 `"done":true`。当前不是 Ollama/Gemini
token-by-token 真实流。

### Issue 2: Stream Content-Type and Header Limitations

#### 问题现象

SSE 通常需要 `Content-Type: text/event-stream`、`Cache-Control: no-cache` 和
持续连接；当前 `HttpData` 响应结构只直接支持 status、content-type 和 body。

#### 原因分析

为了避免重构 `HttpData` / Reactor 底层，本阶段只通过
`LightAgentGateway::Response.contentType` 设置
`text/event-stream; charset=utf-8`，仍由现有 HTTP 响应路径加
`Content-Length`。

#### 修复方式

保持现有响应封装，只保证：

- Content-Type 为 `text/event-stream; charset=utf-8`
- body 为 SSE-like `data: {...}\n\n`
- `curl -N` 可以看到 data 行

#### 验证命令

```bash
curl -i -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"hello stream"}'
```

#### 验证结果预期

响应 header 中出现 `Content-Type: text/event-stream; charset=utf-8`，body 中
出现 `data:` 行。

### Issue 3: UTF-8 Chunk Split

#### 问题现象

如果直接按字节切分中文 answer，可能把一个 UTF-8 中文字符切坏。

#### 原因分析

中文字符通常占多个字节，简单 `substr` 按固定字节切分会产生无效 UTF-8。

#### 修复方式

`splitTextForStream()` 按 UTF-8 leading byte 判断当前 code point 长度，只在
code point 边界切分。遇到异常字节时退化为单字节前进，保证不会死循环。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"请用中文介绍 RAG"}'
```

#### 验证结果预期

返回的 `delta` 不应出现明显乱码；最后仍包含 `"done":true`。

### Issue 4: Provider Failure in Stream

#### 问题现象

stream API 中 provider 可能失败，例如 Ollama 未启动、Gemini timeout、fallback
也失败。

#### 原因分析

stream API 复用 provider 调用链，provider 失败时不能崩溃，也不能返回半截无结构
文本。

#### 修复方式

新增 `buildStreamError()`，失败时返回 SSE error event，并追加 `done=true`。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{}'
```

#### 验证结果预期

返回：

```text
data: {"id":"lightagent-chat-stream-1","object":"chat.completion.chunk","success":false,"error_message":"message field is required"}

data: {"id":"lightagent-chat-stream-1","object":"chat.completion.chunk","done":true}
```

### Issue 5: RAG Stream No Chunks

#### 问题现象

`/api/rag/query/stream` 如果知识库没有命中，可能没有 answer 可拆分。

#### 原因分析

RAG 依赖 `KnowledgeBase::search()` 的本地关键词检索。没有 chunks 时，
`RagEngine` 返回 `success=false` 和 `no relevant chunks found`。

#### 修复方式

RAG stream 复用 `executeRagInternal()`，失败时返回 SSE error event，并追加
`done=true`，不崩溃。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/rag/query/stream \
  -H "Content-Type: application/json" \
  -d '{"question":"一个知识库里完全不存在的问题","top_k":3}'
```

#### 验证结果预期

返回 SSE error event，包含 `error_message`，并以 `done=true` 结束。

## Stage8 Debug Notes

### Issue 1: Upstream Real Stream vs Downstream Buffered SSE

#### 问题现象

Stage8 需要真实读取 Ollama `stream=true`，但当前 WebServer 响应路径仍以一次性
body 写回为主。

#### 原因分析

真正向浏览器逐 token flush 需要改造 Reactor 写回时机、HTTP chunked transfer
或连接生命周期。为避免大规模重构底层网络模块，本阶段只实现 upstream real
stream。

#### 修复方式

`HttpClient::postJsonStream(...)` 真实读取 Ollama streaming JSON lines；
Gateway 将这些 deltas 转换成 SSE-style `data:` events，但下游仍一次性返回
buffered SSE body。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"用三句话介绍一下 RAG"}'
```

#### 验证结果预期

返回中出现 `stream_mode="upstream_real"` 和
`upstream_stream_mode="ollama_stream_true"`。

### Issue 2: Ollama Stream JSON Line Parse Issue

#### 问题现象

Ollama `stream=true` 返回多行 JSON，每行可能包含 `response`，最后一行可能只有
`done=true` 或统计字段。某些行解析不到 `response`。

#### 原因分析

streaming 响应不是单个 JSON object，而是 newline-delimited JSON。最后的 done
行可能没有非空 `response`。

#### 修复方式

新增：

- `extractOllamaStreamDelta(...)`
- `extractOllamaStreamDone(...)`

解析不到 `response` 的行不会导致崩溃；只要有有效 delta，就可以成功返回。
如果所有行都没有 delta，则返回结构化错误。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

#### 验证结果预期

多条 delta event 后有 `done=true`。

### Issue 3: Ollama Service Not Running

#### 问题现象

当 Ollama 未启动时，stream 请求失败。

#### 原因分析

`OllamaProvider::streamChat(...)` 请求
`http://127.0.0.1:11434/api/generate`，服务未启动会导致连接失败。

#### 修复方式

`HttpClient::postJsonStream(...)` 返回 curl error；
`OllamaProvider::streamChat(...)` 包装为 safe error message；
Gateway 返回 SSE error event，并以 `done=true` 结束。

#### 验证命令

```bash
sudo systemctl stop ollama

curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

#### 验证结果预期

返回 SSE error event，不崩溃；错误中包含 `base_url`、`model`、`timeout_ms`
等安全诊断，不包含 API Key。

### Issue 4: UTF-8 Delta Handling

#### 问题现象

Ollama delta 中可能包含中文、换行和引号。

#### 原因分析

SSE body 内嵌 JSON，需要对 delta 做 JSON escape，否则引号、换行会破坏 JSON。

#### 修复方式

所有 stream delta 都通过 `escapeJsonString(...)` 输出。中文 UTF-8 字节原样保留；
换行、引号、反斜杠会被转义。

#### 验证命令

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"请用中文介绍 RAG，并包含双引号"}'
```

#### 验证结果预期

返回的 `data:` JSON 行可被正常阅读，不出现破坏 JSON 的裸引号或裸换行。

### Issue 5: Fallback Stream Behavior

#### 问题现象

Gemini 网络失败后，需要 fallback 到 Ollama，并尽量使用 Ollama `stream=true`。

#### 原因分析

Stage7 fallback 可以生成完整 Ollama answer 再伪流式输出；Stage8 需要在 fallback
分支中优先调用 `OllamaProvider::streamChat(...)`。

#### 修复方式

`executeChatStreamInternal(...)` 和 `executeRagStreamInternal(...)` 在 Gemini
失败且 `enable_ollama_fallback=true`、`OLLAMA_STREAM=true` 时，调用
`OllamaProvider::streamChat(...)`。失败时返回 SSE error event。

#### 验证命令

```bash
sudo GEMINI_API_KEY="test_key_123" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
OLLAMA_MODEL="qwen2.5:0.5b" \
OLLAMA_STREAM="true" \
LIGHTAGENT_STREAM_MODE="upstream_real_ollama" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
LIGHTAGENT_REQUEST_TIMEOUT_MS="5000" \
LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS="30000" \
./WebServer
```

In another terminal:

```bash
curl -N -X POST http://127.0.0.1/api/rag/query/stream \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'

curl http://127.0.0.1/api/metrics
```

#### 验证结果预期

- stream response includes `provider="ollama"`.
- stream response includes `fallback_from="gemini"` and `fallback_to="ollama"`.
- stream response includes `stream_mode="upstream_real"`.
- metrics includes `last_upstream_stream_mode="ollama_stream_true"`.

## Stage9 Documentation Notes

### Issue 1: Documentation Polish, Not Runtime Bug Fix

#### 问题现象

After Stage1 to Stage8, the project had working code and detailed development
logs, but it still lacked a GitHub-facing README and interview-oriented summary
documents.

#### 原因分析

The project evolved through multiple stages. Without a consolidated README, it
is hard to quickly explain:

- what the project is
- which WebServer modules were preserved
- which AI Gateway capabilities were added
- which features are verified
- which limitations are caused by local network or stage scope

#### 修复方式

Added and updated documentation:

- `README.md`
- `docs/project_summary.md`
- `docs/interview_notes.md`
- Stage9 section in `docs/development_log.md`
- Stage9 section in `docs/debug_log.md`

### Issue 2: Gemini Network Limitation Must Be Stated Clearly

#### 问题现象

GeminiProvider can read config and perform HTTP request attempts, but the current
VMware Ubuntu network cannot access `google.com` /
`generativelanguage.googleapis.com`.

#### 原因分析

This is an environment/network limitation, not an API key parsing problem or
ProviderFactory routing problem.

#### 修复方式

README and summary docs now state that real Gemini answers are pending in the
current VM environment, while key detection, missing-key errors, network failure
errors, and Gemini to Ollama fallback have been verified.

### Issue 3: sudo Environment Variables

#### 问题现象

Running `export GEMINI_API_KEY=...` and then `sudo ./WebServer` may lose the
environment variable.

#### 原因分析

`sudo` does not preserve normal user environment variables by default.

#### 修复方式

README uses inline sudo environment variables, for example:

```bash
sudo GEMINI_API_KEY="your_key" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
./WebServer
```

### Issue 4: Port 80 Requires sudo

#### 问题现象

Normal users may fail to start the server when it listens on port 80.

#### 原因分析

Binding to privileged ports below 1024 usually requires elevated permissions on
Linux.

#### 修复方式

README and project summary now mention that the current default port commonly
requires `sudo`, and future work includes changing the development port to 8080.

### Issue 5: GitHub 443 Network Fluctuation

#### 问题现象

Syncing code with `git pull` may be blocked or slow if GitHub 443 access is
unstable in the VM.

#### 原因分析

This is an environment network issue outside the gateway runtime.

#### 修复方式

The final instructions mention that code can also be synchronized via VMware
shared folders or rsync when GitHub 443 is blocked.

### Issue 6: Ollama Service Already Running

#### 问题现象

Running `ollama serve` manually may fail because port `11434` is already in use.

#### 原因分析

Ollama may already be running as a systemd service.

#### 修复方式

README recommends checking:

```bash
systemctl status ollama
curl http://127.0.0.1:11434/api/tags
```

before starting or debugging Ollama manually.

### Issue 7: RAG Chinese Search Initial Limitation

#### 问题现象

The initial local RAG keyword search worked better for English queries than for
Chinese natural language questions such as `什么是 RAG？`.

#### 原因分析

The initial search used simple string matching and space-based tokenization.
Chinese punctuation and no-space queries made terms like `RAG？` hard to match.

#### 修复方式

The docs now record the implemented normalization:

- lowercase ASCII
- remove Chinese and English punctuation
- extract English/digit technical terms from Chinese text
- score content, title, and file path

### Issue 8: Stream Token Granularity

#### 问题现象

Stage8 supports Ollama upstream real streaming, but downstream client behavior is
still not a strict token-by-token flush from the Reactor write loop.

#### 原因分析

The original WebServer response path builds a complete response body. True
downstream streaming would require deeper write-path changes.

#### 修复方式

README and docs now state the accurate scope:

- upstream Ollama `stream=true` is implemented
- downstream response is SSE-style and may still be buffered
- real downstream flush is future work

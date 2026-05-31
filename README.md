# LightAgent Gateway

LightAgent Gateway 是一个基于 C++17 Reactor WebServer 二次开发的轻量级 AI 应用网关。项目在保留原有静态资源服务能力的基础上，扩展了 LLM Provider、本地文件型 RAG、Gemini 到 Ollama 的 fallback、运行指标统计，以及 SSE 风格的流式输出接口。

## 1. 项目背景

本项目基于 LinYa/WebServer 进行二次开发。原项目主要是一个 C++ Reactor WebServer，包含 Epoll、非阻塞 Socket、Channel、EventLoop、Timer 和静态资源服务等能力。

LightAgent Gateway 的目标不是重写 WebServer，而是在已有 Reactor 架构之上增加 AI Gateway 业务层：

- 保留原有 Reactor / Epoll / EventLoop / Channel / Timer 等底层模块；
- 保留 `/hello`、`/favicon.ico` 和静态资源访问；
- 新增 health、metrics、chat、RAG、streaming 等 API 路由；
- 通过 Provider 抽象支持多种 LLM 后端；
- 当 Gemini 不可用时，自动 fallback 到本地 Ollama，保证演示和本地开发可用。

## 2. 功能特性

- 升级到 C++17；
- 保留 Reactor WebServer 基础架构；
- 保留原有静态文件服务能力；
- 扩展 API Server；
- 支持健康检查接口；
- 支持运行指标接口；
- 支持 `MockProvider`；
- 支持 `GeminiProvider`；
- 支持 `OllamaProvider`；
- 支持 `ProviderFactory`；
- 支持 Gemini 失败后 fallback 到本地 Ollama；
- 支持本地文件型 RAG；
- 支持中文问题中英文技术词检索优化；
- 支持 `POST /api/chat`；
- 支持 `POST /api/chat/stream`；
- 支持 `POST /api/rag/query`；
- 支持 `POST /api/rag/query/stream`；
- 支持 Ollama 上游真实 `stream=true`；
- 支持 JSON 结构化错误返回；
- 维护 `docs/development_log.md` 和 `docs/debug_log.md`。

## 3. 架构说明

整体调用链：

```text
Client
  -> LightAgent Gateway
  -> API Router
  -> API Handler
  -> ProviderFactory
  -> MockProvider / GeminiProvider / OllamaProvider
  -> Response / Stream Response
```

RAG 流程：

```text
User Question
  -> KnowledgeBase
  -> Chunk Retrieval
  -> Context Prompt
  -> LLM Provider
  -> Answer
```

Fallback 流程：

```text
GeminiProvider
  -> HTTP failure / timeout
  -> OllamaProvider
  -> local qwen2.5:0.5b
  -> Answer
```

Streaming 流程：

```text
Client
  -> /api/chat/stream 或 /api/rag/query/stream
  -> Ollama stream=true
  -> Upstream JSON lines
  -> SSE-style data chunks
```

Stage8 已经实现 Ollama 上游真实流式读取。由于原 WebServer 的写回路径尚未改造成逐 token flush，下游返回目前仍是一次性 buffered SSE-style body。

## 4. API 概览

| Method | Path | 说明 |
| --- | --- | --- |
| GET | `/hello` | 原始 demo/static endpoint |
| GET | `/favicon.ico` | 原始静态资源 |
| GET | `/api/health` | 服务健康状态和配置摘要 |
| GET | `/api/metrics` | 运行时指标 |
| POST | `/api/chat` | 普通聊天接口 |
| POST | `/api/chat/stream` | SSE 风格聊天流式接口 |
| POST | `/api/rag/query` | 本地文件 RAG 查询 |
| POST | `/api/rag/query/stream` | SSE 风格 RAG 流式查询 |

## 5. 配置说明

配置优先级：

1. 环境变量；
2. `config.json`；
3. `GatewayConfig` 中的默认值。

主要环境变量：

- `GEMINI_API_KEY`
- `LIGHTAGENT_DEFAULT_PROVIDER`
- `LIGHTAGENT_ENABLE_OLLAMA_FALLBACK`
- `OLLAMA_BASE_URL`
- `OLLAMA_MODEL`
- `OLLAMA_STREAM`
- `LIGHTAGENT_KB_DIR`
- `LIGHTAGENT_STREAM_MODE`
- `LIGHTAGENT_STREAM_CHUNK_SIZE`
- `LIGHTAGENT_REQUEST_TIMEOUT_MS`
- `LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS`

注意事项：

- 不要提交 `config.json`；
- 使用 `config.example.json` 作为配置模板；
- `/api/health` 和 `/api/metrics` 不会明文返回 API Key；
- 使用 `sudo` 启动时建议把环境变量写在同一条命令里，因为 `sudo` 默认可能不会保留普通用户环境变量。

## 6. 编译

Linux 依赖：

```bash
sudo apt update
sudo apt install -y build-essential make g++ libcurl4-openssl-dev
```

使用 Makefile 编译：

```bash
make clean
make
```

如果本地副本中 `make clean` 不可用，可以直接执行：

```bash
make
```

也可以使用 CMake：

```bash
cmake -S . -B build
cmake --build build
```

## 7. 使用 Ollama 运行

安装并准备 Ollama：

```bash
curl -fsSL https://ollama.com/install.sh | sh
systemctl status ollama
ollama pull qwen2.5:0.5b
ollama list
curl http://127.0.0.1:11434/api/tags
```

启动 Gateway：

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
OLLAMA_MODEL="qwen2.5:0.5b" \
OLLAMA_STREAM="true" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
./WebServer
```

说明：

- 当前项目通常监听 80 端口，因此 Linux 下通常需要 `sudo`；
- 后续可以将开发端口改为 8080，避免每次启动都需要 `sudo`；
- Ollama 默认运行在 `http://127.0.0.1:11434`。

## 8. 使用 Gemini 运行

```bash
sudo GEMINI_API_KEY="your_key" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
OLLAMA_MODEL="qwen2.5:0.5b" \
./WebServer
```

当前环境限制：

- 当前 VMware Ubuntu 环境无法访问 `google.com` / `generativelanguage.googleapis.com`；
- 因此 Gemini 真实 answer 在当前 VM 网络中尚未完成成功验证；
- 已验证的 Gemini 相关能力包括：API Key 检测、无 Key 安全报错、网络失败结构化错误、Gemini 失败后 fallback 到 Ollama。

## 9. Curl 示例

健康检查：

```bash
curl http://127.0.0.1/api/health
```

运行指标：

```bash
curl http://127.0.0.1/api/metrics
```

普通 Chat：

```bash
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

Chat Stream：

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"用三句话介绍一下 RAG"}'
```

RAG Query：

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'
```

RAG Stream：

```bash
curl -N -X POST http://127.0.0.1/api/rag/query/stream \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'
```

## 10. 已验证结果

阶段开发过程中已经验证：

- `/api/health` 可以返回当前阶段信息；
- `/api/chat` 可以调用本地 Ollama；
- `/api/rag/query` 可以处理 `什么是 RAG？` 这类中文查询；
- `/api/chat/stream` 可以返回 SSE-style `data:` chunks；
- `/api/rag/query/stream` 可以返回 SSE-style `data:` chunks；
- Gemini 失败后可以 fallback 到 Ollama；
- `/api/metrics` 可以记录请求数、fallback、RAG 和 stream 指标；
- 原始 `/hello` 和 `/favicon.ico` 保持可用。

重要说明：

- Gemini 真实 answer 因当前 VM 网络无法访问 Google API endpoint，暂未完成成功验证；
- 上游 Ollama 已使用 `stream=true`，但下游 client 侧仍是 buffered SSE-style response；
- 当前 RAG 有意避免引入重量级向量数据库，先保留本地文件关键词检索方案。

## 11. 开发阶段

- Stage1：API Server 扩展；
- Stage2：配置层和 Provider 抽象；
- Stage3：GeminiProvider 骨架；
- Stage4：Gemini HTTP Client；
- Stage5：OllamaProvider 和 Gemini fallback；
- Stage6：本地文件型 RAG 和中文关键词检索优化；
- Stage7：伪流式 API；
- Stage8：Ollama 上游真实流式读取；
- Stage9：README 和项目文档整理。

详细记录见：

- `docs/development_log.md`
- `docs/debug_log.md`

## 12. 已知限制

- 当前默认端口通常是 80，Linux 下需要 `sudo`；
- RAG 是本地文件关键词检索，不是向量检索；
- 尚未接入 FAISS / Milvus / Chroma；
- Gemini 真实调用受当前 VMware 网络环境限制；
- 上游 Ollama `stream=true` 已实现，但下游逐 token flush 受原 WebServer 响应路径限制；
- `qwen2.5:0.5b` 是轻量模型，回答质量有限；
- JSON 解析是轻量实现，不是完整 JSON parser。

## 13. 后续计划

- 将默认开发端口从 80 改为 8080；
- 增加 Dockerfile / docker-compose；
- 接入真正的向量检索；
- 改进中文分词；
- 增加前端演示页面；
- 实现真正的下游 streaming flush；
- 增加 CI 构建检查；
- 增加结构化日志。

## 14. 简历亮点

基于 C++17 对 LinYa/WebServer 进行二次开发，保留原 Reactor / Epoll / 非阻塞 Socket 架构，扩展实现 LightAgent Gateway。新增 LLM Provider 抽象层，支持 Mock、Gemini、Ollama 多 Provider 切换，并实现 Gemini 调用失败自动 fallback 到本地 Ollama；实现本地文件型 RAG 查询接口、中文关键词检索优化、metrics 统计和 SSE 风格流式输出接口；支持 Ollama upstream `stream=true`，适合展示 C++ 网络服务向 AI 应用网关演进的工程化能力。

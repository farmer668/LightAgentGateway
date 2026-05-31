# LightAgent Gateway Project Summary

## 项目定位

LightAgent Gateway 是一个基于 C++17 Reactor WebServer 二次开发的轻量级 AI 应用网关。它不是从零重写 WebServer，而是在原有 LinYa/WebServer 的 Reactor / Epoll / 非阻塞 Socket / EventLoop 架构上扩展 AI Gateway 业务层。

项目目标是把一个传统 C++ WebServer 演进为面向 RAG / Agent 场景的服务网关，支持 LLM Provider、fallback、本地知识库检索、metrics 和流式输出接口。

## 技术栈

- C++17
- Reactor pattern
- Epoll ET
- Non-blocking socket
- EventLoop / Channel / Timer
- libcurl
- Local Ollama
- Gemini API skeleton and HTTP path
- Local file based RAG
- SSE-style streaming response
- Lightweight JSON parsing

## 核心模块

| Module | Responsibility |
| --- | --- |
| Reactor WebServer | 原始网络 IO、连接管理、静态资源服务 |
| LightAgentGateway | API routing and gateway business logic |
| GatewayConfig | 环境变量、config.json、默认值配置 |
| ProviderFactory | 根据配置选择 Mock/Gemini/Ollama provider |
| MockProvider | 演示和无外部依赖测试 |
| GeminiProvider | Gemini HTTP 调用和错误处理 |
| OllamaProvider | 本地 Ollama chat 和 stream 调用 |
| KnowledgeBase | 本地 `.txt` / `.md` 文件加载和检索 |
| RagEngine | RAG prompt 构造、provider 调用、fallback |
| StreamUtil | SSE-style stream event 构造 |
| HttpClient | libcurl JSON POST and stream response collection |

## API 列表

| Method | Path | Description |
| --- | --- | --- |
| GET | `/hello` | 原始 demo endpoint |
| GET | `/favicon.ico` | 原始静态资源 |
| GET | `/api/health` | 服务健康和配置摘要 |
| GET | `/api/metrics` | 运行指标 |
| POST | `/api/chat` | 普通聊天 |
| POST | `/api/chat/stream` | 聊天流式接口 |
| POST | `/api/rag/query` | 本地文件 RAG 查询 |
| POST | `/api/rag/query/stream` | RAG 流式接口 |

## 已验证功能

- 原有 `/hello`、`/favicon.ico` 保留可用。
- `/api/health` 返回服务名、版本、阶段、provider、RAG、stream 配置。
- `/api/metrics` 记录 chat、RAG、fallback、stream 相关指标。
- `/api/chat` 可以调用 Ollama 本地模型。
- Gemini 网络失败时可以 fallback 到 Ollama。
- `/api/rag/query` 可以检索 `knowledge_base` 下的示例文档。
- 中文问题中夹杂英文技术词时可以命中，例如 `什么是 RAG？`。
- `/api/chat/stream` 和 `/api/rag/query/stream` 返回 SSE-style `data:` events。
- Ollama upstream `stream=true` 已接入。

## 已知限制

- 当前 RAG 是关键词检索，不是向量检索。
- 未接入 FAISS / Milvus / Chroma。
- 当前 VMware Ubuntu 网络无法访问 Google API，Gemini real answer 未在该环境验证。
- 上游 Ollama streaming 已实现，但下游仍是 buffered SSE body。
- 默认监听 80 端口，Linux 下通常需要 `sudo`。
- `qwen2.5:0.5b` 是轻量模型，回答质量有限。

## 面试讲解思路

可以按三层讲：

1. 底层网络层：保留原 Reactor WebServer，说明没有重写 EventLoop / Channel / Epoll。
2. 网关业务层：新增 API router、config、ProviderFactory、health、metrics。
3. AI 能力层：Mock/Gemini/Ollama provider、本地 RAG、fallback、stream。

重点强调工程取舍：

- 为什么不重写底层：降低风险，保留已有稳定 IO 架构。
- 为什么需要 fallback：Gemini 受网络和额度影响，本地 Ollama 保证演示可用。
- 为什么先做文件型 RAG：阶段性控制复杂度，避免过早引入向量数据库。
- 为什么 Stage8 只做 upstream real stream：不大改 Reactor 写回机制，先验证 provider streaming 能力。

## 简历写法

基于 C++17 对 LinYa/WebServer 进行二次开发，保留原 Reactor / Epoll / 非阻塞 Socket 架构，扩展实现 LightAgent Gateway。新增 LLM Provider 抽象层，支持 Mock、Gemini、Ollama 多 Provider 切换，并实现 Gemini 调用失败自动 fallback 到本地 Ollama；实现本地文件型 RAG 查询接口、中文关键词检索优化、metrics 统计和 SSE 风格流式输出接口；接入 Ollama upstream `stream=true`，完成从传统 C++ WebServer 到 AI 应用网关的工程化演进。

# LightAgent Gateway Interview Notes

## 1. 一句话介绍项目

LightAgent Gateway 是我基于 C++17 对 LinYa/WebServer 做的二次开发项目，它保留原来的 Reactor / Epoll / 非阻塞 Socket 架构，在业务层扩展了 LLM Provider、Gemini 到 Ollama fallback、本地文件 RAG、metrics 和流式输出接口。

## 2. 为什么基于原 WebServer 二次开发，而不是重写

我的目标不是重新造一个 WebServer，而是模拟真实工程里的增量演进。原项目已经有 Reactor、Epoll、Channel、EventLoop、Timer 和静态资源服务能力，所以我保留这些底层模块，把主要改动集中在 AI Gateway 业务层。

这样做有两个好处：

- 风险更低，不破坏已有网络 IO 和静态资源能力。
- 更接近真实工程改造：在老系统上扩展新能力，而不是推倒重来。

## 3. Reactor 部分保留了什么

底层保留了：

- Epoll ET
- non-blocking socket
- Channel
- EventLoop
- EventLoopThreadPool
- Server
- Timer
- 原有静态资源服务

我主要在 `LightAgentGateway`、Provider、RAG 和配置层做增量开发。

## 4. AI Gateway 新增了什么

新增能力包括：

- API endpoints: `/api/health`、`/api/metrics`、`/api/chat`、`/api/rag/query`
- stream endpoints: `/api/chat/stream`、`/api/rag/query/stream`
- `GatewayConfig` 配置层
- `ProviderFactory`
- `MockProvider`
- `GeminiProvider`
- `OllamaProvider`
- 本地文件型 `KnowledgeBase`
- `RagEngine`
- metrics 统计
- SSE-style stream response

## 5. ProviderFactory 怎么设计

ProviderFactory 根据 `GatewayConfig.default_provider` 创建 provider：

- `mock` -> `MockProvider`
- `gemini` -> `GeminiProvider`
- `ollama` -> `OllamaProvider`

这样 `/api/chat` 和 RAG 不需要直接依赖某个具体 provider。后续如果加 OpenAI、Claude、本地 vLLM，也可以沿用这个抽象。

## 6. Gemini 失败为什么 fallback 到 Ollama

Gemini 适合演示云端 LLM API，但它依赖网络和 API key，也可能受额度影响。当前 VMware Ubuntu 环境访问 Google API 失败，所以如果只依赖 Gemini，项目展示会不可控。

Ollama 是本地模型服务，可以在没有外网的情况下继续演示 chat、RAG 和 stream。因此我实现了 Gemini 网络失败或 timeout 后 fallback 到 Ollama 的机制。

## 7. RAG 是怎么做的

当前 RAG 是本地文件型 RAG：

1. 从 `knowledge_base/` 加载 `.txt` / `.md` 文件。
2. 按配置的 chunk size 切分文档。
3. 对用户问题做 normalize 和关键词提取。
4. 对 chunk content、title、file_path 打分。
5. 选 top_k chunks。
6. 构造带 Context 的 prompt。
7. 调用 LLM Provider 生成答案。

这个版本没有引入向量数据库，重点是把 RAG 主链路跑通，并保持 C++ 工程可控。

## 8. 为什么当前不使用向量数据库

当前项目阶段更强调 WebServer 到 AI Gateway 的演进，包括 provider 抽象、fallback、RAG API、metrics 和 stream。如果一开始就接 FAISS / Milvus / Chroma，会增加部署和依赖复杂度。

所以我先实现文件型关键词检索，验证 RAG 的 API 和工程链路。后续可以把 `KnowledgeBase::search()` 替换为 embedding + vector search，而 API 层和 RagEngine 的主体结构可以保留。

## 9. stream 是怎么实现的

Stage7 先实现 pseudo streaming：先拿到完整 answer，再切成多个 SSE-style `data:` events。

Stage8 增加了 Ollama upstream real streaming：

- 请求 Ollama `/api/generate`
- 设置 `"stream": true`
- 读取 Ollama 返回的 newline JSON
- 每行提取 `response` 字段作为 delta
- 转成 SSE-style `data:` event

需要说明的是：当前上游 Ollama 是真实 streaming，但下游 WebServer 仍然是 buffered SSE body，没有重构 Reactor 写回 flush 机制。

## 10. 当前项目不足

- 默认监听 80 端口，需要 sudo。
- RAG 是关键词检索，不是向量检索。
- 没有接 FAISS / Milvus / Chroma。
- Gemini real answer 受当前 VM 网络限制，没有完成真实成功验证。
- 下游 streaming 还不是严格的 token-by-token flush。
- JSON parser 是轻量实现，不是完整 JSON 库。
- qwen2.5:0.5b 是轻量模型，回答质量有限。

## 11. 后续优化

可以继续做：

- 默认端口改 8080。
- Dockerfile / docker-compose。
- 接入 embedding 和 vector DB。
- 更好的中文分词。
- 真正的 downstream streaming flush。
- 前端演示页面。
- CI build。
- structured logging。
- 更严格的 JSON parser。

## 12. 面试回答重点

面试时可以重点讲这几个点：

- 我没有重写底层 Reactor，而是在稳定网络层上做业务扩展。
- ProviderFactory 解耦了网关和具体模型服务。
- Gemini fallback 到 Ollama 解决了网络不可用和演示稳定性问题。
- RAG 从文件型关键词检索开始，保留了后续替换向量检索的扩展点。
- stream 分阶段实现：先伪流式，再 upstream real Ollama streaming，后续再优化 downstream flush。
- 整个项目有 health、metrics、debug log 和 development log，体现工程化闭环。

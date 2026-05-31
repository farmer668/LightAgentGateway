# LightAgent Gateway

LightAgent Gateway is a lightweight AI application gateway built by extending a C++17 Reactor WebServer. It preserves the original static file service while adding LLM providers, local file based RAG, Gemini to Ollama fallback, metrics, and SSE-style streaming APIs.

## 1. Project Background

This project is based on LinYa/WebServer. The original code focuses on a C++ Reactor WebServer with Epoll, non-blocking sockets, Channel, EventLoop, Timer, and static resource serving.

The goal of LightAgent Gateway is not to rewrite the WebServer. Instead, it keeps the existing Reactor foundation and adds an AI Gateway business layer on top:

- keep the original Reactor / Epoll / EventLoop / Channel / Timer modules
- preserve `/hello`, `/favicon.ico`, and static resource serving
- add API routing for health, metrics, chat, RAG, and streaming
- support multiple LLM providers through a provider abstraction
- provide local Ollama fallback when Gemini is unavailable

## 2. Features

- C++17 upgrade
- Reactor WebServer base preserved
- Static file serving preserved
- API Server extension
- Health check endpoint
- Runtime metrics endpoint
- `MockProvider`
- `GeminiProvider`
- `OllamaProvider`
- `ProviderFactory`
- Gemini failure fallback to local Ollama
- Local file based RAG
- Chinese keyword search normalization
- `POST /api/chat`
- `POST /api/chat/stream`
- `POST /api/rag/query`
- `POST /api/rag/query/stream`
- Ollama upstream real streaming with `stream=true`
- JSON structured error handling
- Development and debug logs in `docs/`

## 3. Architecture

```text
Client
  -> LightAgent Gateway
  -> API Router
  -> API Handler
  -> ProviderFactory
  -> MockProvider / GeminiProvider / OllamaProvider
  -> Response / Stream Response
```

RAG flow:

```text
User Question
  -> KnowledgeBase
  -> Chunk Retrieval
  -> Context Prompt
  -> LLM Provider
  -> Answer
```

Fallback flow:

```text
GeminiProvider
  -> HTTP failure / timeout
  -> OllamaProvider
  -> local qwen2.5:0.5b
  -> Answer
```

Streaming flow:

```text
Client
  -> /api/chat/stream or /api/rag/query/stream
  -> Ollama stream=true
  -> Upstream JSON lines
  -> SSE-style data chunks
```

Stage8 implements upstream real Ollama streaming. The downstream WebServer response is still a buffered SSE-style body because the original Reactor write path has not been refactored for token-by-token flush.

## 4. API Overview

| Method | Path | Description |
| --- | --- | --- |
| GET | `/hello` | Original demo/static endpoint |
| GET | `/favicon.ico` | Original static resource |
| GET | `/api/health` | Service health and config summary |
| GET | `/api/metrics` | Runtime metrics |
| POST | `/api/chat` | Normal chat completion |
| POST | `/api/chat/stream` | SSE-style streaming chat |
| POST | `/api/rag/query` | Local file RAG query |
| POST | `/api/rag/query/stream` | SSE-style streaming RAG query |

## 5. Configuration

Configuration priority:

1. Environment variables
2. `config.json`
3. Default values in `GatewayConfig`

Main environment variables:

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

Notes:

- Do not commit `config.json`.
- Use `config.example.json` as a template.
- API keys are not returned in plaintext by `/api/health` or `/api/metrics`.
- Use environment variables when running with `sudo`, because normal user environment variables may not be preserved by default.

## 6. Build

Linux dependencies:

```bash
sudo apt update
sudo apt install -y build-essential make g++ libcurl4-openssl-dev
```

Build:

```bash
make clean
make
```

If `make clean` is not available in your local copy:

```bash
make
```

CMake is also supported:

```bash
cmake -S . -B build
cmake --build build
```

## 7. Run with Ollama

Install and prepare Ollama:

```bash
curl -fsSL https://ollama.com/install.sh | sh
systemctl status ollama
ollama pull qwen2.5:0.5b
ollama list
curl http://127.0.0.1:11434/api/tags
```

Start the gateway:

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
OLLAMA_MODEL="qwen2.5:0.5b" \
OLLAMA_STREAM="true" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
./WebServer
```

Notes:

- The current project commonly listens on port 80, so Linux usually requires `sudo`.
- A future improvement is changing the development port to 8080 to avoid `sudo`.
- Ollama runs locally at `http://127.0.0.1:11434`.

## 8. Run with Gemini

```bash
sudo GEMINI_API_KEY="your_key" \
LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
OLLAMA_MODEL="qwen2.5:0.5b" \
./WebServer
```

Known environment limitation:

- The current VMware Ubuntu environment cannot access `google.com` / `generativelanguage.googleapis.com`.
- Real Gemini answers have not been verified in that VM network.
- Verified Gemini-related behavior includes API key detection, missing-key safe errors, network failure structured errors, and Gemini failure fallback to Ollama.

## 9. Curl Examples

Health:

```bash
curl http://127.0.0.1/api/health
```

Metrics:

```bash
curl http://127.0.0.1/api/metrics
```

Chat:

```bash
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

Chat stream:

```bash
curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"用三句话介绍一下 RAG"}'
```

RAG query:

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'
```

RAG stream:

```bash
curl -N -X POST http://127.0.0.1/api/rag/query/stream \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'
```

## 10. Verified Results

The project has verified the following behavior during staged development:

- `/api/health` returns the current phase information.
- `/api/chat` works with Ollama.
- `/api/rag/query` works with Chinese queries such as `什么是 RAG？`.
- `/api/chat/stream` returns SSE-style `data:` chunks.
- `/api/rag/query/stream` returns SSE-style `data:` chunks.
- Gemini failure can fallback to Ollama.
- `/api/metrics` records requests, fallback, RAG, and stream metrics.
- Original `/hello` and `/favicon.ico` are preserved.

Important notes:

- Real Gemini answers are pending because the current VM network cannot reach Google API endpoints.
- Upstream Ollama uses `stream=true`; downstream client streaming is still a buffered SSE-style response.
- The current RAG implementation intentionally avoids heavyweight vector databases.

## 11. Development Stages

- Stage1: API server extension
- Stage2: config and provider abstraction
- Stage3: Gemini provider skeleton
- Stage4: Gemini HTTP client
- Stage5: Ollama provider and Gemini fallback
- Stage6: local file RAG and Chinese keyword search improvements
- Stage7: pseudo streaming APIs
- Stage8: upstream real Ollama streaming
- Stage9: README and project documentation polish

Detailed records:

- `docs/development_log.md`
- `docs/debug_log.md`

## 12. Known Limitations

- The current default port is commonly 80, which requires `sudo` on Linux.
- RAG is local file keyword retrieval, not vector retrieval.
- FAISS / Milvus / Chroma are not integrated.
- Real Gemini calls are limited by the current VMware network environment.
- Upstream Ollama `stream=true` is implemented, but downstream token-by-token flush is limited by the original WebServer response path.
- `qwen2.5:0.5b` is a lightweight model, so answer quality is limited.
- JSON parsing is lightweight and not a full JSON parser.

## 13. Future Work

- Change the default development port from 80 to 8080.
- Add Dockerfile / docker-compose.
- Add real vector retrieval.
- Improve Chinese tokenization.
- Add a frontend demo page.
- Implement real downstream streaming flush.
- Add CI build checks.
- Add structured logging.

## 14. Resume Highlights

基于 C++17 对 LinYa/WebServer 进行二次开发，保留原 Reactor / Epoll / 非阻塞 Socket 架构，扩展实现 LightAgent Gateway。新增 LLM Provider 抽象层，支持 Mock、Gemini、Ollama 多 Provider 切换，并实现 Gemini 调用失败自动 fallback 到本地 Ollama；实现本地文件型 RAG 查询接口、中文关键词检索优化、metrics 统计和 SSE 风格流式输出接口；支持 Ollama upstream `stream=true`，适合展示 C++ 网络服务向 AI 应用网关演进的工程化能力。

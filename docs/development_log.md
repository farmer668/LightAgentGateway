# Development Log

## Stage 3 Goal

Add the GeminiProvider skeleton and provider selection infrastructure without
calling the real Gemini API. Keep MockProvider as the safe fallback and preserve
the Stage 1/2 HTTP surface:

- `GET /hello`
- `GET /favicon.ico`
- `GET /api/health`
- `GET /api/metrics`
- `POST /api/chat`

## Modified Files

- `CMakeLists.txt`
- `GeminiProvider.h`
- `GeminiProvider.cpp`
- `GatewayConfig.h`
- `LightAgentGateway.cpp`
- `MockProvider.cpp`
- `ProviderFactory.h`
- `ProviderFactory.cpp`
- `.gitignore`
- `config.example.json`
- `scripts/start_gateway.sh`
- `scripts/start_gateway_with_env.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## New Classes / Structs / Functions

- `GeminiProvider`
  - `GeminiProvider::GeminiProvider(const GatewayConfig&)`
  - `GeminiProvider::name()`
  - `GeminiProvider::chat(const ChatRequest&)`
- `ProviderFactory`
  - `ProviderFactory::create(const GatewayConfig&)`

## API Changes

- `POST /api/chat` now selects the provider through `ProviderFactory`.
- `default_provider=mock` continues to return the mock answer.
- `default_provider=gemini` uses `GeminiProvider`.
- Gemini without `GEMINI_API_KEY` returns JSON with:
  - `success: false`
  - `provider: "gemini"`
  - `error_message: "GEMINI_API_KEY is not configured"`
- Gemini with `GEMINI_API_KEY` returns JSON with:
  - `success: false`
  - `provider: "gemini"`
  - `error_message: "Gemini HTTP call is not implemented in Stage 3"`

## Config Changes

- Default `GatewayConfig.version` is now `0.3.0`.
- Default `GatewayConfig.stage` is now `phase-3`.
- Added `config.example.json`.
- `.gitignore` ignores local secrets and build outputs:
  - `config.json`
  - `*.log`
  - `core`
  - `core.*`
  - `build/`
  - `build-*/`
  - `*.o`
  - `WebServer`
  - `scripts/start_gateway_with_env.sh`

Configuration priority remains:

1. Environment variables
2. `config.json`
3. Built-in defaults

## Build Commands

```bash
cmake -S . -B build
cmake --build build --target WebServer -j 4
```

Or:

```bash
make
```

## Test Commands

```bash
curl -i http://127.0.0.1/hello
curl -i http://127.0.0.1/favicon.ico
curl -i http://127.0.0.1/api/health
curl -i http://127.0.0.1/api/metrics
curl -i -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello from mock"}'
```

Gemini skeleton without key:

```bash
LIGHTAGENT_DEFAULT_PROVIDER=gemini ./WebServer -p 8080 -l /tmp/WebServer.log
curl -i -X POST http://127.0.0.1:8080/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello gemini"}'
```

Gemini skeleton with test key:

```bash
GEMINI_API_KEY="test_key_123" LIGHTAGENT_DEFAULT_PROVIDER=gemini \
  ./WebServer -p 8080 -l /tmp/WebServer.log
curl -i -X POST http://127.0.0.1:8080/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello gemini"}'
```

## Known Limitations

- No real Gemini HTTP call is implemented in Stage 3.
- No real Ollama integration is implemented.
- No RAG endpoint is implemented.
- No streaming endpoint is implemented.
- JSON parsing is still lightweight and only supports simple string fields.
- Provider failure returns a JSON error response; it does not perform automatic
  runtime fallback from Gemini to Mock.

## Suggested Commit Message

```text
Add Stage 3 Gemini provider skeleton and provider factory
```

## Stage 4 Goal

Implement the real Gemini HTTPS call path for `default_provider=gemini` while
preserving the existing Reactor WebServer architecture and all Stage 1-3
endpoints.

## Stage 4 Modified Files

- `CMakeLists.txt`
- `Makefile`
- `Makefile.bak`
- `GatewayConfig.h`
- `GatewayConfig.cpp`
- `GeminiProvider.h`
- `GeminiProvider.cpp`
- `LightAgentGateway.cpp`
- `JsonUtil.h`
- `JsonUtil.cpp`
- `HttpClient.h`
- `HttpClient.cpp`
- `.gitignore`
- `config.example.json`
- `scripts/start_gateway_with_env.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 4 New Classes / Structs / Functions

- `HttpResponse`
- `HttpClient`
  - `HttpClient::postJson(...)`
- JSON helpers:
  - `escapeJsonString`
  - `extractJsonStringField`
  - `extractGeminiText`
  - `buildErrorJson`
  - `buildChatJson`
- `GeminiProvider::chat` now performs the real Gemini HTTP POST when enabled.

## Stage 4 API Changes

- `POST /api/chat` with `default_provider=mock` still returns MockProvider
  output.
- `POST /api/chat` with `default_provider=gemini` and no key returns:
  - `success: false`
  - `provider: "gemini"`
  - `error_message: "GEMINI_API_KEY is not configured"`
- `POST /api/chat` with `default_provider=gemini` and a key calls Gemini
  `generateContent`.
- Missing or empty `message` returns structured JSON with:
  - `success: false`
  - `error_message: "message field is required"`
- `GET /api/health` includes:
  - `gemini_model`
  - `gemini_api_base`
  - `enable_real_gemini`
- `GET /api/metrics` includes:
  - `last_chat_success`
  - `last_chat_error`

## Stage 4 Config Changes

- Default `version` is `0.4.0`.
- Default `stage` is `phase-4`.
- Added:
  - `gemini_model`, default `gemini-1.5-flash`
  - `gemini_api_base`, default `https://generativelanguage.googleapis.com/v1beta`
  - `enable_real_gemini`, default `true`
- Added environment variables:
  - `GEMINI_MODEL`
  - `GEMINI_API_BASE`
  - `LIGHTAGENT_ENABLE_REAL_GEMINI`

## Stage 4 Build Commands

Install dependency:

```bash
sudo apt update
sudo apt install -y libcurl4-openssl-dev
```

Build:

```bash
make clean
make
```

Or:

```bash
cmake -S . -B build
cmake --build build --target WebServer -j 4
```

## Stage 4 Runtime Commands

Mock provider:

```bash
sudo ./WebServer
```

Gemini provider:

```bash
sudo GEMINI_API_KEY="test_or_real_key" \
  LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
  ./WebServer
```

Development port:

```bash
LIGHTAGENT_DEFAULT_PROVIDER="gemini" GEMINI_API_KEY="test_or_real_key" \
  ./WebServer -p 8080 -l /tmp/WebServer.log
```

## Stage 4 Curl Test Commands

```bash
curl http://127.0.0.1/hello
curl http://127.0.0.1/favicon.ico
curl http://127.0.0.1/api/health
curl http://127.0.0.1/api/metrics
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello from mock"}'
```

Gemini:

```bash
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"用一句话介绍一下什么是 RAG"}'
```

Gemini without key:

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="gemini" ./WebServer
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello"}'
```

## Stage 4 Verification Checklist

- `GET /hello` still returns 200.
- `GET /favicon.ico` still returns 200.
- `GET /api/health` returns phase-4 config fields and never returns the API key.
- `GET /api/metrics` updates after chat calls.
- Mock chat returns `provider: "mock"` and `success: true`.
- Gemini without key returns `success: false` and a safe error.
- Gemini with key attempts a real HTTPS call through libcurl.
- Gemini non-2xx or JSON parse failure returns structured JSON and does not
  crash the WebServer.

## Stage 4 Known Limitations

- JSON handling is still lightweight and not a complete JSON parser.
- Gemini response parsing searches for the first string field named `text`.
- No streaming output is implemented.
- No RAG endpoint is implemented.
- No real Ollama call is implemented.
- No retry/backoff/circuit breaker is implemented for Gemini calls.

## Stage 4 Suggested Commit Message

```text
Implement Stage 4 Gemini HTTP provider path
```

## Stage 5 Goal

Add local Ollama model support and use Ollama as an optional fallback when
Gemini fails. Keep the original Reactor WebServer unchanged and preserve all
existing endpoints.

## Stage 5 Modified Files

- `CMakeLists.txt`
- `GatewayConfig.h`
- `GatewayConfig.cpp`
- `LlmProvider.h`
- `JsonUtil.h`
- `JsonUtil.cpp`
- `ProviderFactory.h`
- `ProviderFactory.cpp`
- `LightAgentGateway.cpp`
- `OllamaProvider.h`
- `OllamaProvider.cpp`
- `config.example.json`
- `scripts/start_gateway_with_env.sh.example`
- `scripts/start_gateway_ollama.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 5 New Classes / Structs / Functions

- `OllamaProvider`
  - `OllamaProvider::OllamaProvider(const GatewayConfig&)`
  - `OllamaProvider::name()`
  - `OllamaProvider::chat(const ChatRequest&)`
- `ProviderFactory::create(const GatewayConfig&, const std::string&)`
- `extractOllamaText`
- `ChatResult::fallback_from`
- `ChatResult::fallback_to`

## Stage 5 API Changes

- `default_provider=mock` still returns MockProvider output.
- `default_provider=ollama` calls local Ollama `/api/generate`.
- `default_provider=gemini` still calls Gemini first.
- If Gemini fails and `enable_ollama_fallback=true`, `/api/chat` attempts
  Ollama fallback.
- Fallback responses include:
  - `provider: "ollama"`
  - `fallback_from: "gemini"`
  - `fallback_to: "ollama"`
- Missing or empty `message` still returns structured JSON with
  `error_message: "message field is required"`.

## Stage 5 Config Changes

- Default `version` is `0.5.0`.
- Default `stage` is `phase-5`.
- Added:
  - `ollama_model`, default `qwen2.5:0.5b`
  - `enable_real_ollama`, default `true`
  - `enable_ollama_fallback`, default `true`
- Added environment variables:
  - `OLLAMA_MODEL`
  - `LIGHTAGENT_ENABLE_REAL_OLLAMA`
  - `LIGHTAGENT_ENABLE_OLLAMA_FALLBACK`

## Stage 5 Build Commands

```bash
make clean
make
```

Or:

```bash
cmake -S . -B build
cmake --build build --target WebServer -j 4
```

## Stage 5 Runtime Commands

Mock provider:

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="mock" ./WebServer
```

Ollama provider:

```bash
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
  OLLAMA_MODEL="qwen2.5:0.5b" \
  ./WebServer
```

Gemini with Ollama fallback:

```bash
sudo GEMINI_API_KEY="test_or_real_key" \
  LIGHTAGENT_DEFAULT_PROVIDER="gemini" \
  LIGHTAGENT_ENABLE_OLLAMA_FALLBACK="true" \
  OLLAMA_MODEL="qwen2.5:0.5b" \
  ./WebServer
```

## Stage 5 Curl Test Commands

```bash
curl http://127.0.0.1/api/health
curl http://127.0.0.1/api/metrics
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"hello mock"}'
curl -X POST http://127.0.0.1/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"用一句话介绍一下什么是 RAG"}'
```

## Stage 5 Ollama Setup Commands

```bash
ollama serve
ollama pull qwen2.5:0.5b
```

## Stage 5 Verification Checklist

- `GET /hello` still returns 200.
- `GET /favicon.ico` still returns 200.
- `GET /api/health` returns `phase-5`, `ollama_model`, and
  `enable_ollama_fallback`.
- `GET /api/metrics` returns `fallback_count`, `last_fallback_from`, and
  `last_fallback_to`.
- `default_provider=ollama` returns `provider: "ollama"` on success.
- Gemini failure with Ollama fallback returns final `provider: "ollama"` and
  `fallback_from: "gemini"` when Ollama succeeds.
- If both Gemini and Ollama fail, `/api/chat` returns structured JSON and the
  WebServer does not crash.

## Stage 5 Known Limitations

- No RAG endpoint is implemented.
- No streaming output is implemented.
- Ollama parsing is lightweight and extracts the `response` string field.
- No retry/backoff/circuit breaker is implemented.
- No automatic model pulling is implemented.

## Stage 5 Suggested Commit Message

```text
Add Stage 5 Ollama provider and Gemini fallback
```

## Stage 6 Goal

Add a local file based RAG query skeleton without introducing a vector database.
The flow is: user question -> local `.txt` / `.md` files -> keyword chunk
retrieval -> context prompt -> current LLM provider -> structured RAG JSON.

## Stage 6 Modified Files

- `CMakeLists.txt`
- `GatewayConfig.h`
- `GatewayConfig.cpp`
- `JsonUtil.h`
- `JsonUtil.cpp`
- `LightAgentGateway.h`
- `LightAgentGateway.cpp`
- `KnowledgeBase.h`
- `KnowledgeBase.cpp`
- `RagEngine.h`
- `RagEngine.cpp`
- `config.example.json`
- `knowledge_base/rag_intro.md`
- `knowledge_base/lightagent_gateway.md`
- `scripts/start_gateway_rag_ollama.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 6 New Classes / Structs / Functions

- `DocumentChunk`
- `KnowledgeBase`
  - `KnowledgeBase::load`
  - `KnowledgeBase::search`
  - `KnowledgeBase::lastError`
- `RagResult`
- `RagEngine`
  - `RagEngine::query`
  - `RagEngine::callProvider`
  - `RagEngine::buildPrompt`
- `LightAgentGateway::ragQuery`
- `extractJsonIntField`

## Stage 6 API Changes

- Added `POST /api/rag/query`.
- Request accepts `question` or `message`.
- Optional `top_k` is clamped to `1..10`.
- Response includes `retrieved_chunks`, provider/model, fallback fields,
  latency, and structured errors.
- Existing endpoints remain unchanged:
  - `GET /hello`
  - `GET /favicon.ico`
  - `GET /api/health`
  - `GET /api/metrics`
  - `POST /api/chat`

## Stage 6 RAG Query Flow

1. Parse `question`; fallback to `message`.
2. Load files from `knowledge_base_dir`.
3. Read only `.txt` and `.md` files.
4. Skip files larger than 1 MB.
5. Split documents by `rag_chunk_size`.
6. Score chunks by simple keyword and substring matching.
7. Return top-k chunks.
8. Build a context prompt bounded by `rag_max_context_chars`.
9. Call the configured provider through `ProviderFactory`.
10. If Gemini fails and Ollama fallback is enabled, fallback to Ollama.

## Stage 6 Config Changes

- Default `version` is `0.6.0`.
- Default `stage` is `phase-6`.
- Added:
  - `knowledge_base_dir`, default `./knowledge_base`
  - `rag_top_k`, default `3`
  - `rag_chunk_size`, default `800`
  - `rag_enable_llm_answer`, default `true`
  - `rag_provider`, default empty, meaning use `default_provider`
  - `rag_max_context_chars`, default `3000`
- Added environment variables:
  - `LIGHTAGENT_KB_DIR`
  - `LIGHTAGENT_RAG_TOP_K`
  - `LIGHTAGENT_RAG_CHUNK_SIZE`
  - `LIGHTAGENT_RAG_ENABLE_LLM_ANSWER`
  - `LIGHTAGENT_RAG_PROVIDER`
  - `LIGHTAGENT_RAG_MAX_CONTEXT_CHARS`

## Stage 6 Build Commands

```bash
make clean
make
```

## Stage 6 Runtime Commands

```bash
systemctl status ollama
ollama list
sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
  OLLAMA_MODEL="qwen2.5:0.5b" \
  LIGHTAGENT_KB_DIR="./knowledge_base" \
  ./WebServer
```

## Stage 6 Curl Test Commands

```bash
curl http://127.0.0.1/api/health
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"数据库事务隔离级别有哪些？","top_k":3}'
curl http://127.0.0.1/api/metrics
```

## Stage 6 Verification Checklist

- `GET /api/health` returns `phase-6` and RAG config fields.
- `POST /api/rag/query` returns `object: "rag.query"`.
- Matching questions return non-empty `retrieved_chunks`.
- Each chunk content is capped to 500 characters in the response.
- Missing question returns `question field is required`.
- If LLM generation fails after retrieval, `retrieved_chunks` are still
  returned with `error_message`.
- `/api/metrics` updates `rag_requests_total`, `last_rag_chunks`, and
  `last_rag_provider`.

## Stage 6 Known Limitations

- No embeddings are used.
- No FAISS, Milvus, Chroma, or other vector database is used.
- Retrieval is keyword based and intentionally simple.
- JSON parsing remains lightweight.
- RAG prompt size is bounded but not token-aware.
- No streaming output.

## Stage 6 Suggested Commit Message

```text
Add Stage 6 local file RAG query skeleton
```

## Stage 6 Search Fix Goal

Improve the local file RAG keyword search so Chinese natural language questions
that contain English technical terms can still retrieve relevant chunks. Example
queries:

- `什么是 RAG？`
- `LightAgent Gateway 支持哪些 Provider？`
- `Ollama 是用来做什么的？`

## Stage 6 Search Fix Modified Files

- `KnowledgeBase.h`
- `KnowledgeBase.cpp`
- `MockProvider.cpp`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 6 Search Fix New Functions

- `normalizeForSearch(const std::string& input)`
  - Lowercases ASCII letters.
  - Removes common English and Chinese punctuation.
  - Collapses repeated whitespace.
  - Keeps English letters, numbers, and UTF-8 Chinese text.
- `extractSearchTerms(const std::string& query)`
  - Splits the normalized query by spaces.
  - Extracts continuous ASCII letter/digit runs from the raw query.
  - Keeps important technical terms such as `rag`, `lightagent`, `gateway`,
    `provider`, `gemini`, and `ollama`.
  - Removes meaningless one-letter terms and deduplicates terms.

## Stage 6 Search Fix Scoring Changes

- Normalize query, chunk content, file path, and title before matching.
- Add a high score when normalized chunk content contains the full normalized
  query.
- Add content match score for each extracted search term.
- Add title/path match score for each extracted search term.
- Give important technical terms higher scores.
- Keep only chunks with `score > 0`.
- Use stable score-descending sort, then file path and chunk offset for ties.

## Stage 6 Search Fix Verification Commands

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'

curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"Ollama 是用来做什么的？","top_k":3}'
```

## Stage 6 Search Fix Known Limitations

- This is still keyword search, not semantic search.
- There is still no embedding model, vector database, or reranker.
- Chinese text is not segmented by a dictionary-based tokenizer.

## Stage 6 Search Fix Suggested Commit Message

```text
fix: improve rag keyword search for Chinese queries
```

## Stage 6 RAG Fallback Fix Goal

Repair the RAG path where `default_provider=gemini` correctly triggers
fallback to Ollama after a Gemini timeout, but the final RAG response still
returns `success=false`.

## Stage 6 RAG Fallback Fix Modified Files

- `GatewayConfig.h`
- `GatewayConfig.cpp`
- `GeminiProvider.cpp`
- `OllamaProvider.cpp`
- `LightAgentGateway.cpp`
- `config.example.json`
- `scripts/start_gateway_rag_ollama.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 6 RAG Fallback Fix Root Cause

The fallback test used `LIGHTAGENT_REQUEST_TIMEOUT_MS=5000` to make Gemini fail
quickly in a network-restricted VM. Before this fix, `OllamaProvider` reused the
same generic timeout, so the local RAG prompt sent to Ollama also had only five
seconds to complete. Direct Ollama tests with the default timeout could pass,
while Gemini -> Ollama fallback could still fail.

## Stage 6 RAG Fallback Fix Changes

- Added provider-specific timeout config:
  - `gemini_request_timeout_ms`
  - `ollama_request_timeout_ms`
- Added environment variables:
  - `LIGHTAGENT_GEMINI_REQUEST_TIMEOUT_MS`
  - `LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS`
  - `GEMINI_REQUEST_TIMEOUT_MS`
  - `OLLAMA_REQUEST_TIMEOUT_MS`
- `GeminiProvider` now uses `gemini_request_timeout_ms` when set, otherwise
  falls back to `request_timeout_ms`.
- `OllamaProvider` now uses `ollama_request_timeout_ms`, defaulting to 30000 ms.
- `/api/health` now reports the effective Gemini and Ollama timeout values.
- Ollama failures now include safe diagnostic fields: `base_url`, `model`,
  `timeout_ms`, optional HTTP status, and HttpClient error text. API keys are
  not included.

## Stage 6 RAG Fallback Fix Verification Commands

Run these in VMware Ubuntu after syncing the code:

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

In another terminal:

```bash
curl -X POST http://127.0.0.1/api/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question":"LightAgent Gateway 支持哪些 Provider？","top_k":3}'

curl http://127.0.0.1/api/metrics
```

Expected RAG fields:

- `success=true`
- `provider="ollama"`
- `model="qwen2.5:0.5b"`
- `fallback_from="gemini"`
- `fallback_to="ollama"`
- `retrieved_chunks` is not empty
- `answer` is not empty

Expected metrics fields:

- `fallback_count` increases
- `last_fallback_from="gemini"`
- `last_fallback_to="ollama"`
- `last_rag_provider="ollama"`
- `last_rag_success=true`

## Stage 6 RAG Fallback Fix Suggested Commit Message

```text
fix: repair rag ollama fallback after gemini failure
```

## Stage 7 Goal

Add pseudo streaming APIs without changing the Reactor/Epoll/EventLoop/Server
architecture:

- `POST /api/chat/stream`
- `POST /api/rag/query/stream`

The current stage generates the complete answer first, then formats it as
SSE-like `data: {...}\n\n` chunks. It is not real token streaming from
Gemini/Ollama yet.

## Stage 7 Modified Files

- `CMakeLists.txt`
- `GatewayConfig.h`
- `GatewayConfig.cpp`
- `LightAgentGateway.h`
- `LightAgentGateway.cpp`
- `MockProvider.cpp`
- `StreamUtil.h`
- `StreamUtil.cpp`
- `config.example.json`
- `scripts/start_gateway_stream_ollama.sh.example`
- `docs/development_log.md`
- `docs/debug_log.md`

## Stage 7 New Classes / Structs / Functions

- `StreamBuildResult`
- `splitTextForStream(const std::string&, size_t)`
- `buildSseDataEvent(const std::string&)`
- `buildStreamFromAnswer(...)`
- `buildStreamError(...)`
- `LightAgentGateway::chatStream(...)`
- `LightAgentGateway::ragQueryStream(...)`
- Internal helpers:
  - `executeChatInternal(...)`
  - `executeRagInternal(...)`
  - `setLastStreamStatus(...)`
  - `streamChunkSize(...)`

## Stage 7 API Changes

- Added `POST /api/chat/stream`.
- Added `POST /api/rag/query/stream`.
- Existing APIs remain available:
  - `GET /hello`
  - `GET /favicon.ico`
  - `GET /api/health`
  - `GET /api/metrics`
  - `POST /api/chat`
  - `POST /api/rag/query`

## Stage 7 Stream Response Format

Responses use `Content-Type: text/event-stream; charset=utf-8`.

Each event is formatted as:

```text
data: {"id":"...","object":"...","delta":"...","provider":"...","model":"..."}

```

The final event is always:

```text
data: {"id":"...","object":"...","done":true}

```

Provider failures return an SSE error event followed by `done=true`.

## Stage 7 Chat Stream Flow

1. `POST /api/chat/stream` parses `message`.
2. It reuses `executeChatInternal(...)`, the same provider path used by
   `/api/chat`.
3. Provider selection still goes through `ProviderFactory`.
4. Gemini failure can still fallback to Ollama when enabled.
5. The complete `ChatResult.answer` is split by `StreamUtil`.
6. The response body is returned as SSE-style data chunks.

## Stage 7 RAG Stream Flow

1. `POST /api/rag/query/stream` parses `question`, falling back to `message`.
2. It reuses `executeRagInternal(...)`, the same RAG path used by
   `/api/rag/query`.
3. `KnowledgeBase` retrieves chunks.
4. `RagEngine` builds the RAG prompt and calls the selected provider.
5. Gemini failure can still fallback to Ollama when enabled.
6. The first SSE event includes metadata such as `question`, `top_k`, and
   `retrieved_chunks_count`.
7. The complete RAG answer is split into pseudo stream chunks.

## Stage 7 Metrics Changes

`GET /api/metrics` now includes:

- `stream_requests_total`
- `chat_stream_requests_total`
- `rag_stream_requests_total`
- `last_stream_latency_ms`
- `last_stream_success`
- `last_stream_error`
- `last_stream_provider`
- `last_stream_chunks`
- `last_stream_type`

## Stage 7 Config Changes

`GatewayConfig` now supports:

- `stream_enabled`, default `true`
- `stream_mode`, default `pseudo`
- `stream_chunk_size`, default `40`

Environment variables:

- `LIGHTAGENT_STREAM_ENABLED`
- `LIGHTAGENT_STREAM_MODE`
- `LIGHTAGENT_STREAM_CHUNK_SIZE`

`/api/health` now reports `version=0.7.0`, `stage=phase-7`,
`stream_enabled`, `stream_mode`, and `stream_chunk_size`.

## Stage 7 Build Commands

```bash
make clean
make
```

Or:

```bash
cmake -S . -B build
cmake --build build
```

## Stage 7 Runtime Commands

```bash
systemctl status ollama
ollama list
curl http://127.0.0.1:11434/api/tags

sudo pkill WebServer

sudo LIGHTAGENT_DEFAULT_PROVIDER="ollama" \
OLLAMA_MODEL="qwen2.5:0.5b" \
LIGHTAGENT_KB_DIR="./knowledge_base" \
LIGHTAGENT_STREAM_CHUNK_SIZE="40" \
./WebServer
```

## Stage 7 Curl Test Commands

```bash
curl http://127.0.0.1/api/health

curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{"message":"用一句话介绍一下什么是 RAG"}'

curl -N -X POST http://127.0.0.1/api/rag/query/stream \
  -H "Content-Type: application/json" \
  -d '{"question":"什么是 RAG？","top_k":3}'

curl -N -X POST http://127.0.0.1/api/chat/stream \
  -H "Content-Type: application/json" \
  -d '{}'

curl http://127.0.0.1/api/metrics
```

## Stage 7 Verification Checklist

- `/api/health` returns `phase-7`, `stream_enabled=true`, and
  `stream_mode="pseudo"`.
- `/api/chat/stream` returns multiple `data: {...}` lines and ends with
  `done=true`.
- `/api/rag/query/stream` returns metadata, answer chunks, and `done=true`.
- Missing `message` or `question` returns an SSE error event.
- `/api/metrics` updates stream counters and last stream fields.

## Stage 7 Known Limitations

- This is pseudo streaming, not real token streaming.
- The server still sends one complete HTTP response body.
- No WebSocket support is implemented.
- No real chunked transfer flushing is implemented.
- UTF-8 splitting is boundary-aware for common UTF-8 sequences, but it is not a
  full Unicode grapheme cluster segmenter.

## Stage 7 Suggested Commit Message

```text
feat: add pseudo streaming chat and rag APIs
```

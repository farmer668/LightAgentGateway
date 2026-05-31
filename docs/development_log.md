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

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

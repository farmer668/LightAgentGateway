# LightAgent Gateway

LightAgent Gateway is an AI application gateway evolved from a C++ Reactor
WebServer. It keeps the original static resource service while adding API
routes for health checks, metrics, chat, and local file based RAG queries.

The gateway supports MockProvider, GeminiProvider, and OllamaProvider.

When Gemini is unavailable, LightAgent Gateway can fallback to Ollama so local
AI demos can continue without access to Google APIs.

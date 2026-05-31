#include "RagEngine.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <sstream>

#include "ProviderFactory.h"

namespace {

using Clock = std::chrono::steady_clock;

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::string providerForRag(const GatewayConfig &config) {
  if (!config.rag_provider.empty()) return config.rag_provider;
  return config.default_provider;
}

bool shouldFallbackToOllama(const GatewayConfig &config,
                            const std::string &provider,
                            const ChatResult &result) {
  const std::string normalizedProvider = lowerCopy(provider);
  const std::string normalizedFallback = lowerCopy(config.fallback_provider);
  return !result.success && config.enable_ollama_fallback &&
         normalizedProvider == "gemini" && normalizedFallback == "ollama";
}

}  // namespace

RagEngine::RagEngine(const GatewayConfig &config) : config_(config) {}

RagPreparedRequest RagEngine::prepareRequest(const std::string &question,
                                             int top_k) {
  const auto started = Clock::now();
  RagPreparedRequest prepared;
  prepared.result.question = question;
  prepared.result.top_k = top_k;

  KnowledgeBase knowledgeBase(config_);
  if (!knowledgeBase.load()) {
    prepared.result.error_message = knowledgeBase.lastError();
    prepared.result.latency_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                              started)
            .count();
    return prepared;
  }

  prepared.result.retrieved_chunks = knowledgeBase.search(question, top_k);
  if (prepared.result.retrieved_chunks.empty()) {
    prepared.result.answer = "知识库中没有找到相关内容";
    prepared.result.error_message = "no relevant chunks found";
    prepared.result.latency_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                              started)
            .count();
    return prepared;
  }

  prepared.chat_request = makeChatRequest(question);
  prepared.chat_request.system_prompt =
      buildPrompt(question, prepared.result.retrieved_chunks);
  prepared.ready = true;
  prepared.result.latency_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  return prepared;
}

RagResult RagEngine::query(const std::string &question, int top_k) {
  const auto started = Clock::now();
  RagPreparedRequest prepared = prepareRequest(question, top_k);
  RagResult result = prepared.result;
  if (!prepared.ready) return result;

  if (!config_.rag_enable_llm_answer) {
    result.success = true;
    result.answer = "retrieved_chunks only";
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            Clock::now() - started)
                            .count();
    return result;
  }

  ChatResult chatResult = callProvider(prepared.chat_request);

  result.success = chatResult.success;
  result.answer = chatResult.answer;
  result.provider = chatResult.provider;
  result.model = chatResult.model;
  result.fallback_from = chatResult.fallback_from;
  result.fallback_to = chatResult.fallback_to;
  result.error_message = chatResult.error_message.value_or("");
  result.latency_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  return result;
}

ChatResult RagEngine::callProvider(const ChatRequest &request) const {
  const std::string providerName = providerForRag(config_);
  std::unique_ptr<ILlmProvider> provider =
      ProviderFactory::create(config_, providerName);
  ChatResult result = provider->chat(request);

  if (shouldFallbackToOllama(config_, providerName, result)) {
    std::unique_ptr<ILlmProvider> fallback =
        ProviderFactory::create(config_, "ollama");
    ChatResult fallbackResult = fallback->chat(request);
    fallbackResult.fallback_from = "gemini";
    fallbackResult.fallback_to = "ollama";
    if (!fallbackResult.success) {
      fallbackResult.error_message =
          "Gemini failed: " +
          result.error_message.value_or("primary provider failed") +
          "; Ollama fallback failed: " +
          fallbackResult.error_message.value_or("fallback provider failed");
    }
    return fallbackResult;
  }
  return result;
}

std::string RagEngine::buildPrompt(
    const std::string &question,
    const std::vector<DocumentChunk> &chunks) const {
  std::ostringstream prompt;
  prompt << "你是一个基于本地知识库回答问题的助手。\n";
  prompt << "请只根据下面的 Context 回答用户问题。\n";
  prompt << "如果 Context 中没有答案，请说明“知识库中没有找到足够信息”。\n\n";
  prompt << "Context:\n";

  int index = 1;
  size_t contextChars = 0;
  const size_t maxContext =
      static_cast<size_t>(std::max(200, config_.rag_max_context_chars));
  for (const auto &chunk : chunks) {
    std::ostringstream item;
    item << "[" << index++ << "] file: " << chunk.file_path << "\n";
    item << chunk.content << "\n\n";
    const std::string itemText = item.str();
    if (contextChars + itemText.size() > maxContext) break;
    prompt << itemText;
    contextChars += itemText.size();
  }

  prompt << "Question:\n" << question << "\n\n";
  prompt << "Answer:\n";
  return prompt.str();
}

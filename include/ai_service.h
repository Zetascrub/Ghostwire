#pragma once

#include <Arduino.h>
#include <vector>

enum class AiProvider { OpenAI, Claude };

struct AiTurn {
    String role;
    String text;
};

class AiService {
public:
    bool loadConfig();
    bool isConfigured() const;
    AiProvider provider() const { return provider_; }
    const char* providerName() const;
    const String& model() const { return model_; }
    const String& status() const { return status_; }
    const std::vector<AiTurn>& history() const { return history_; }
    void toggleProvider();
    void clearHistory();
    bool send(const String& prompt, String& answer);
    bool synthesize(const String& text, const char* outputPath);
    bool transcribe(const char* wavPath, String& transcript);

private:
    bool requestJson(const String& url, const String& body,
                     String& response, bool anthropic);
    void trimHistory();
    void logEvent(const char* event, const String& message = "",
                  int code = 0, size_t bytes = 0);

    AiProvider provider_ = AiProvider::OpenAI;
    String openAiKey_;
    String anthropicKey_;
    String openAiModel_ = "gpt-5-mini";
    String anthropicModel_ = "claude-sonnet-5";
    String model_ = openAiModel_;
    String status_ = "Load /ghostwire/secrets/ai.json";
    std::vector<AiTurn> history_;
};

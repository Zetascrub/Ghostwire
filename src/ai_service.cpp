#include "ai_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <time.h>

namespace {
constexpr char kConfigPath[] = "/ghostwire/secrets/ai.json";
constexpr char kLogPath[] = "/ghostwire/logs/ai_chat.jsonl";
constexpr char kOldLogPath[] = "/ghostwire/logs/ai_chat.previous.jsonl";
constexpr size_t kMaxLogBytes = 512 * 1024;
constexpr char kGtsRootR4[] = R"CERT(-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
)CERT";

String jsonError(const String& response, int code) {
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
        JsonVariant message = doc["error"]["message"];
        if (message.is<const char*>()) {
            return String(message.as<const char*>()).substring(0, 160);
        }
        JsonVariant detail = doc["detail"];
        if (detail.is<const char*>()) {
            return String(detail.as<const char*>()).substring(0, 160);
        }
    }
    if (!response.isEmpty()) return response.substring(0, 160);
    return "HTTP " + String(code);
}
}

void AiService::logEvent(const char* event, const String& message, int code,
                         size_t bytes) {
    if (SD.cardType() == CARD_NONE) return;
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/logs");

    File existing = SD.open(kLogPath, FILE_READ);
    const bool rotate = existing && existing.size() >= kMaxLogBytes;
    if (existing) existing.close();
    if (rotate) {
        SD.remove(kOldLogPath);
        SD.rename(kLogPath, kOldLogPath);
    }

    File file = SD.open(kLogPath, FILE_APPEND);
    if (!file) return;
    JsonDocument entry;
    const time_t now = time(nullptr);
    if (now > 1000000000) entry["timestamp"] = static_cast<int64_t>(now);
    entry["uptime_ms"] = millis();
    entry["event"] = event;
    entry["provider"] = providerName();
    entry["model"] = model_;
    if (!message.isEmpty()) entry["message"] = message;
    if (code != 0) entry["code"] = code;
    if (bytes != 0) entry["bytes"] = bytes;
    serializeJson(entry, file);
    file.println();
    file.flush();
    file.close();
}

bool AiService::loadConfig() {
    File file = SD.open(kConfigPath, FILE_READ);
    if (!file) {
        status_ = "Missing /ghostwire/secrets/ai.json";
        logEvent("config_error", status_);
        return false;
    }
    JsonDocument doc;
    const auto error = deserializeJson(doc, file);
    file.close();
    if (error) {
        status_ = "Invalid ai.json";
        logEvent("config_error", String(error.c_str()));
        return false;
    }
    openAiKey_ = String(doc["openai_api_key"] | "");
    anthropicKey_ = String(doc["anthropic_api_key"] | "");
    openAiKey_.trim();
    anthropicKey_.trim();
    openAiModel_ = String(doc["openai_model"] | "");
    anthropicModel_ = String(doc["anthropic_model"] | "");
    openAiModel_.trim();
    anthropicModel_.trim();
    // A copied template may leave optional model fields blank. Treat blank
    // exactly like a missing field instead of sending an empty model value.
    if (openAiModel_.isEmpty()) {
        openAiModel_ = String(doc["model"] | "");
        openAiModel_.trim();
    }
    if (openAiModel_.isEmpty()) openAiModel_ = "gpt-5-mini";
    if (anthropicModel_.isEmpty()) {
        anthropicModel_ = "claude-sonnet-4-20250514";
    }
    const String selected = String(doc["provider"] | "openai");
    provider_ = selected.equalsIgnoreCase("claude") ? AiProvider::Claude
                                                     : AiProvider::OpenAI;
    model_ = provider_ == AiProvider::OpenAI ? openAiModel_
                                              : anthropicModel_;
    status_ = isConfigured() ? "Ready" : "Selected provider key missing";
    logEvent("session_start", status_);
    return isConfigured();
}

bool AiService::isConfigured() const {
    return provider_ == AiProvider::OpenAI ? !openAiKey_.isEmpty()
                                           : !anthropicKey_.isEmpty();
}

const char* AiService::providerName() const {
    return provider_ == AiProvider::OpenAI ? "OpenAI" : "Claude";
}

void AiService::toggleProvider() {
    provider_ = provider_ == AiProvider::OpenAI ? AiProvider::Claude
                                                 : AiProvider::OpenAI;
    model_ = provider_ == AiProvider::OpenAI ? openAiModel_
                                              : anthropicModel_;
    status_ = isConfigured() ? "Ready" : "Provider key missing";
    logEvent("provider_changed", status_);
}

void AiService::clearHistory() {
    history_.clear();
    status_ = "Conversation cleared";
    logEvent("conversation_cleared");
}

void AiService::trimHistory() {
    while (history_.size() > 8) history_.erase(history_.begin());
    size_t characters = 0;
    for (const auto& turn : history_) characters += turn.text.length();
    while (characters > 6000 && history_.size() > 2) {
        characters -= history_.front().text.length();
        history_.erase(history_.begin());
    }
}

bool AiService::requestJson(const String& url, const String& body,
                            String& response, bool anthropic) {
    JsonDocument validation;
    if (deserializeJson(validation, body)) {
        status_ = "Local JSON serialization failed";
        logEvent("request_error", status_);
        return false;
    }
    const char* host = anthropic ? "api.anthropic.com" : "api.openai.com";
    const char* path = anthropic ? "/v1/messages"
                                 : "/v1/chat/completions";
    WiFiClientSecure client;
    client.setCACert(kGtsRootR4);
    // Stream timeouts are milliseconds. The previous 45 value allowed only
    // 45 ms, so a valid but slightly delayed status line was mistaken for
    // response content even when OpenAI returned HTTP 200.
    client.setTimeout(45000);
    if (!client.connect(host, 443)) {
        status_ = "TLS connection failed";
        logEvent("request_error", status_);
        return false;
    }
    client.printf("POST %s HTTP/1.1\r\nHost: %s\r\n", path, host);
    client.print("User-Agent: Ghostwire/0.3\r\n");
    client.print("Content-Type: application/json\r\n");
    if (anthropic) {
        client.print("x-api-key: " + anthropicKey_ + "\r\n");
        client.print("anthropic-version: 2023-06-01\r\n");
    } else {
        client.print("Authorization: Bearer " + openAiKey_ + "\r\n");
    }
    client.printf("Content-Length: %u\r\nConnection: close\r\n\r\n",
                  static_cast<unsigned>(body.length()));
    Serial.printf("[ai] POST %s model=%s bytes=%u\n", url.c_str(),
                  model_.c_str(), static_cast<unsigned>(body.length()));
    logEvent("request_started", url, 0, body.length());
    if (client.write(reinterpret_cast<const uint8_t*>(body.c_str()),
                     body.length()) != body.length()) {
        client.stop();
        status_ = "JSON upload incomplete";
        logEvent("request_error", status_);
        return false;
    }

    const unsigned long responseDeadline = millis() + 45000;
    while (!client.available() && client.connected() &&
           static_cast<long>(responseDeadline - millis()) > 0) {
        delay(1);
    }
    String statusLine;
    while (client.available() && !statusLine.startsWith("HTTP/")) {
        statusLine = client.readStringUntil('\n');
        statusLine.trim();
    }
    if (!statusLine.startsWith("HTTP/")) {
        client.stop();
        status_ = "No HTTP response status";
        logEvent("request_error", status_);
        return false;
    }
    const int firstSpace = statusLine.indexOf(' ');
    const int code = firstSpace >= 0
                         ? statusLine.substring(firstSpace + 1).toInt()
                         : -1;
    bool chunked = false;
    int contentLength = -1;
    for (;;) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) break;
        String lower = line;
        lower.toLowerCase();
        if (lower.startsWith("content-length:")) {
            contentLength = line.substring(line.indexOf(':') + 1).toInt();
        } else if (lower.startsWith("transfer-encoding:") &&
                   lower.indexOf("chunked") >= 0) {
            chunked = true;
        }
    }
    response = "";
    response.reserve(contentLength > 0 && contentLength < 32768
                         ? contentLength
                         : 4096);
    uint8_t buffer[512];
    if (chunked) {
        for (;;) {
            String sizeLine = client.readStringUntil('\n');
            sizeLine.trim();
            size_t remaining = strtoul(sizeLine.c_str(), nullptr, 16);
            if (remaining == 0) break;
            while (remaining > 0) {
                const size_t wanted = std::min(remaining, sizeof(buffer));
                const size_t received = client.readBytes(buffer, wanted);
                if (received == 0) break;
                if (response.length() + received <= 32768) {
                    response.concat(reinterpret_cast<const char*>(buffer),
                                    received);
                }
                remaining -= received;
            }
            client.readStringUntil('\n');
        }
    } else {
        int remaining = contentLength;
        while ((remaining != 0) && (client.connected() || client.available())) {
            const size_t wanted = remaining > 0
                                      ? std::min<size_t>(remaining, sizeof(buffer))
                                      : sizeof(buffer);
            const size_t received = client.readBytes(buffer, wanted);
            if (received == 0) break;
            if (response.length() + received <= 32768) {
                response.concat(reinterpret_cast<const char*>(buffer),
                                received);
            }
            if (remaining > 0) remaining -= received;
        }
    }
    client.stop();
    logEvent("http_response", code >= 200 && code < 300
                                  ? String("success")
                                  : response.substring(0, 512),
             code, response.length());
    if (code < 200 || code >= 300) {
        Serial.printf("[ai] HTTP %d response: %s\n", code,
                      response.substring(0, 512).c_str());
        status_ = jsonError(response, code);
        return false;
    }
    return true;
}

bool AiService::send(const String& prompt, String& answer) {
    if (WiFi.status() != WL_CONNECTED) {
        status_ = "Connect Wi-Fi first";
        logEvent("chat_error", status_);
        return false;
    }
    if (!isConfigured()) {
        status_ = "Selected provider key missing";
        logEvent("chat_error", status_);
        return false;
    }
    logEvent("chat_message", String("user: ") + prompt);
    history_.push_back({"user", prompt});
    trimHistory();
    JsonDocument doc;
    String url;
    if (provider_ == AiProvider::OpenAI) {
        if (model_.isEmpty()) model_ = "gpt-5-mini";
        url = "https://api.openai.com/v1/chat/completions";
        doc["model"] = model_;
        // Reasoning-tier models (this default, gpt-5-mini, among them)
        // spend part of this budget on hidden reasoning tokens before
        // producing any visible output -- a tight budget can leave zero
        // tokens for the actual answer, silently producing an empty
        // "content" field. 700 was too tight; 2000 leaves real headroom.
        doc["max_completion_tokens"] = 2000;
        JsonArray messages = doc["messages"].to<JsonArray>();
        for (const auto& turn : history_) {
            JsonObject item = messages.add<JsonObject>();
            item["role"] = turn.role;
            item["content"] = turn.text;
        }
    } else {
        if (model_.isEmpty()) model_ = "claude-sonnet-4-20250514";
        url = "https://api.anthropic.com/v1/messages";
        doc["model"] = model_;
        doc["max_tokens"] = 2000;
        JsonArray messages = doc["messages"].to<JsonArray>();
        for (const auto& turn : history_) {
            JsonObject item = messages.add<JsonObject>();
            item["role"] = turn.role;
            item["content"] = turn.text;
        }
    }
    String body;
    serializeJson(doc, body);
    String response;
    status_ = "Waiting for " + String(providerName()) + "...";
    if (!requestJson(url, body, response, provider_ == AiProvider::Claude)) {
        history_.pop_back();
        return false;
    }
    doc.clear();
    const DeserializationError parseError = deserializeJson(doc, response);
    if (parseError) {
        // A 2xx HTTP status with a body that still fails to parse had no
        // diagnostics at all before this -- ArduinoJson's own error reason
        // (IncompleteInput/InvalidInput/NoMemory/etc.) plus the actual bytes
        // received are the only way to tell a truncated read apart from
        // truly malformed content apart from a heap allocation failure.
        Serial.printf("[ai] JSON parse failed (%s) on %u bytes: %s\n",
                      parseError.c_str(),
                      static_cast<unsigned>(response.length()),
                      response.substring(0, 400).c_str());
        history_.pop_back();
        status_ = String("Bad JSON: ") + parseError.c_str();
        logEvent("response_parse_error",
                 status_ + String("; preview: ") + response.substring(0, 512),
                 0, response.length());
        return false;
    }
    if (provider_ == AiProvider::OpenAI) {
        // Do not use `| nullptr` here. ArduinoJson infers nullptr_t from that
        // fallback, so even a valid JSON string is converted to null.
        const char* text =
            doc["choices"][0]["message"]["content"].as<const char*>();
        if (text) answer = text;
    } else {
        for (JsonObject content : doc["content"].as<JsonArray>()) {
            const char* text = content["text"].as<const char*>();
            if (text) answer += text;
        }
    }
    if (answer.isEmpty()) {
        // The JSON parsed fine but no text field was found where expected --
        // dump the actual parsed shape (finish/stop reason, and a preview of
        // the raw body) rather than guessing further, since the two most
        // likely real causes (a reasoning-tier model consuming the whole
        // completion-token budget on hidden reasoning with none left for
        // visible output, vs. a genuinely different response shape) look
        // identical from the caller's side without this.
        const char* finishReason =
            provider_ == AiProvider::OpenAI
                ? (doc["choices"][0]["finish_reason"] | "?")
                : (doc["stop_reason"] | "?");
        Serial.printf("[ai] No text extracted (finish/stop reason: %s) from "
                      "%u-byte response: %s\n",
                      finishReason, static_cast<unsigned>(response.length()),
                      response.substring(0, 500).c_str());
        history_.pop_back();
        status_ = String("No text (") + finishReason + ")";
        logEvent("response_error",
                 status_ + String("; preview: ") + response.substring(0, 512),
                 0, response.length());
        return false;
    }
    logEvent("chat_message", String("assistant: ") + answer);
    history_.push_back({"assistant", answer});
    trimHistory();
    status_ = "Ready";
    return true;
}

bool AiService::synthesize(const String& text, const char* outputPath) {
    if (openAiKey_.isEmpty()) {
        status_ = "OpenAI key required for speech";
        logEvent("speech_error", status_);
        return false;
    }
    WiFiClientSecure client;
    client.setCACert(kGtsRootR4);
    HTTPClient http;
    if (!http.begin(client, "https://api.openai.com/v1/audio/speech")) {
        status_ = "TLS setup failed";
        logEvent("speech_error", status_);
        return false;
    }
    http.setTimeout(45000);
    http.addHeader("Authorization", "Bearer " + openAiKey_);
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["model"] = "tts-1";
    doc["voice"] = "coral";
    doc["response_format"] = "mp3";
    doc["input"] = text.substring(0, 4096);
    String body;
    serializeJson(doc, body);
    const int code = http.sendRequest(
        "POST", reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())),
        body.length());
    if (code < 200 || code >= 300) {
        status_ = jsonError(http.getString(), code);
        logEvent("speech_error", status_, code);
        http.end();
        return false;
    }
    SD.remove(outputPath);
    File output = SD.open(outputPath, FILE_WRITE);
    const int written = output ? http.writeToStream(&output) : -1;
    if (output) output.close();
    http.end();
    if (written <= 0) {
        status_ = "Could not save speech";
        logEvent("speech_error", status_);
        return false;
    }
    status_ = "Speech ready";
    logEvent("speech_ready", outputPath, 0, written);
    return true;
}

bool AiService::transcribe(const char* wavPath, String& transcript) {
    if (openAiKey_.isEmpty()) {
        status_ = "OpenAI key required for STT";
        logEvent("transcription_error", status_);
        return false;
    }
    File wav = SD.open(wavPath, FILE_READ);
    if (!wav) {
        status_ = "Recording missing";
        logEvent("transcription_error", status_);
        return false;
    }
    const String boundary = "GhostwireBoundary7MA4YWxk";
    const String prefix = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\ngpt-4o-mini-transcribe\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    const String suffix = "\r\n--" + boundary + "--\r\n";
    const char* bodyPath = "/ghostwire/ai_request.tmp";
    SD.remove(bodyPath);
    File body = SD.open(bodyPath, FILE_WRITE);
    if (!body) {
        wav.close();
        status_ = "Cannot stage upload";
        logEvent("transcription_error", status_);
        return false;
    }
    body.print(prefix);
    uint8_t chunk[1024];
    while (wav.available()) {
        const size_t count = wav.read(chunk, sizeof(chunk));
        body.write(chunk, count);
    }
    wav.close();
    body.print(suffix);
    body.close();
    body = SD.open(bodyPath, FILE_READ);

    WiFiClientSecure client;
    client.setCACert(kGtsRootR4);
    HTTPClient http;
    if (!body || !http.begin(client, "https://api.openai.com/v1/audio/transcriptions")) {
        if (body) body.close();
        SD.remove(bodyPath);
        status_ = "TLS setup failed";
        logEvent("transcription_error", status_);
        return false;
    }
    http.setTimeout(60000);
    http.addHeader("Authorization", "Bearer " + openAiKey_);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    const int code = http.sendRequest("POST", &body, body.size());
    const String response = http.getString();
    body.close();
    SD.remove(bodyPath);
    http.end();
    if (code < 200 || code >= 300) {
        status_ = jsonError(response, code);
        logEvent("transcription_error", status_, code, response.length());
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, response)) {
        status_ = "Invalid transcription response";
        logEvent("transcription_error",
                 status_ + String("; preview: ") + response.substring(0, 512),
                 0, response.length());
        return false;
    }
    transcript = String(doc["text"] | "");
    status_ = transcript.isEmpty() ? "No speech detected" : "Ready";
    logEvent(transcript.isEmpty() ? "transcription_error" : "transcription",
             transcript.isEmpty() ? status_ : transcript);
    return !transcript.isEmpty();
}

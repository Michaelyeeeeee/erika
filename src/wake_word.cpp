#include "wake_word.hpp"
#include "config.hpp"

#include <vosk_api.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace
{

    std::string normalize_text(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        text.erase(
            text.begin(),
            std::find_if(
                text.begin(),
                text.end(),
                [](unsigned char c)
                {
                    return !std::isspace(c);
                }));

        text.erase(
            std::find_if(
                text.rbegin(),
                text.rend(),
                [](unsigned char c)
                {
                    return !std::isspace(c);
                })
                .base(),
            text.end());

        return text;
    }

} // namespace

WakeWordDetector::WakeWordDetector(
    const std::string &model_path,
    const std::string &wake_word,
    float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      wake_word_(normalize_text(wake_word)),
      detection_count_(0)
{
    model_ = vosk_model_new(model_path.c_str());

    if (model_ == nullptr)
    {
        throw std::runtime_error("Failed to load Vosk wake-word model.");
    }

    /*
     * [unk] is very important.
     *
     * Without it, the recognizer is strongly encouraged
     * to interpret unrelated speech/noise as the only
     * phrase in its grammar.
     */
    std::string grammar = "[\"" + wake_word_ + "\", \"[unk]\"]";

    recognizer_ = vosk_recognizer_new_grm(
        model_,
        sample_rate,
        grammar.c_str());

    if (recognizer_ == nullptr)
    {
        vosk_model_free(model_);
        model_ = nullptr;

        throw std::runtime_error("Failed to create wake-word recognizer.");
    }
}

WakeWordDetector::~WakeWordDetector()
{
    if (recognizer_ != nullptr)
    {
        vosk_recognizer_free(recognizer_);
    }

    if (model_ != nullptr)
    {
        vosk_model_free(model_);
    }
}

std::string WakeWordDetector::extract_value(
    const std::string &json,
    const std::string &key)
{
    const std::string marker = "\"" + key + "\"";

    std::size_t key_pos = json.find(marker);

    if (key_pos == std::string::npos)
    {
        return "";
    }

    std::size_t colon = json.find(':', key_pos + marker.length());

    if (colon == std::string::npos)
    {
        return "";
    }

    std::size_t first_quote = json.find('"', colon + 1);

    if (first_quote == std::string::npos)
    {
        return "";
    }

    std::size_t second_quote = json.find('"', first_quote + 1);

    if (second_quote == std::string::npos)
    {
        return "";
    }

    return json.substr(
        first_quote + 1,
        second_quote - first_quote - 1);
}

bool WakeWordDetector::is_exact_wake_word(const std::string &text) const
{
    return normalize_text(text) == wake_word_;
}

bool WakeWordDetector::process(const int16_t *samples, int sample_count)
{
    int complete = vosk_recognizer_accept_waveform(
        recognizer_,
        reinterpret_cast<const char *>(samples),
        sample_count * sizeof(int16_t));

    std::string recognized;

    if (complete)
    {
        const char *result = vosk_recognizer_result(recognizer_);

        if (result != nullptr)
        {
            recognized = extract_value(result, "text");
        }
    }
    else
    {
        const char *result = vosk_recognizer_partial_result(recognizer_);

        if (result != nullptr)
        {
            recognized = extract_value(result, "partial");
        }
    }

    /*
     * Require an exact, stable recognition of the wake word.
     */
    if (is_exact_wake_word(recognized))
    {
        ++detection_count_;

        if (detection_count_ >= Config::WAKE_REQUIRED_DETECTIONS)
        {
            reset();
            return true;
        }
    }
    else
    {
        detection_count_ = 0;
    }

    return false;
}

void WakeWordDetector::reset()
{
    detection_count_ = 0;

    if (recognizer_ != nullptr)
    {
        vosk_recognizer_reset(recognizer_);
    }
}
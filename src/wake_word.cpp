#include "wake_word.hpp"

#include <vosk_api.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace
{
    std::string to_lower(
        std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        return text;
    }
}

WakeWordDetector::WakeWordDetector(
    const std::string &model_path,
    const std::string &wake_word,
    float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      wake_word_(
          to_lower(wake_word))
{
    model_ =
        vosk_model_new(
            model_path.c_str());

    if (model_ == nullptr)
    {
        throw std::runtime_error(
            "Failed to load Vosk wake-word model");
    }

    /*
     * Restrict this recognizer to the wake phrase.
     */
    std::string grammar =
        "[\"" + wake_word + "\", \"[unk]\"]";

    recognizer_ =
        vosk_recognizer_new_grm(
            model_,
            sample_rate,
            grammar.c_str());

    if (recognizer_ == nullptr)
    {
        vosk_model_free(
            model_);

        model_ = nullptr;

        throw std::runtime_error(
            "Failed to create Vosk wake-word recognizer");
    }
}

WakeWordDetector::~WakeWordDetector()
{
    if (recognizer_ != nullptr)
    {
        vosk_recognizer_free(
            recognizer_);
    }

    if (model_ != nullptr)
    {
        vosk_model_free(
            model_);
    }
}

bool WakeWordDetector::process(
    const int16_t *samples,
    int sample_count)
{
    int complete =
        vosk_recognizer_accept_waveform(
            recognizer_,
            reinterpret_cast<const char *>(
                samples),
            sample_count *
                sizeof(int16_t));

    const char *result =
        nullptr;

    /*
     * Check both finished and partial recognition.
     *
     * Partial recognition is what lets:
     *
     *   "hey raspberry open firefox"
     *
     * trigger without requiring a long pause after
     * "raspberry".
     */
    if (complete)
    {
        result =
            vosk_recognizer_result(
                recognizer_);
    }
    else
    {
        result =
            vosk_recognizer_partial_result(
                recognizer_);
    }

    if (result == nullptr)
    {
        return false;
    }

    if (
        result_contains_wake_word(
            result))
    {
        /*
         * Prevent the same phrase from being detected
         * repeatedly from Vosk's existing state.
         */
        reset();

        return true;
    }

    return false;
}

bool WakeWordDetector::result_contains_wake_word(
    const std::string &result)
{
    std::string lower =
        to_lower(result);

    return (
        lower.find(
            wake_word_) !=
        std::string::npos);
}

void WakeWordDetector::reset()
{
    if (recognizer_ != nullptr)
    {
        vosk_recognizer_reset(
            recognizer_);
    }
}
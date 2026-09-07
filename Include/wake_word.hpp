#pragma once

#include <cstdint>
#include <string>

struct VoskModel;
struct VoskRecognizer;

class WakeWordDetector
{
public:
    WakeWordDetector(
        const std::string &model_path,
        const std::string &wake_word,
        float sample_rate = 16000.0f);

    ~WakeWordDetector();

    /*
     * Feed audio into Vosk.
     *
     * Returns true immediately when the wake phrase
     * appears in either a partial or final result.
     */
    bool process(
        const int16_t *samples,
        int sample_count);

    void reset();

private:
    bool result_contains_wake_word(
        const std::string &result);

    VoskModel *model_;
    VoskRecognizer *recognizer_;

    std::string wake_word_;
};
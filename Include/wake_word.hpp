#pragma once

#include "config.hpp"

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
        float sample_rate = Config::SAMPLE_RATE);

    ~WakeWordDetector();

    bool process(const int16_t *samples, int sample_count);

    void reset();

private:
    std::string extract_value(const std::string &json, const std::string &key);

    bool is_exact_wake_word(const std::string &text) const;

    VoskModel *model_;
    VoskRecognizer *recognizer_;

    std::string wake_word_;

    int detection_count_;
};
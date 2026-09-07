#pragma once

#include "config.hpp"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct whisper_context;

class Transcriber
{
public:
    Transcriber(const std::string &model_path, int sample_rate = Config::SAMPLE_RATE);

    ~Transcriber();

    bool process(const int16_t *samples, int sample_count);

    std::string get_result();

    void reset();

private:
    bool chunk_contains_speech(const int16_t *samples, int sample_count) const;

    std::string clean_result(const std::string &text) const;

    void update_pre_roll(const int16_t *samples, int sample_count);

    whisper_context *context_;

    int sample_rate_;

    /*
     * Complete command sent to Whisper.
     */
    std::vector<int16_t> audio_buffer_;

    /*
     * Rolling audio immediately before VAD decides
     * that speech has started.
     */
    std::deque<int16_t> pre_roll_buffer_;

    bool speech_started_;

    int silence_samples_;
};
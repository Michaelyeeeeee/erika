#pragma once

#include <cstdint>
#include <string>

struct VoskModel;
struct VoskRecognizer;

class Transcriber
{
public:
    Transcriber(
        const std::string &model_path,
        float sample_rate = 16000.0f);

    ~Transcriber();

    // Feed microphone samples into the recognizer.
    // Returns true when Vosk thinks the sentence is complete.
    bool process(
        const int16_t *samples,
        int sample_count);

    // Get the recognized sentence.
    std::string get_result();

    // Reset before listening for a new command.
    void reset();

private:
    std::string extract_text(
        const std::string &json);

    VoskModel *model_;
    VoskRecognizer *recognizer_;
};
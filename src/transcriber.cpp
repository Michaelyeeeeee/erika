#include "transcriber.hpp"

#include <vosk_api.h>

#include <iostream>
#include <stdexcept>

Transcriber::Transcriber(
    const std::string &model_path,
    float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr)
{
    model_ =
        vosk_model_new(
            model_path.c_str());

    if (model_ == nullptr)
    {
        throw std::runtime_error(
            "Failed to load Vosk transcription model");
    }

    recognizer_ =
        vosk_recognizer_new(
            model_,
            sample_rate);

    if (recognizer_ == nullptr)
    {
        vosk_model_free(model_);
        model_ = nullptr;

        throw std::runtime_error(
            "Failed to create Vosk transcriber");
    }
}

Transcriber::~Transcriber()
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

bool Transcriber::process(
    const int16_t *samples,
    int sample_count)
{
    int sentence_finished =
        vosk_recognizer_accept_waveform(
            recognizer_,
            reinterpret_cast<const char *>(
                samples),
            sample_count * sizeof(int16_t));

    return sentence_finished != 0;
}

std::string Transcriber::get_result()
{
    const char *result =
        vosk_recognizer_result(
            recognizer_);

    if (result == nullptr)
    {
        return "";
    }

    std::string json_result =
        result;

    std::string text =
        extract_text(
            json_result);

    std::cout
        << "Command: "
        << text
        << '\n';

    return text;
}

void Transcriber::reset()
{
    if (recognizer_ != nullptr)
    {
        vosk_recognizer_reset(
            recognizer_);
    }
}

std::string Transcriber::extract_text(
    const std::string &json)
{
    const std::string key =
        "\"text\"";

    std::size_t key_pos =
        json.find(key);

    if (key_pos == std::string::npos)
    {
        return "";
    }

    std::size_t colon =
        json.find(
            ':',
            key_pos);

    if (colon == std::string::npos)
    {
        return "";
    }

    std::size_t first_quote =
        json.find(
            '"',
            colon);

    if (first_quote == std::string::npos)
    {
        return "";
    }

    std::size_t second_quote =
        json.find(
            '"',
            first_quote + 1);

    if (second_quote == std::string::npos)
    {
        return "";
    }

    return json.substr(
        first_quote + 1,
        second_quote - first_quote - 1);
}
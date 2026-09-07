#include "transcriber.hpp"

#include <whisper.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

Transcriber::Transcriber(const std::string &model_path, int sample_rate)
    : context_(nullptr), sample_rate_(sample_rate), speech_started_(false), silence_samples_(0)
{
    /*
     * Whisper models expect 16 kHz audio.
     */
    if (sample_rate_ != 16000)
    {
        throw std::runtime_error("Whisper transcriber requires 16000 Hz audio.");
    }

    /*
     * Raspberry Pi 5 uses the CPU backend.
     */
    whisper_context_params context_params = whisper_context_default_params();
    context_params.use_gpu = false;

    context_ = whisper_init_from_file_with_params(model_path.c_str(), context_params);

    if (context_ == nullptr)
    {
        throw std::runtime_error("Failed to load Whisper model: " + model_path);
    }
}

Transcriber::~Transcriber()
{
    if (context_ != nullptr)
    {
        whisper_free(context_);
    }
}

bool Transcriber::chunk_contains_speech(const int16_t *samples, int sample_count) const
{
    if (samples == nullptr || sample_count <= 0)
    {
        return false;
    }

    /*
     * Compute RMS amplitude normalized to 0..1.
     */
    double sum_squared = 0.0;

    for (int i = 0; i < sample_count; ++i)
    {
        double value = static_cast<double>(samples[i]) / 32768.0;
        sum_squared += value * value;
    }

    double rms = std::sqrt(sum_squared / sample_count);

    return rms >= Config::COMMAND_SPEECH_RMS_THRESHOLD;
}

bool Transcriber::process(const int16_t *samples, int sample_count)
{
    if (samples == nullptr || sample_count <= 0)
    {
        return false;
    }

    bool speech = chunk_contains_speech(samples, sample_count);

    /*
     * =========================================================
     * WAITING FOR SPEECH
     * =========================================================
     */
    if (!speech_started_)
    {
        update_pre_roll(samples, sample_count);

        if (!speech)
        {
            return false;
        }

        speech_started_ = true;
        silence_samples_ = 0;

        audio_buffer_.assign(pre_roll_buffer_.begin(), pre_roll_buffer_.end());

        /*
         * Current chunk is already contained in pre-roll.
         */
        pre_roll_buffer_.clear();
    }
    else
    {
        /*
         * =====================================================
         * SPEECH ALREADY STARTED
         * =====================================================
         */
        audio_buffer_.insert(audio_buffer_.end(), samples, samples + sample_count);
    }

    /*
     * =========================================================
     * END-OF-SPEECH DETECTION
     * =========================================================
     */
    if (speech)
    {
        silence_samples_ = 0;
    }
    else
    {
        silence_samples_ += sample_count;
    }

    const int required_silence_samples =
        (Config::COMMAND_END_SILENCE_MS * sample_rate_) / 1000;

    if (silence_samples_ >= required_silence_samples)
    {
        return true;
    }

    const int maximum_samples =
        (Config::COMMAND_MAX_DURATION_MS * sample_rate_) / 1000;

    if (static_cast<int>(audio_buffer_.size()) >= maximum_samples)
    {
        return true;
    }

    return false;
}

std::string Transcriber::get_result()
{
    if (context_ == nullptr || audio_buffer_.empty())
    {
        return "";
    }

    /*
     * Convert int16 PCM to the float PCM expected by Whisper.
     */
    std::vector<float> pcm;
    pcm.reserve(audio_buffer_.size());

    for (int16_t sample : audio_buffer_)
    {
        pcm.push_back(static_cast<float>(sample) / 32768.0f);
    }

    whisper_full_params params =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.greedy.best_of = 1;

    /*
     * Tell Whisper that Erika uses English commands,
     * but names, artists, and song titles may contain
     * other languages.
     */
    params.initial_prompt =
        "Voice assistant commands. "
        "Song titles and artist names may be in any language. "
        "Commands include play, open terminal, open firefox, "
        "close firefox, raise the volume, lower the volume, "
        "turn the volume up, turn the volume down, stop, "
        "cancel, and what time is it.";

    unsigned int hardware_threads = std::thread::hardware_concurrency();

    int thread_count =
        hardware_threads > 0
            ? static_cast<int>(std::min(hardware_threads, 4u))
            : 4;

    params.n_threads = thread_count;

    /*
     * Automatically detect the language instead of
     * forcing all speech to English.
     */
    params.language = "auto";

    /*
     * Preserve the spoken language.
     *
     * We do NOT want foreign titles translated into English.
     */
    params.translate = false;

    /*
     * Each voice command is independent.
     */
    params.no_context = true;

    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;

    params.single_segment = true;

    int result = whisper_full(
        context_,
        params,
        pcm.data(),
        static_cast<int>(pcm.size()));

    if (result != 0)
    {
        std::cerr << "Whisper transcription failed.\n";
        return "";
    }

    std::string text;

    const int segment_count = whisper_full_n_segments(context_);

    for (int i = 0; i < segment_count; ++i)
    {
        const char *segment = whisper_full_get_segment_text(context_, i);

        if (segment != nullptr)
        {
            text += segment;
        }
    }

    text = clean_result(text);

    if (!text.empty())
    {
        std::cout << "Command: " << text << '\n';
    }

    return text;
}

std::string Transcriber::clean_result(const std::string &text) const
{
    std::string result = text;

    /*
     * Only lowercase ASCII characters.
     *
     * UTF-8 bytes belonging to Chinese, Japanese, Korean,
     * Arabic, Cyrillic, etc. must be left unchanged.
     */
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            if (c < 128)
            {
                return static_cast<char>(std::tolower(c));
            }

            return static_cast<char>(c);
        });

    /*
     * Trim ASCII whitespace at the beginning.
     */
    while (!result.empty() &&
           std::isspace(static_cast<unsigned char>(result.front())))
    {
        result.erase(result.begin());
    }

    /*
     * Trim ASCII whitespace at the end.
     */
    while (!result.empty() &&
           std::isspace(static_cast<unsigned char>(result.back())))
    {
        result.pop_back();
    }

    return result;
}

void Transcriber::reset()
{
    audio_buffer_.clear();
    pre_roll_buffer_.clear();

    speech_started_ = false;
    silence_samples_ = 0;
}

void Transcriber::update_pre_roll(const int16_t *samples, int sample_count)
{
    const int max_pre_roll_samples =
        (Config::COMMAND_PRE_ROLL_MS * sample_rate_) / 1000;

    for (int i = 0; i < sample_count; ++i)
    {
        pre_roll_buffer_.push_back(samples[i]);
    }

    while (static_cast<int>(pre_roll_buffer_.size()) > max_pre_roll_samples)
    {
        pre_roll_buffer_.pop_front();
    }
}
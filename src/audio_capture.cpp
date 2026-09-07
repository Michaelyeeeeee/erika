#include "audio_capture.hpp"
#include "config.hpp"

#include <iostream>

AudioCapture::AudioCapture(const std::string &device, unsigned int sample_rate, unsigned int channels)
    : pcm_(nullptr), device_(device), sample_rate_(sample_rate), channels_(channels)
{
}

AudioCapture::~AudioCapture()
{
    stop();
}

bool AudioCapture::start()
{
    int err = snd_pcm_open(&pcm_, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0);

    if (err < 0)
    {
        std::cerr << "Failed to open ALSA device: " << snd_strerror(err) << '\n';
        return false;
    }

    /*
     * We ask ALSA for:
     *
     *   S16_LE
     *   16 kHz
     *   mono
     *
     * Because we're using plughw, ALSA will convert
     * the DJI's native:
     *
     *   S24_3LE
     *   48 kHz
     *   stereo
     */
    err = snd_pcm_set_params(
        pcm_,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        channels_,
        sample_rate_,
        1,
        500000);

    if (err < 0)
    {
        std::cerr << "Failed to configure ALSA: " << snd_strerror(err) << '\n';

        stop();
        return false;
    }

    return true;
}

void AudioCapture::stop()
{
    if (pcm_ != nullptr)
    {
        snd_pcm_drop(pcm_);
        snd_pcm_close(pcm_);
        pcm_ = nullptr;
    }
}

int AudioCapture::read(std::vector<int16_t> &buffer)
{
    if (pcm_ == nullptr)
    {
        return -1;
    }

    constexpr int frames = Config::AUDIO_CHUNK_SAMPLES;

    buffer.resize(frames * channels_);

    snd_pcm_sframes_t nframes = snd_pcm_readi(pcm_, buffer.data(), frames);

    if (nframes < 0)
    {
        nframes = snd_pcm_recover(pcm_, static_cast<int>(nframes), 1);

        if (nframes < 0)
        {
            std::cerr << "ALSA read error: " << snd_strerror(nframes) << '\n';
            return -1;
        }
    }

    return static_cast<int>(nframes * channels_);
}
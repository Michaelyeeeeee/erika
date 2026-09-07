#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <alsa/asoundlib.h>

class AudioCapture
{
public:
    AudioCapture(const std::string &device, unsigned int sample_rate, unsigned int channels);

    ~AudioCapture();

    bool start();
    void stop();

    int read(std::vector<int16_t> &buffer);

private:
    snd_pcm_t *pcm_;

    std::string device_;
    unsigned int sample_rate_;
    unsigned int channels_;
};
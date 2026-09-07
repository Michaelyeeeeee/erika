#include "audio_device.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace
{

    std::string to_lower(const std::string &text)
    {
        std::string result = text;

        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c)
                       {
                           return std::tolower(c);
                       });

        return result;
    }

    bool contains(const std::string &text, const std::string &word)
    {
        return to_lower(text).find(to_lower(word)) != std::string::npos;
    }

} // namespace

AudioDevice::AudioDevice()
{
    DeviceResult input = find_input_device();
    DeviceResult output = find_output_device();

    input_device_ = input.device;
    output_device_ = output.device;

    input_name_ = input.name;
    output_name_ = output.name;
}

const std::string &AudioDevice::input() const
{
    return input_device_;
}

const std::string &AudioDevice::output() const
{
    return output_device_;
}

const std::string &AudioDevice::input_name() const
{
    return input_name_;
}

const std::string &AudioDevice::output_name() const
{
    return output_name_;
}

AudioDevice::DeviceResult AudioDevice::find_input_device()
{
    return find_device(true);
}

AudioDevice::DeviceResult AudioDevice::find_output_device()
{
    return find_device(false);
}

AudioDevice::DeviceResult AudioDevice::find_device(bool capture)
{
    DeviceResult best{
        "default",
        "System Default",
        -1};

    int card = -1;

    if (snd_card_next(&card) < 0)
    {
        return best;
    }

    while (card >= 0)
    {
        std::string control_name = "hw:" + std::to_string(card);

        snd_ctl_t *control = nullptr;

        if (snd_ctl_open(&control, control_name.c_str(), 0) >= 0)
        {
            snd_ctl_card_info_t *card_info;
            snd_ctl_card_info_alloca(&card_info);

            if (snd_ctl_card_info(control, card_info) >= 0)
            {
                const char *id_ptr = snd_ctl_card_info_get_id(card_info);
                const char *name_ptr = snd_ctl_card_info_get_name(card_info);
                const char *longname_ptr = snd_ctl_card_info_get_longname(card_info);
                const char *driver_ptr = snd_ctl_card_info_get_driver(card_info);

                std::string card_id = id_ptr ? id_ptr : "";
                std::string card_name = name_ptr ? name_ptr : "";
                std::string long_name = longname_ptr ? longname_ptr : "";
                std::string driver = driver_ptr ? driver_ptr : "";

                int device = -1;

                while (snd_ctl_pcm_next_device(control, &device) >= 0 && device >= 0)
                {
                    snd_pcm_info_t *pcm_info;
                    snd_pcm_info_alloca(&pcm_info);

                    snd_pcm_info_set_device(pcm_info, device);
                    snd_pcm_info_set_subdevice(pcm_info, 0);
                    snd_pcm_info_set_stream(
                        pcm_info,
                        capture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);

                    if (snd_ctl_pcm_info(control, pcm_info) < 0)
                    {
                        continue;
                    }

                    const char *pcm_name_ptr = snd_pcm_info_get_name(pcm_info);
                    std::string pcm_name = pcm_name_ptr ? pcm_name_ptr : "";

                    int score = 10;

                    bool is_usb =
                        contains(driver, "usb") ||
                        contains(long_name, "usb") ||
                        contains(card_name, "usb");

                    bool is_hdmi =
                        contains(card_id, "hdmi") ||
                        contains(card_name, "hdmi") ||
                        contains(long_name, "hdmi");

                    if (is_usb)
                    {
                        score += 100;
                    }

                    if (is_hdmi)
                    {
                        score -= 20;
                    }

                    if (!pcm_name.empty())
                    {
                        score += 1;
                    }

                    if (score > best.score)
                    {
                        best.score = score;

                        best.device =
                            "plughw:CARD=" +
                            card_id +
                            ",DEV=" +
                            std::to_string(device);

                        best.name = card_name;

                        if (!pcm_name.empty())
                        {
                            best.name += " - " + pcm_name;
                        }
                    }
                }
            }

            snd_ctl_close(control);
        }

        if (snd_card_next(&card) < 0)
        {
            break;
        }
    }

    return best;
}
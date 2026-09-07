#pragma once

#include <atomic>
#include <string>

#include <sys/types.h>

class Feedback
{
public:
    /*
     * speaker_device is an ALSA playback device.
     *
     * Examples:
     *
     *   "default"
     *   "plughw:CARD=Device,DEV=0"
     */
    explicit Feedback(const std::string &speaker_device = "default");

    ~Feedback();

    /*
     * Speak arbitrary text.
     */
    void speak(const std::string &text);

    /*
     * Standard successful-command response.
     */
    void success();

    /*
     * Speak the current time.
     */
    void speak_time();

    /*
     * Stop speech immediately.
     */
    void stop();

    bool is_speaking() const;

private:
    std::string speaker_device_;

    std::atomic<bool> speaking_;

    /*
     * espeak-ng process.
     */
    std::atomic<pid_t> speech_pid_;

    /*
     * aplay process.
     */
    std::atomic<pid_t> playback_pid_;
};
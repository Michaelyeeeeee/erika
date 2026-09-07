#include "actions.hpp"
#include "audio_capture.hpp"
#include "audio_device.hpp"
#include "command_handler.hpp"
#include "config.hpp"
#include "transcriber.hpp"
#include "wake_word.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

enum class State
{
    WAITING_FOR_WAKE,
    WAKE_GUARD,
    RECORDING_COMMAND
};

int main(int argc, char *argv[])
{
    /*
     * =========================================================
     * AUDIO DEVICE DISCOVERY
     * =========================================================
     */
    AudioDevice audio_device;
    CommandHandler command_handler;
    Actions actions(audio_device.output());

    /*
     * =========================================================
     * DIRECT COMMAND TEST
     * =========================================================
     */
    if (argc >= 3 && std::string(argv[1]) == "--command")
    {
        std::string text;

        for (int i = 2; i < argc; ++i)
        {
            if (!text.empty())
            {
                text += " ";
            }

            text += argv[i];
        }

        ParsedCommand command = command_handler.parse(text);
        actions.execute(command);

        while (actions.is_running())
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(Config::ACTION_WAIT_POLL_MS));
        }

        return 0;
    }

    /*
     * =========================================================
     * LIST COMMANDS
     * =========================================================
     */
    if (argc == 2 && std::string(argv[1]) == "--list-commands")
    {
        CommandHandler::list_commands();
        return 0;
    }

    /*
     * =========================================================
     * AUDIO
     * =========================================================
     */
    constexpr int WAKE_GUARD_CHUNKS =
        (Config::WAKE_GUARD_MS * Config::SAMPLE_RATE / 1000) /
        Config::AUDIO_CHUNK_SAMPLES;

    /*
     * Vosk is only used for the wake phrase.
     */
    const std::string wake_model_path =
        "vosk/vosk-model-small-en-us-0.15";

    /*
     * Multilingual Whisper model.
     *
     * Unlike base.en, base can recognize names and
     * phrases from many different languages.
     */
    const std::string command_model_path =
        "whisper.cpp/models/ggml-tiny.bin";

    std::cout
        << "\nErika audio configuration:\n"
        << "  Input:  " << audio_device.input_name() << '\n'
        << "          " << audio_device.input() << '\n'
        << "  Output: " << audio_device.output_name() << '\n'
        << "          " << audio_device.output() << "\n\n";

    /*
     * =========================================================
     * MICROPHONE
     * =========================================================
     */
    AudioCapture audio(audio_device.input(), Config::SAMPLE_RATE, 1);

    if (!audio.start())
    {
        std::cerr
            << "Failed to start microphone using:\n"
            << "  " << audio_device.input() << '\n';

        return 1;
    }

    /*
     * =========================================================
     * RECOGNIZERS
     * =========================================================
     */
    std::cout << "Loading Vosk wake-word model...\n";

    WakeWordDetector wake_detector(
        wake_model_path,
        "hey jamal",
        static_cast<float>(Config::SAMPLE_RATE));

    std::cout << "Loading multilingual Whisper base model...\n";

    Transcriber transcriber(command_model_path, Config::SAMPLE_RATE);

    std::cout << "Speech models loaded.\n\n";

    /*
     * =========================================================
     * BUFFERS
     * =========================================================
     */
    std::vector<int16_t> samples;
    std::vector<int16_t> command_audio;

    State state = State::WAITING_FOR_WAKE;
    int wake_guard_chunks = 0;

    std::cout
        << "Microphone opened.\n"
        << "Listening for: hey jamal\n\n";

    /*
     * =========================================================
     * MAIN LOOP
     * =========================================================
     */
    while (true)
    {
        int count = audio.read(samples);

        if (count <= 0)
        {
            continue;
        }

        /*
         * =====================================================
         * WAKE GUARD
         * =====================================================
         */
        if (state == State::WAKE_GUARD)
        {
            if (wake_guard_chunks > 0)
            {
                --wake_guard_chunks;
                continue;
            }

            transcriber.reset();
            state = State::RECORDING_COMMAND;

            /*
             * Do not continue here.
             *
             * This first post-guard chunk can immediately
             * become the beginning of the command.
             */
        }

        /*
         * =====================================================
         * WAKE WORD
         * =====================================================
         */
        bool wake_detected = wake_detector.process(samples.data(), count);

        if (wake_detected)
        {
            std::cout
                << "\n"
                << "*** WAKE WORD DETECTED ***\n";

            if (actions.is_running())
            {
                std::cout << "Interrupting current action...\n";
                actions.stop();
            }

            command_audio.clear();
            transcriber.reset();

            wake_guard_chunks = WAKE_GUARD_CHUNKS;
            state = State::WAKE_GUARD;

            std::cout << "Listening for command...\n\n";

            continue;
        }

        /*
         * =====================================================
         * WAITING
         * =====================================================
         */
        if (state == State::WAITING_FOR_WAKE)
        {
            continue;
        }

        /*
         * =====================================================
         * RECORD COMMAND
         * =====================================================
         */
        if (state == State::RECORDING_COMMAND)
        {
            command_audio.insert(
                command_audio.end(),
                samples.begin(),
                samples.begin() + count);

            bool sentence_finished = transcriber.process(samples.data(), count);

            if (!sentence_finished)
            {
                continue;
            }

            std::cout << "Transcribing with Whisper...\n";

            std::string text = transcriber.get_result();

            if (!text.empty())
            {
                std::cout << "Recognized command: " << text << '\n';

                ParsedCommand command = command_handler.parse(text);
                actions.execute(command);
            }
            else
            {
                std::cout << "No command recognized.\n";
            }

            double seconds =
                static_cast<double>(command_audio.size()) /
                Config::SAMPLE_RATE;

            std::cout
                << "Command length: "
                << seconds
                << " seconds\n\n";

            command_audio.clear();
            transcriber.reset();

            state = State::WAITING_FOR_WAKE;

            std::cout << "Listening for: hey jamal\n\n";
        }
    }

    return 0;
}
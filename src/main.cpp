#include "actions.hpp"
#include "audio_capture.hpp"
#include "command_handler.hpp"
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

int main(
    int argc,
    char *argv[])
{
    /*
     * ---------------------------------------------------------
     * Shared parser/action objects
     * ---------------------------------------------------------
     */
    CommandHandler command_handler;
    Actions actions;

    /*
     * ---------------------------------------------------------
     * Direct command test
     *
     * Example:
     *
     *   make test play bohemian rhapsody
     * ---------------------------------------------------------
     */
    if (
        argc >= 3 &&
        std::string(argv[1]) == "--command")
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

        ParsedCommand command =
            command_handler.parse(text);

        actions.execute(command);

        /*
         * Direct test mode waits for asynchronous
         * action completion.
         */
        while (actions.is_running())
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100));
        }

        return 0;
    }

    /*
     * ---------------------------------------------------------
     * List commands
     * ---------------------------------------------------------
     */
    if (
        argc == 2 &&
        std::string(argv[1]) == "--list-commands")
    {
        CommandHandler::list_commands();

        return 0;
    }

    /*
     * ---------------------------------------------------------
     * Audio configuration
     * ---------------------------------------------------------
     */
    constexpr int SAMPLE_RATE = 16000;

    /*
     * audio_capture.cpp currently reads 400 samples
     * at a time:
     *
     * 400 / 16000 = 25 ms
     */
    constexpr int AUDIO_CHUNK_SAMPLES = 400;

    /*
     * Ignore audio for 200 ms after detecting the
     * wake phrase.
     *
     * This prevents:
     *
     *   raspberry -> berry
     *   raspberry -> barry
     *
     * from becoming part of the command.
     */
    constexpr int WAKE_GUARD_MS = 400;

    constexpr int WAKE_GUARD_CHUNKS =
        (WAKE_GUARD_MS * SAMPLE_RATE / 1000) /
        AUDIO_CHUNK_SAMPLES;

    const std::string model_path =
        "vosk/vosk-model-small-en-us-0.15";

    /*
     * ---------------------------------------------------------
     * DJI microphone
     * ---------------------------------------------------------
     */
    AudioCapture audio(
        "plughw:CARD=Rx,DEV=0",
        SAMPLE_RATE,
        1);

    if (!audio.start())
    {
        std::cerr
            << "Failed to start microphone.\n";

        return 1;
    }

    /*
     * ---------------------------------------------------------
     * Speech recognizers
     * ---------------------------------------------------------
     */
    WakeWordDetector wake_detector(
        model_path,
        "hey raspberry",
        static_cast<float>(SAMPLE_RATE));

    Transcriber transcriber(
        model_path,
        static_cast<float>(SAMPLE_RATE));

    /*
     * ---------------------------------------------------------
     * Audio buffers
     * ---------------------------------------------------------
     */
    std::vector<int16_t> samples;
    std::vector<int16_t> command_audio;

    State state =
        State::WAITING_FOR_WAKE;

    int wake_guard_chunks = 0;

    std::cout
        << "DJI microphone opened.\n"
        << "Listening for: hey raspberry\n\n";

    /*
     * ---------------------------------------------------------
     * Main microphone loop
     * ---------------------------------------------------------
     */
    while (true)
    {
        int count =
            audio.read(samples);

        if (count <= 0)
        {
            continue;
        }

        /*
         * =====================================================
         * WAKE GUARD
         * =====================================================
         *
         * Do NOT run either recognizer during this short
         * window.
         *
         * We're intentionally throwing away the end of the
         * wake phrase.
         */
        if (state == State::WAKE_GUARD)
        {
            if (wake_guard_chunks > 0)
            {
                --wake_guard_chunks;
                continue;
            }

            /*
             * Wake phrase should now be completely finished.
             */
            transcriber.reset();

            state =
                State::RECORDING_COMMAND;

            continue;
        }

        /*
         * =====================================================
         * ALWAYS CHECK FOR A NEW WAKE WORD
         * =====================================================
         *
         * This happens while:
         *
         *   - waiting normally
         *   - recording another command
         *   - another asynchronous action is running
         *
         * This is what allows:
         *
         *   hey raspberry ...
         *
         * to interrupt the previous command.
         */
        bool wake_detected =
            wake_detector.process(
                samples.data(),
                count);

        if (wake_detected)
        {
            std::cout
                << "\n"
                << "*** WAKE WORD DETECTED ***\n";

            /*
             * Interrupt the current action.
             */
            if (actions.is_running())
            {
                std::cout
                    << "Interrupting current action...\n";

                actions.stop();
            }

            /*
             * Discard any command currently being recorded.
             */
            command_audio.clear();

            transcriber.reset();

            /*
             * Ignore the next 200 ms.
             */
            wake_guard_chunks =
                WAKE_GUARD_CHUNKS;

            state =
                State::WAKE_GUARD;

            std::cout
                << "Listening for command...\n\n";

            continue;
        }

        /*
         * =====================================================
         * WAITING FOR WAKE
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
            /*
             * Save raw command audio into RAM.
             */
            command_audio.insert(
                command_audio.end(),
                samples.begin(),
                samples.begin() + count);

            /*
             * Feed command audio into Vosk.
             */
            bool sentence_finished =
                transcriber.process(
                    samples.data(),
                    count);

            if (!sentence_finished)
            {
                continue;
            }

            /*
             * Vosk detected the end of the command.
             */
            std::string text =
                transcriber.get_result();

            if (!text.empty())
            {
                std::cout
                    << "Recognized command: "
                    << text
                    << '\n';

                ParsedCommand command =
                    command_handler.parse(
                        text);

                /*
                 * Action starts asynchronously, so
                 * microphone processing continues.
                 */
                actions.execute(
                    command);
            }
            else
            {
                std::cout
                    << "No command recognized.\n";
            }

            /*
             * Diagnostics.
             */
            double seconds =
                static_cast<double>(
                    command_audio.size()) /
                SAMPLE_RATE;

            std::cout
                << "Command buffer contains "
                << command_audio.size()
                << " samples.\n";

            std::cout
                << "Command length: "
                << seconds
                << " seconds\n\n";

            /*
             * Prepare for next wake phrase.
             */
            command_audio.clear();

            transcriber.reset();

            state =
                State::WAITING_FOR_WAKE;

            std::cout
                << "Listening for: hey raspberry\n\n";
        }
    }

    return 0;
}
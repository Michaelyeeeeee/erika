#include "feedback.hpp"

#include <chrono>
#include <csignal>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

Feedback::Feedback(
    const std::string &speaker_device)
    : speaker_device_(speaker_device),
      speaking_(false),
      speech_pid_(-1),
      playback_pid_(-1)
{
}

Feedback::~Feedback()
{
    stop();
}

void Feedback::speak(
    const std::string &text)
{
    if (text.empty())
    {
        return;
    }

    /*
     * Stop any previous speech.
     */
    stop();

    /*
     * Pipe:
     *
     *   espeak-ng --stdout "text"
     *              |
     *              v
     *   aplay -D <speaker>
     */
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1)
    {
        std::cerr
            << "Feedback: failed to create audio pipe.\n";

        return;
    }

    /*
     * ---------------------------------------------------------
     * Start espeak-ng
     * ---------------------------------------------------------
     */
    pid_t espeak_pid =
        fork();

    if (espeak_pid < 0)
    {
        std::cerr
            << "Feedback: failed to fork espeak-ng.\n";

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        return;
    }

    if (espeak_pid == 0)
    {
        /*
         * Send espeak WAV output into the pipe.
         */
        dup2(
            pipe_fd[1],
            STDOUT_FILENO);

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execlp(
            "espeak-ng",
            "espeak-ng",

            /*
             * Generate WAV audio on stdout.
             */
            "--stdout",

            /*
             * Speech speed.
             */
            "-s",
            "165",

            /*
             * Pitch.
             */
            "-p",
            "45",

            text.c_str(),

            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * ---------------------------------------------------------
     * Start aplay
     * ---------------------------------------------------------
     */
    pid_t aplay_pid =
        fork();

    if (aplay_pid < 0)
    {
        std::cerr
            << "Feedback: failed to fork aplay.\n";

        kill(
            espeak_pid,
            SIGTERM);

        waitpid(
            espeak_pid,
            nullptr,
            0);

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        return;
    }

    if (aplay_pid == 0)
    {
        /*
         * Read WAV data from espeak.
         */
        dup2(
            pipe_fd[0],
            STDIN_FILENO);

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execlp(
            "aplay",
            "aplay",
            "-q",
            "-D",
            speaker_device_.c_str(),
            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * Parent no longer needs the pipe.
     */
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    speech_pid_ =
        espeak_pid;

    playback_pid_ =
        aplay_pid;

    speaking_ =
        true;

    /*
     * Wait for both programs to finish.
     *
     * This blocks the Actions worker thread,
     * but NOT Erika's microphone thread.
     */
    waitpid(
        espeak_pid,
        nullptr,
        0);

    waitpid(
        aplay_pid,
        nullptr,
        0);

    speech_pid_ = -1;
    playback_pid_ = -1;
    speaking_ = false;
}

void Feedback::success()
{
    speak(
        "Success.");
}

void Feedback::speak_time()
{
    auto now =
        std::chrono::system_clock::now();

    std::time_t current_time =
        std::chrono::system_clock::to_time_t(
            now);

    std::tm local_time{};

    localtime_r(
        &current_time,
        &local_time);

    std::ostringstream speech;

    speech
        << "The time is "
        << std::put_time(
               &local_time,
               "%I:%M %p");

    speak(
        speech.str());
}

void Feedback::stop()
{
    pid_t espeak_pid =
        speech_pid_.load();

    pid_t aplay_pid =
        playback_pid_.load();

    /*
     * Stop audio playback first.
     */
    if (aplay_pid > 0)
    {
        kill(
            aplay_pid,
            SIGTERM);
    }

    /*
     * Stop speech generation.
     */
    if (espeak_pid > 0)
    {
        kill(
            espeak_pid,
            SIGTERM);
    }

    /*
     * Do not call waitpid here.
     *
     * speak() owns those child processes and will
     * reap them when they terminate.
     */
    speaking_ = false;
}

bool Feedback::is_speaking() const
{
    return speaking_.load();
}
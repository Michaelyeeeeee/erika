#include "actions.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

Actions::Actions()
    : feedback_("plughw:CARD=Audio,DEV=0"),
      cancel_requested_(false),
      running_(false),
      active_child_pid_(-1)
{
}

Actions::~Actions()
{
    stop();
}

bool Actions::cancelled() const
{
    return cancel_requested_.load();
}

bool Actions::is_running() const
{
    return running_.load();
}

void Actions::set_active_child(
    pid_t pid)
{
    active_child_pid_.store(
        pid);
}

void Actions::clear_active_child(
    pid_t pid)
{
    pid_t expected =
        pid;

    active_child_pid_.compare_exchange_strong(
        expected,
        -1);
}

void Actions::stop()
{
    /*
     * Tell current action to stop.
     */
    cancel_requested_ =
        true;

    /*
     * Immediately stop speech.
     */
    feedback_.stop();

    /*
     * Terminate a currently tracked child process,
     * such as yt-dlp.
     */
    pid_t child =
        active_child_pid_.load();

    if (child > 0)
    {
        kill(
            child,
            SIGTERM);
    }

    /*
     * Wait for the Actions worker thread.
     */
    if (worker_.joinable())
    {
        worker_.join();
    }

    active_child_pid_ =
        -1;

    running_ =
        false;
}

void Actions::execute(
    const ParsedCommand &command)
{
    /*
     * ---------------------------------------------------------
     * STOP
     * ---------------------------------------------------------
     */
    if (
        command.type ==
        CommandType::STOP)
    {
        stop();

        std::cout
            << "Stopped current action.\n";

        /*
         * stop() sets cancel_requested_.
         * Reset it so feedback can play.
         */
        cancel_requested_ =
            false;

        feedback_.speak(
            "Stopped.");

        return;
    }

    /*
     * New command replaces previous action.
     */
    stop();

    cancel_requested_ =
        false;

    running_ =
        true;

    worker_ =
        std::thread(
            [this, command]()
            {
                execute_action(
                    command);

                running_ =
                    false;
            });
}

void Actions::execute_action(
    const ParsedCommand &command)
{
    switch (command.type)
    {
    case CommandType::GET_TIME:
    {
        get_time();
        break;
    }

    case CommandType::OPEN_TERMINAL:
    {
        open_terminal();
        break;
    }

    case CommandType::OPEN_FIREFOX:
    {
        open_firefox();
        break;
    }

    case CommandType::CLOSE_FIREFOX:
    {
        close_firefox();
        break;
    }

    case CommandType::PLAY_MUSIC:
    {
        play_music(
            command.argument);

        break;
    }

    case CommandType::ASK_GEMINI:
    {
        ask_gemini(
            command.argument);

        break;
    }

    case CommandType::STOP:
    case CommandType::UNKNOWN:
    default:
    {
        break;
    }
    }
}

/*
 * =============================================================
 * TIME
 * =============================================================
 */

void Actions::get_time()
{
    if (cancelled())
    {
        return;
    }

    auto now =
        std::chrono::system_clock::now();

    std::time_t current_time =
        std::chrono::system_clock::to_time_t(
            now);

    std::tm local_time{};

    localtime_r(
        &current_time,
        &local_time);

    std::cout
        << "Current time: "
        << std::put_time(
               &local_time,
               "%I:%M %p")
        << '\n';

    /*
     * Special feedback for time:
     *
     * "The time is 7:42 PM."
     */
    feedback_.speak_time();
}

/*
 * =============================================================
 * TERMINAL
 * =============================================================
 */

void Actions::open_terminal()
{
    if (cancelled())
    {
        return;
    }

    std::cout
        << "Opening terminal...\n";

    pid_t pid =
        fork();

    if (pid < 0)
    {
        std::cerr
            << "Failed to fork terminal process.\n";

        return;
    }

    if (pid == 0)
    {
        execlp(
            "x-terminal-emulator",
            "x-terminal-emulator",
            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * Successful non-Gemini command.
     */
    feedback_.success();
}

/*
 * =============================================================
 * OPEN FIREFOX
 * =============================================================
 */

void Actions::open_firefox()
{
    if (cancelled())
    {
        return;
    }

    std::cout
        << "Opening Firefox...\n";

    pid_t pid =
        fork();

    if (pid < 0)
    {
        std::cerr
            << "Failed to fork Firefox process.\n";

        return;
    }

    if (pid == 0)
    {
        execlp(
            "firefox",
            "firefox",
            "--new-window",
            "about:blank",
            static_cast<char *>(nullptr));

        _exit(1);
    }

    feedback_.success();
}

/*
 * =============================================================
 * CLOSE TOPMOST FIREFOX WINDOW
 * =============================================================
 */

void Actions::close_firefox()
{
    if (cancelled())
    {
        return;
    }

    std::cout
        << "Closing topmost Firefox window...\n";

    /*
     * Focus a Firefox window first.
     */
    pid_t focus_pid =
        fork();

    if (focus_pid < 0)
    {
        std::cerr
            << "Failed to fork wlrctl focus process.\n";

        return;
    }

    if (focus_pid == 0)
    {
        execlp(
            "wlrctl",
            "wlrctl",
            "window",
            "focus",
            "app_id:firefox",
            static_cast<char *>(nullptr));

        _exit(1);
    }

    int focus_status = 0;

    waitpid(
        focus_pid,
        &focus_status,
        0);

    if (
        !WIFEXITED(focus_status) ||
        WEXITSTATUS(focus_status) != 0)
    {
        std::cerr
            << "Could not focus Firefox.\n";

        return;
    }

    if (cancelled())
    {
        return;
    }

    /*
     * Give labwc time to update focus.
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            100));

    /*
     * Alt+F4 closes only the focused window.
     */
    pid_t close_pid =
        fork();

    if (close_pid < 0)
    {
        std::cerr
            << "Failed to fork wtype close process.\n";

        return;
    }

    if (close_pid == 0)
    {
        execlp(
            "wtype",
            "wtype",
            "-M",
            "alt",
            "-k",
            "F4",
            "-m",
            "alt",
            static_cast<char *>(nullptr));

        _exit(1);
    }

    int close_status = 0;

    waitpid(
        close_pid,
        &close_status,
        0);

    if (
        WIFEXITED(close_status) &&
        WEXITSTATUS(close_status) == 0)
    {
        feedback_.success();
    }
    else
    {
        std::cerr
            << "Failed to close Firefox window.\n";
    }
}

/*
 * =============================================================
 * YOUTUBE SEARCH
 * =============================================================
 */

std::string Actions::get_first_youtube_result(
    const std::string &query)
{
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1)
    {
        std::cerr
            << "Failed to create yt-dlp pipe.\n";

        return "";
    }

    pid_t pid =
        fork();

    if (pid < 0)
    {
        close(
            pipe_fd[0]);

        close(
            pipe_fd[1]);

        std::cerr
            << "Failed to fork yt-dlp.\n";

        return "";
    }

    if (pid == 0)
    {
        close(
            pipe_fd[0]);

        if (
            dup2(
                pipe_fd[1],
                STDOUT_FILENO) == -1)
        {
            _exit(1);
        }

        close(
            pipe_fd[1]);

        std::string search =
            "ytsearch1:" + query;

        execlp(
            "yt-dlp",
            "yt-dlp",
            "--skip-download",
            "--no-warnings",
            "--print",
            "webpage_url",
            search.c_str(),
            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * Allow "stop" or a new wake command to
     * terminate yt-dlp.
     */
    set_active_child(
        pid);

    close(
        pipe_fd[1]);

    std::string output;

    char buffer[512];

    while (!cancelled())
    {
        ssize_t bytes_read =
            read(
                pipe_fd[0],
                buffer,
                sizeof(buffer));

        if (bytes_read > 0)
        {
            output.append(
                buffer,
                bytes_read);
        }
        else
        {
            break;
        }
    }

    close(
        pipe_fd[0]);

    int status = 0;

    waitpid(
        pid,
        &status,
        0);

    clear_active_child(
        pid);

    if (cancelled())
    {
        return "";
    }

    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0)
    {
        std::cerr
            << "yt-dlp search failed.\n";

        return "";
    }

    /*
     * Remove newline from yt-dlp result.
     */
    while (
        !output.empty() &&
        (output.back() == '\n' ||
         output.back() == '\r'))
    {
        output.pop_back();
    }

    return output;
}

/*
 * =============================================================
 * PLAY MUSIC
 * =============================================================
 */

void Actions::play_music(
    const std::string &song)
{
    if (cancelled())
    {
        return;
    }

    std::cout
        << "Searching YouTube for: "
        << song
        << '\n';

    std::string video_url =
        get_first_youtube_result(
            song);

    if (cancelled())
    {
        std::cout
            << "YouTube search cancelled.\n";

        return;
    }

    if (video_url.empty())
    {
        std::cerr
            << "No YouTube video found.\n";

        return;
    }

    std::cout
        << "Found video: "
        << video_url
        << '\n';

    /*
     * Request YouTube autoplay.
     */
    if (
        video_url.find('?') !=
        std::string::npos)
    {
        video_url +=
            "&autoplay=1";
    }
    else
    {
        video_url +=
            "?autoplay=1";
    }

    if (cancelled())
    {
        return;
    }

    pid_t pid =
        fork();

    if (pid < 0)
    {
        std::cerr
            << "Failed to fork Firefox.\n";

        return;
    }

    if (pid == 0)
    {
        execlp(
            "firefox",
            "firefox",
            "--new-window",
            video_url.c_str(),
            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * Successful music command.
     */
    feedback_.success();
}

/*
 * =============================================================
 * GEMINI
 * =============================================================
 */

void Actions::ask_gemini(
    const std::string &question)
{
    if (
        question.empty() ||
        cancelled())
    {
        return;
    }

    const std::string response_path =
        "/tmp/erika_gemini_response.txt";

    /*
     * Delete previous response.
     */
    std::remove(
        response_path.c_str());

    std::cout
        << "Asking Gemini: "
        << question
        << '\n';

    /*
     * ---------------------------------------------------------
     * Open Ulauncher
     * ---------------------------------------------------------
     */
    pid_t launcher_pid =
        fork();

    if (launcher_pid < 0)
    {
        std::cerr
            << "Failed to launch Ulauncher.\n";

        return;
    }

    if (launcher_pid == 0)
    {
        execlp(
            "ulauncher-toggle",
            "ulauncher-toggle",
            static_cast<char *>(nullptr));

        _exit(1);
    }

    /*
     * Let Ulauncher receive focus.
     */
    for (int i = 0; i < 3; ++i)
    {
        if (cancelled())
        {
            return;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                100));
    }

    /*
     * ---------------------------------------------------------
     * Type Gemini query
     * ---------------------------------------------------------
     */
    std::string query =
        "gm " + question;

    pid_t type_pid =
        fork();

    if (type_pid < 0)
    {
        std::cerr
            << "Failed to start wtype.\n";

        return;
    }

    if (type_pid == 0)
    {
        execlp(
            "wtype",
            "wtype",
            query.c_str(),
            static_cast<char *>(nullptr));

        _exit(1);
    }

    waitpid(
        type_pid,
        nullptr,
        0);

    if (cancelled())
    {
        return;
    }

    /*
     * Do NOT press Enter.
     *
     * Gemini Direct starts automatically.
     */
    std::cout
        << "Waiting for Gemini response...\n";

    std::string response =
        wait_for_gemini_response();

    if (cancelled())
    {
        std::cout
            << "Gemini request cancelled.\n";

        return;
    }

    if (!response.empty())
    {
        std::cout
            << "\nGemini:\n"
            << response
            << "\n\n";

        /*
         * Read Gemini's response aloud.
         *
         * feedback_.speak() blocks this worker thread until
         * the speech has finished.
         */
        feedback_.speak(
            response);

        /*
         * Close Ulauncher after the response has finished
         * being spoken.
         */
        if (!cancelled())
        {
            pid_t close_launcher_pid =
                fork();

            if (close_launcher_pid < 0)
            {
                std::cerr
                    << "Failed to close Ulauncher.\n";
            }
            else if (close_launcher_pid == 0)
            {
                execlp(
                    "ulauncher-toggle",
                    "ulauncher-toggle",
                    static_cast<char *>(nullptr));

                _exit(1);
            }
            else
            {
                waitpid(
                    close_launcher_pid,
                    nullptr,
                    0);
            }
        }
    }
    else
    {
        std::cerr
            << "Timed out waiting for Gemini response.\n";
    }
}

std::string Actions::wait_for_gemini_response()
{
    const std::string path =
        "/tmp/erika_gemini_response.txt";

    constexpr int timeout_ms =
        30000;

    constexpr int poll_ms =
        100;

    int waited_ms =
        0;

    while (
        waited_ms < timeout_ms &&
        !cancelled())
    {
        std::ifstream file(
            path);

        if (file.good())
        {
            std::string response(
                (
                    std::istreambuf_iterator<char>(
                        file)),
                std::istreambuf_iterator<char>());

            if (!response.empty())
            {
                return response;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                poll_ms));

        waited_ms +=
            poll_ms;
    }

    return "";
}
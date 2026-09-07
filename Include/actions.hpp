#pragma once

#include "command_handler.hpp"
#include "feedback.hpp"

#include <atomic>
#include <string>
#include <thread>

#include <sys/types.h>

class Actions
{
public:
    Actions();

    ~Actions();

    /*
     * Start an action asynchronously.
     *
     * Starting another action cancels the
     * previous action.
     */
    void execute(
        const ParsedCommand &command);

    /*
     * Cancel current action and any spoken feedback.
     */
    void stop();

    bool is_running() const;

private:
    void execute_action(
        const ParsedCommand &command);

    bool cancelled() const;

    void get_time();

    void open_terminal();

    void open_firefox();

    void close_firefox();

    void play_music(
        const std::string &song);

    void ask_gemini(
        const std::string &question);

    std::string get_first_youtube_result(
        const std::string &query);

    std::string wait_for_gemini_response();

    void set_active_child(
        pid_t pid);

    void clear_active_child(
        pid_t pid);

    /*
     * All speech/output feedback lives here.
     *
     * Change "default" later to the ALSA name
     * of the USB speaker.
     */
    Feedback feedback_;

    std::thread worker_;

    std::atomic<bool> cancel_requested_;
    std::atomic<bool> running_;

    std::atomic<pid_t> active_child_pid_;
};
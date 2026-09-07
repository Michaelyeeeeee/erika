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
    explicit Actions(const std::string &output_device);
    ~Actions();

    void execute(const ParsedCommand &command);
    void stop();

    bool is_running() const;

private:
    void execute_action(const ParsedCommand &command);

    bool cancelled() const;

    void get_time();

    void open_terminal();

    void open_firefox();
    void close_firefox();

    void volume_up(int percent);
    void volume_down(int percent);
    void change_volume(int percent, bool increase);

    void pause_current_media();
    void play_music(const std::string &song);

    void ask_gemini(const std::string &question);

    std::string get_first_youtube_result(const std::string &query);
    std::string wait_for_gemini_response();

    void set_active_child(pid_t pid);
    void clear_active_child(pid_t pid);

    Feedback feedback_;

    std::thread worker_;

    std::atomic<bool> cancel_requested_;
    std::atomic<bool> running_;
    std::atomic<pid_t> active_child_pid_;
};
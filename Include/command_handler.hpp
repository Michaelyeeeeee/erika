#pragma once

#include <string>

enum class CommandType
{
    UNKNOWN,

    GET_TIME,

    OPEN_TERMINAL,

    OPEN_FIREFOX,
    CLOSE_FIREFOX,

    PLAY_MUSIC,

    VOLUME_UP,
    VOLUME_DOWN,

    ASK_GEMINI,

    STOP
};

struct ParsedCommand
{
    CommandType type;

    /*
     * Optional command-specific data.
     *
     * Examples:
     *
     * PLAY_MUSIC:
     *   "bohemian rhapsody"
     *
     * VOLUME_UP:
     *   "20"
     *
     * VOLUME_DOWN:
     *   "10"
     */
    std::string argument;
};

class CommandHandler
{
public:
    ParsedCommand parse(const std::string &text);

    static void list_commands();

private:
    std::string normalize(const std::string &text);
};
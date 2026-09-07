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
    ASK_GEMINI,
    STOP
};

struct ParsedCommand
{
    CommandType type;
    std::string argument;
};

class CommandHandler
{
public:
    ParsedCommand parse(
        const std::string &text);

    static void list_commands();

private:
    std::string normalize(
        const std::string &text);
};
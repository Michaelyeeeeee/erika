#include "command_handler.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

std::string CommandHandler::normalize(
    const std::string &text)
{
    std::string result =
        text;

    /*
     * Convert to lowercase.
     */
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        });

    /*
     * Trim leading whitespace.
     */
    result.erase(
        result.begin(),
        std::find_if(
            result.begin(),
            result.end(),
            [](unsigned char c)
            {
                return !std::isspace(c);
            }));

    /*
     * Trim trailing whitespace.
     */
    result.erase(
        std::find_if(
            result.rbegin(),
            result.rend(),
            [](unsigned char c)
            {
                return !std::isspace(c);
            })
            .base(),
        result.end());

    /*
     * ---------------------------------------------------------
     * Defensive wake-word cleanup
     * ---------------------------------------------------------
     *
     * Even with the post-wake guard, speech recognition can
     * occasionally place part of "raspberry" at the beginning
     * of the command.
     *
     * Examples:
     *
     *   berry open firefox
     *   barry what time is it
     *
     * Remove those artifacts only when they occur at the
     * beginning.
     */
    const std::vector<std::string> wake_artifacts =
        {
            "hey raspberry ",
            "raspberry ",
            "rasp berry ",
            "berry ",
            "barry "};

    for (const std::string &artifact : wake_artifacts)
    {
        if (
            result.rfind(
                artifact,
                0) == 0)
        {
            result.erase(
                0,
                artifact.length());

            break;
        }
    }

    /*
     * Trim again in case removing the artifact left
     * whitespace.
     */
    result.erase(
        result.begin(),
        std::find_if(
            result.begin(),
            result.end(),
            [](unsigned char c)
            {
                return !std::isspace(c);
            }));

    return result;
}

ParsedCommand CommandHandler::parse(
    const std::string &text)
{
    std::string command =
        normalize(text);

    if (command.empty())
    {
        return {
            CommandType::UNKNOWN,
            ""};
    }

    /*
     * ---------------------------------------------------------
     * Stop
     * ---------------------------------------------------------
     */
    if (
        command == "stop" ||
        command == "cancel" ||
        command == "stop current command")
    {
        return {
            CommandType::STOP,
            ""};
    }

    /*
     * ---------------------------------------------------------
     * Time
     * ---------------------------------------------------------
     */
    if (
        command == "what time is it" ||
        command == "what is the time" ||
        command == "tell me the time")
    {
        return {
            CommandType::GET_TIME,
            ""};
    }

    /*
     * ---------------------------------------------------------
     * Terminal
     * ---------------------------------------------------------
     */
    if (
        command == "open terminal" ||
        command == "open the terminal")
    {
        return {
            CommandType::OPEN_TERMINAL,
            ""};
    }

    /*
     * ---------------------------------------------------------
     * Firefox
     * ---------------------------------------------------------
     */
    if (
        command == "open firefox" ||
        command == "open a firefox window" ||
        command == "open a new firefox window" ||
        command == "open firefox window")
    {
        return {
            CommandType::OPEN_FIREFOX,
            ""};
    }

    if (
        command == "close firefox" ||
        command == "close the firefox window" ||
        command == "close firefox window")
    {
        return {
            CommandType::CLOSE_FIREFOX,
            ""};
    }

    /*
     * ---------------------------------------------------------
     * Music
     * ---------------------------------------------------------
     *
     * Anything after "play " becomes the YouTube search.
     */
    const std::string play_prefix =
        "play ";

    if (
        command.rfind(
            play_prefix,
            0) == 0)
    {
        std::string song =
            command.substr(
                play_prefix.length());

        if (!song.empty())
        {
            return {
                CommandType::PLAY_MUSIC,
                song};
        }
    }

    /*
     * ---------------------------------------------------------
     * Everything else -> Gemini
     * ---------------------------------------------------------
     */
    return {
        CommandType::ASK_GEMINI,
        command};
}

void CommandHandler::list_commands()
{
    std::cout
        << "Valid Erika commands:\n"
        << '\n'

        << "Control:\n"
        << "  stop\n"
        << "  cancel\n"
        << '\n'

        << "Time:\n"
        << "  what time is it\n"
        << "  what is the time\n"
        << "  tell me the time\n"
        << '\n'

        << "Applications:\n"
        << "  open terminal\n"
        << "  open firefox\n"
        << '\n'

        << "Applications:\n"
        << "  open terminal\n"
        << "  open firefox\n"
        << "  close firefox\n"
        << '\n'

        << "Music:\n"
        << "  play <song>\n"
        << '\n'

        << "Gemini:\n"
        << "  Any other question or sentence\n"
        << '\n'

        << "Examples:\n"
        << "  stop\n"
        << "  play bohemian rhapsody\n"
        << "  what is the capital of japan\n"
        << "  explain what a mutex is\n";
}
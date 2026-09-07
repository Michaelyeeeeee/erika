#include "command_handler.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace
{

    bool matches_any(const std::string &command, const std::vector<std::string> &aliases)
    {
        return std::find(aliases.begin(), aliases.end(), command) != aliases.end();
    }

    bool starts_with(const std::string &text, const std::string &prefix)
    {
        return text.rfind(prefix, 0) == 0;
    }

    bool contains_any(const std::string &text, const std::vector<std::string> &phrases)
    {
        for (const std::string &phrase : phrases)
        {
            if (text.find(phrase) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    /*
     * Lowercase only ASCII characters.
     *
     * Non-ASCII UTF-8 bytes are left untouched.
     */
    std::string lower_ascii(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c)
            {
                if (c < 128)
                {
                    return static_cast<char>(std::tolower(c));
                }

                return static_cast<char>(c);
            });

        return text;
    }

    std::string trim_whitespace(std::string text)
    {
        while (!text.empty() &&
               std::isspace(static_cast<unsigned char>(text.front())))
        {
            text.erase(text.begin());
        }

        while (!text.empty() &&
               std::isspace(static_cast<unsigned char>(text.back())))
        {
            text.pop_back();
        }

        return text;
    }

    int volume_amount(const std::string &command)
    {
        const std::vector<std::string> small_phrases = {
            "a little",
            "little bit",
            "a bit",
            "slightly",
            "somewhat",
            "tiny bit",
            "small amount",
            "just a little",
            "just a bit"};

        const std::vector<std::string> large_phrases = {
            "a lot",
            "a bunch",
            "much",
            "significantly",
            "substantially",
            "considerably",
            "way up",
            "way down",
            "much louder",
            "much quieter",
            "much higher",
            "much lower"};

        if (contains_any(command, large_phrases))
        {
            return 40;
        }

        if (contains_any(command, small_phrases))
        {
            return 10;
        }

        return 20;
    }

} // namespace

std::string CommandHandler::normalize(const std::string &text)
{
    std::string result;
    result.reserve(text.size());

    /*
     * Normalization is only used for identifying Erika's
     * English control commands.
     *
     * Non-ASCII characters become spaces here, but the
     * original text is preserved separately for song names.
     */
    for (unsigned char c : text)
    {
        if (c < 128 && std::isalnum(c))
        {
            result += static_cast<char>(std::tolower(c));
        }
        else
        {
            result += ' ';
        }
    }

    /*
     * Collapse repeated spaces.
     */
    std::string collapsed;
    bool previous_space = true;

    for (char c : result)
    {
        bool current_space =
            std::isspace(static_cast<unsigned char>(c));

        if (current_space)
        {
            if (!previous_space)
            {
                collapsed += ' ';
            }
        }
        else
        {
            collapsed += c;
        }

        previous_space = current_space;
    }

    if (!collapsed.empty() && collapsed.back() == ' ')
    {
        collapsed.pop_back();
    }

    result = collapsed;

    /*
     * Remove common remnants of the wake phrase.
     */
    const std::vector<std::string> wake_artifacts = {
        "hey jamal ",
        "jamal ",
        "hey raspberry ",
        "hey rasp berry ",
        "raspberry ",
        "rasp berry ",
        "berry ",
        "barry "};

    for (const std::string &artifact : wake_artifacts)
    {
        if (starts_with(result, artifact))
        {
            result.erase(0, artifact.length());
            break;
        }
    }

    return result;
}

ParsedCommand CommandHandler::parse(const std::string &text)
{
    /*
     * Preserve Whisper's original UTF-8 transcription.
     */
    const std::string original = trim_whitespace(text);

    /*
     * ASCII-lowercase version preserves UTF-8 titles while
     * allowing English command prefixes to be matched.
     */
    const std::string lower_original = lower_ascii(original);

    /*
     * Fully normalized version is used for regular commands.
     */
    const std::string command = normalize(original);

    if (original.empty())
    {
        return {CommandType::UNKNOWN, ""};
    }

    /*
     * =========================================================
     * PLAY MUSIC
     * =========================================================
     *
     * Check this BEFORE general normalization destroys
     * non-ASCII characters.
     */
    const std::vector<std::string> play_prefixes = {
        "please play the song",
        "please play",
        "play the song",
        "play song",
        "play",
        "please put on",
        "put on"};

    for (const std::string &prefix : play_prefixes)
    {
        if (starts_with(lower_original, prefix))
        {
            std::string song = original.substr(prefix.length());
            song = trim_whitespace(song);

            /*
             * Remove common punctuation Whisper may append
             * to the end of the transcription.
             */
            while (!song.empty() &&
                   (song.back() == '.' ||
                    song.back() == ',' ||
                    song.back() == '!' ||
                    song.back() == '?'))
            {
                song.pop_back();
            }

            song = trim_whitespace(song);

            if (!song.empty())
            {
                return {CommandType::PLAY_MUSIC, song};
            }
        }
    }

    /*
     * =========================================================
     * STOP
     * =========================================================
     */
    const std::vector<std::string> stop_aliases = {
        "stop",
        "stopped",
        "stop it",
        "stop now",
        "please stop",
        "cancel",
        "cancel it",
        "cancel that",
        "cancel command",
        "cancel the command",
        "stop command",
        "stop the command",
        "stop current command",
        "stop the current command"};

    if (matches_any(command, stop_aliases))
    {
        return {CommandType::STOP, ""};
    }

    /*
     * =========================================================
     * TIME
     * =========================================================
     */
    const std::vector<std::string> time_aliases = {
        "what time is it",
        "what time is it now",
        "what is the time",
        "whats the time",
        "what s the time",
        "tell me the time",
        "tell me what time it is",
        "give me the time",
        "current time",
        "what time it is",
        "what time is this"};

    if (matches_any(command, time_aliases))
    {
        return {CommandType::GET_TIME, ""};
    }

    /*
     * =========================================================
     * VOLUME DOWN
     * =========================================================
     */
    const std::vector<std::string> volume_down_phrases = {
        "lower the volume",
        "lower volume",
        "decrease the volume",
        "decrease volume",
        "reduce the volume",
        "reduce volume",
        "turn the volume down",
        "turn volume down",
        "turn it down",
        "make it quieter",
        "make the volume quieter",
        "volume down",
        "quieter"};

    if (contains_any(command, volume_down_phrases))
    {
        int amount = volume_amount(command);
        return {CommandType::VOLUME_DOWN, std::to_string(amount)};
    }

    /*
     * =========================================================
     * VOLUME UP
     * =========================================================
     */
    const std::vector<std::string> volume_up_phrases = {
        "raise the volume",
        "raise volume",
        "increase the volume",
        "increase volume",
        "turn the volume up",
        "turn volume up",
        "turn it up",
        "make it louder",
        "make the volume louder",
        "volume up",
        "louder"};

    if (contains_any(command, volume_up_phrases))
    {
        int amount = volume_amount(command);
        return {CommandType::VOLUME_UP, std::to_string(amount)};
    }

    /*
     * =========================================================
     * TERMINAL
     * =========================================================
     */
    const std::vector<std::string> terminal_aliases = {
        "open terminal",
        "open the terminal",
        "open a terminal",
        "open terminal window",
        "open a terminal window",
        "launch terminal",
        "launch the terminal",
        "start terminal",
        "start the terminal",
        "open command line",
        "open the command line",
        "open console",
        "open the console"};

    if (matches_any(command, terminal_aliases))
    {
        return {CommandType::OPEN_TERMINAL, ""};
    }

    /*
     * =========================================================
     * OPEN FIREFOX
     * =========================================================
     */
    const std::vector<std::string> open_firefox_aliases = {
        "open firefox",
        "open fire fox",
        "open firefox browser",
        "open the firefox browser",
        "open the firefox",
        "open a firefox window",
        "open a new firefox window",
        "open firefox window",
        "launch firefox",
        "launch fire fox",
        "start firefox",
        "start fire fox",
        "open firebox",
        "open firefax",
        "open fire fax"};

    if (matches_any(command, open_firefox_aliases))
    {
        return {CommandType::OPEN_FIREFOX, ""};
    }

    /*
     * =========================================================
     * CLOSE FIREFOX
     * =========================================================
     */
    const std::vector<std::string> close_firefox_aliases = {
        "close firefox",
        "close fire fox",
        "close firefox window",
        "close the firefox window",
        "close a firefox window",
        "close the firefox",
        "close firefox browser",
        "close the firefox browser",
        "exit firefox",
        "exit fire fox",
        "close firebox",
        "close firefax",
        "close fire fax"};

    if (matches_any(command, close_firefox_aliases))
    {
        return {CommandType::CLOSE_FIREFOX, ""};
    }

    /*
     * =========================================================
     * GEMINI
     * =========================================================
     *
     * Preserve the original UTF-8 text here too. This means
     * Gemini queries can also contain foreign-language text.
     */
    return {CommandType::ASK_GEMINI, original};
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
        << "  tell me the time\n"
        << '\n'
        << "Volume:\n"
        << "  lower the volume a little   (-10%)\n"
        << "  lower the volume            (-20%)\n"
        << "  lower the volume a lot      (-40%)\n"
        << "  raise the volume a little   (+10%)\n"
        << "  raise the volume            (+20%)\n"
        << "  raise the volume a lot      (+40%)\n"
        << '\n'
        << "Applications:\n"
        << "  open terminal\n"
        << "  open firefox\n"
        << "  close firefox\n"
        << '\n'
        << "Music:\n"
        << "  play <song>\n"
        << "  put on <song>\n"
        << "  Song titles may be in other languages\n"
        << '\n'
        << "Gemini:\n"
        << "  Any other question or sentence\n";
}
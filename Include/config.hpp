#pragma once

namespace Config
{

    /*
     * Amount of audio retained before speech is detected.
     *
     * This prevents the beginning of short words such as
     * "play", "open", and "stop" from being clipped by VAD.
     */
    inline constexpr int COMMAND_PRE_ROLL_MS = 350;

    /*
     * =============================================================
     * AUDIO
     * =============================================================
     */

    inline constexpr int SAMPLE_RATE = 16000;

    /*
     * Number of samples read from ALSA at a time.
     *
     * 400 samples at 16 kHz = 25 ms.
     */
    inline constexpr int AUDIO_CHUNK_SAMPLES = 400;

    /*
     * =============================================================
     * WAKE WORD
     * =============================================================
     */

    /*
     * Amount of audio discarded after wake-word detection
     * to prevent the end of "raspberry" from leaking into
     * the command transcription.
     */
    inline constexpr int WAKE_GUARD_MS = 400;

    /*
     * Number of consecutive Vosk detections required
     * before accepting the wake phrase.
     */
    inline constexpr int WAKE_REQUIRED_DETECTIONS = 3;

    /*
     * =============================================================
     * COMMAND RECORDING / WHISPER
     * =============================================================
     */

    /*
     * Amount of silence required to consider the spoken
     * command finished.
     */
    inline constexpr int COMMAND_END_SILENCE_MS = 700;

    /*
     * Maximum command length before forcing transcription.
     */
    inline constexpr int COMMAND_MAX_DURATION_MS = 15000;

    /*
     * RMS threshold used to decide whether a chunk contains
     * speech.
     */
    inline constexpr float COMMAND_SPEECH_RMS_THRESHOLD = 0.010f;

    /*
     * =============================================================
     * GEMINI / ULAUNCHER
     * =============================================================
     */

    /*
     * Maximum time to wait for Gemini to respond.
     */
    inline constexpr int GEMINI_TIMEOUT_MS = 30000;

    /*
     * How often Erika checks the Gemini response file.
     */
    inline constexpr int GEMINI_POLL_MS = 100;

    /*
     * Delay after opening Ulauncher so it has time to
     * receive keyboard focus.
     */
    inline constexpr int ULAUNCHER_FOCUS_DELAY_MS = 300;

    /*
     * =============================================================
     * WINDOW MANAGEMENT
     * =============================================================
     */

    /*
     * Delay after focusing Firefox before sending Alt+F4.
     */
    inline constexpr int FIREFOX_FOCUS_DELAY_MS = 100;

    /*
     * =============================================================
     * DIRECT COMMAND MODE
     * =============================================================
     */

    /*
     * Poll interval while waiting for an asynchronous
     * action to finish during:
     *
     * make test ...
     */
    inline constexpr int ACTION_WAIT_POLL_MS = 100;

}
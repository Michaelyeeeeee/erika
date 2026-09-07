# hey-jamal voice recognition

Wake word ("hey jamal") + command transcription for a Raspberry Pi 5, using
Porcupine for the always-on keyword spotter and whisper.cpp for transcribing
whatever you say after it.

## One-time setup

1. **Picovoice account** (free): sign up at https://console.picovoice.ai
   - Copy your **AccessKey** from the console dashboard.
   - Go to the Porcupine tab, type "hey jamal" as a custom wake word, select
     **Raspberry Pi** as the target platform, and train it. Download the
     resulting `hey-jamal_en_raspberry-pi_v3_0_0.ppn` file into `models/`.

2. **Porcupine SDK**: download the C SDK from the Picovoice console or GitHub
   (`Porcupine/lib/raspberry-pi/...`, `Porcupine/lib/common/porcupine_params.pv`,
   `Porcupine/include/pv_porcupine.h`) and place it under `third_party/porcupine/`.
   Copy `porcupine_params.pv` into `models/`.

3. **whisper.cpp**:
   ```
   git clone https://github.com/ggml-org/whisper.cpp third_party/whisper.cpp
   cd third_party/whisper.cpp
   bash models/download-ggml-model.sh base.en-q5_1
   cp models/ggml-base.en-q5_1.bin ../../models/
   cd ../..
   ```
   (Use `tiny.en-q5_1` instead if you want faster, less accurate transcription.)

4. **ALSA dev headers**:
   ```
   sudo apt install libasound2-dev
   ```

5. **Find your DJI Mic RX's ALSA device name**:
   ```
   arecord -l
   ```
   Look for something like `card 1: DJIMICRX`. Update the device string in
   `src/main.cpp` (`"plughw:CARD=DJIMICRX,DEV=0"`) to match exactly what you see.

## Build

```
mkdir build && cd build
cmake ..
make -j4
```

## Run

```
export PICOVOICE_ACCESS_KEY="your-access-key-here"
./voice_recognition
```

Say "hey jamal", wait for the log line confirming it heard the wake word,
then say your command. It'll print the transcribed text -- wire that string
into an intent dispatcher to actually act on it (play videos, check weather,
etc).

## Tuning notes

- `WakeWord` sensitivity defaults to 0.5 (0.0-1.0). Raise it if "hey jamal"
  isn't triggering reliably; lower it if it's firing on unrelated speech/noise.
- `record_until_silence()` in `endpointing.h` defaults to 700ms of silence to
  end the utterance and an 8s hard cap. Adjust `silence_threshold` if your
  room is noisy -- it's a raw RMS amplitude on 16-bit samples, so higher values
  need louder "quiet" to trigger.
- `base.en` gives noticeably better accuracy than `tiny.en` for command
  parsing; only drop to `tiny.en` if base is too slow on your Pi 5 (it
  shouldn't be, with 4 threads).

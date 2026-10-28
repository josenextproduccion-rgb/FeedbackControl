# Feedback Control — Starter v0.2

Cross-platform JUCE starter for an automatic live-sound feedback-control prototype.

## Included in this milestone
- JUCE C++ project targeting VST3 and Standalone on Windows/macOS, plus AU on macOS.
- Mono and stereo main bus layouts.
- Four manual notch-like filters.
- Automatic spectral candidate detector based on a 2048-point Hann-windowed FFT.
- Candidate persistence check over multiple analysis hops.
- Four automatic notch filters, inserted at detected candidate frequencies.
- Sensitivity control, automatic detection toggle, bypass, status readout, and parameter state recall.
- Fixed-size analysis buffers; no dynamic allocation in the FFT detector itself.

## Important: experimental prototype only
This is an early research milestone, **not a release-ready feedback suppressor**. The detector uses a simple peak-prominence and persistence heuristic and may react to sustained musical notes, resonances, or other tonal content. It has not been validated in a live PA feedback loop. Do not rely on it for show-critical protection or as a substitute for proper gain structure, microphone placement, loudspeaker aiming, and an engineer monitoring the system.

The current automatic filter insertion updates biquad coefficients when a candidate is accepted. Before release, coefficient transitions should be smoothed/crossfaded, and all manual parameter-update paths should be audited for strict real-time safety. The manual filter coefficient helper still uses JUCE coefficient objects and needs a production-grade update strategy.

## Build prerequisites
- CMake 3.22+
- Git
- C++17 toolchain
- macOS: Xcode / Xcode command-line tools
- Windows: Visual Studio 2022 with Desktop development with C++

CMake downloads JUCE 8.0.8 from the official JUCE GitHub repository during configuration, so first configuration requires internet access.

## Build
From this folder:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The exact output paths depend on the operating system and generator. On macOS, the Standalone and AU targets can be built in addition to VST3. On Windows, Standalone and VST3 are the intended formats.

## Architecture
1. Audio callback writes mono-summed input into a fixed-size ring buffer.
2. Every 512 samples, a 2048-sample Hann-windowed FFT is run once the ring is full.
3. A local spectral maximum is evaluated for amplitude, neighbour prominence, and persistence.
4. A persistent candidate creates an automatic notch, unless a nearby notch is already active.
5. Manual filters are processed along with the automatic notches.

## Next engineering milestones
1. Compile and validate on macOS and Windows.
2. Add coefficient smoothing/crossfades and a safe parameter update path.
3. Improve detection: spectral history, rise-rate analysis, harmonic/context rejection, hold/release logic, and user-adjustable thresholds.
4. Add visual spectrum and notch markers.
5. Add automated DSP tests using sine tones, music, speech, and synthetic feedback-loop captures.
6. Run CPU, latency, mono/stereo, host compatibility, and live PA tests.

## Real-time rules
Avoid heap allocation, blocking locks, file/network I/O, or UI calls from `processBlock`. The detector buffers are fixed-size. This project still requires a complete real-time audit before professional or live use.

## Automated VST3 builds (Windows + macOS)

The repository includes `.github/workflows/build-vst3.yml`. After uploading this project to a GitHub repository:

1. Open the repository's **Actions** tab.
2. Select **Build Feedback Control VST3**.
3. Choose **Run workflow** (or push a commit to `main`/`master`).
4. Wait for both the Windows and macOS jobs to finish successfully.
5. Download the two artifacts named `FeedbackControl-VST3-Windows x64` and `FeedbackControl-VST3-macOS Universal` from the completed workflow run.

The Windows artifact contains a VST3 bundle for Windows x64. The macOS artifact requests a universal `arm64;x86_64` build. These artifacts are produced only when the build and packaging steps succeed. They are not digitally signed or notarized; macOS may require local approval, and commercial distribution would require signing/notarization.

This execution environment cannot reach GitHub to fetch JUCE or launch GitHub Actions on your behalf. Therefore, adding this workflow prepares the automated build but does **not** mean any VST3 binary has already been compiled or validated. The automatic detector remains experimental and needs audio/host validation before live use.

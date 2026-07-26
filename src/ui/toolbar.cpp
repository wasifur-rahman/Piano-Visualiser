#include "toolbar.h"
#include "panel.h"
#include "../midi.h"
#include "../recording.h"
#include "../playback.h"
#include "../visuals/blocks.h"
#include "../window_state.h"
#include <raylib.h>
#include <raygui.h>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <array>
#include <filesystem>
#include <cctype>

namespace {
    bool toolbarVisible = false;
    float toolbarSlide = 0.0f; // 0.0 = hidden, 1.0 = fully visible
    constexpr float toolbarHeight = 60.0f;
    constexpr float toolbarAnimationSpeed = 5.0f;

    // Recomputed each update() call, read back in draw()
    float toolbarY = -toolbarHeight;
    Rectangle minusButton{};
    Rectangle plusButton{};
    Rectangle settingsButton{};
    Rectangle recordButton{};
    Rectangle playbackButton{};
    Rectangle playPauseButton{};
    Rectangle loadButton{};        // playback-mode-only: anchored where transpose cluster is
    Rectangle skipBackButton{};    // playback-mode-only
    Rectangle skipForwardButton{}; // playback-mode-only
    Rectangle speedButton{};       // playback-mode-only
    Rectangle progressBarVisual{}; // playback-mode-only: the drawn scrub bar
    Rectangle progressBarHit{};    // playback-mode-only: taller invisible grab area over the bar

    constexpr float recordButtonSize = 30.0f;
    constexpr float recordButtonGap = 20.0f; // gap between adjacent buttons in this right-side cluster
    constexpr float playPauseButtonSize = 40.0f;
    constexpr float smallButtonSize = 30.0f;
    constexpr float smallButtonGap = 15.0f;
    constexpr float speedButtonWidth = 55.0f;
    constexpr float skipSeconds = 5.0f;
    constexpr float progressBarHeight = 5.0f;
    constexpr float progressBarHitPadding = 6.0f;
    constexpr float progressBarSideMargin = 20.0f;

    // True while the secondary "playback mode" toolbar is showing instead
    // of the normal one (record/transpose hidden, playback controls shown).
    bool playbackModeActive = false;

    // Set while the scrub bar is being dragged; tracked across frames since
    // the mouse can move outside progressBarHit mid-drag and we still want
    // to keep scrubbing until the button is released.
    bool draggingProgress = false;

    // 0.5x / 0.75x / 1x / 1.5x / 2x, cycled by clicking the speed button.
    constexpr std::array<float, 5> speedSteps = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
    int speedStepIndex = 2; // starts at 1.0x

    void cycleSpeed() {
        speedStepIndex = (speedStepIndex + 1) % (int)speedSteps.size();
        playback::setSpeed(speedSteps[speedStepIndex]);
    }

    std::string formatSpeedLabel(float speed) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2gx", speed);
        return buf;
    }

    std::string formatTime(double seconds) {
        if (seconds < 0.0) seconds = 0.0;
        int totalSeconds = (int)seconds;
        int minutes = totalSeconds / 60;
        int secs = totalSeconds % 60;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d:%02d", minutes, secs);
        return buf;
    }

    void drawPlayTriangle(Vector2 center, float size, Color color) {
        Vector2 p1 = { center.x - size / 2.0f, center.y - size / 2.0f };
        Vector2 p2 = { center.x - size / 2.0f, center.y + size / 2.0f };
        Vector2 p3 = { center.x + size / 2.0f, center.y };
        DrawTriangle(p1, p2, p3, color);
    }

    void drawPauseBars(Vector2 center, float size, Color color) {
        float barWidth = size * 0.3f;
        float gap = size * 0.3f;
        DrawRectangle((int)(center.x - gap / 2.0f - barWidth), (int)(center.y - size / 2.0f), (int)barWidth, (int)size, color);
        DrawRectangle((int)(center.x + gap / 2.0f), (int)(center.y - size / 2.0f), (int)barWidth, (int)size, color);
    }

    // Two small triangles pointing the given direction - a standard
    // "skip to next/previous" glyph.
    //
    // Note: raylib's DrawTriangle only renders vertices given in
    // counter-clockwise order. Mirroring the shape via `dir` flips its
    // winding as a side effect, so p1/p2 need to swap for the backward
    // case to keep it counter-clockwise - otherwise it's silently culled.
    void drawSkipIcon(Vector2 center, float size, Color color, bool forward) {
        float dir = forward ? 1.0f : -1.0f;
        float triSize = size * 0.6f;
        float spacing = size * 0.4f;

        for (int i = 0; i < 2; ++i) {
            float offset = ((i == 0) ? -spacing : 0.0f) * dir;
            Vector2 c = { center.x + offset, center.y };
            Vector2 p1 = { c.x - (triSize / 2.0f) * dir, c.y - triSize / 2.0f };
            Vector2 p2 = { c.x - (triSize / 2.0f) * dir, c.y + triSize / 2.0f };
            Vector2 p3 = { c.x + (triSize / 2.0f) * dir, c.y };

            if (forward) {
                DrawTriangle(p1, p2, p3, color);
            } else {
                DrawTriangle(p2, p1, p3, color); // swapped to preserve CCW winding
            }
        }
    }

    // Transpose state (moved in from settings.cpp)
    int transposeSemitones = 0;
    constexpr int TRANSPOSE_MIN = -6;
    constexpr int TRANSPOSE_MAX = 6;

    // Generalized version of the pattern already used for background image
    // selection (see panel.cpp's pickImagePathFromExplorer) — same
    // temp-file trick, just parameterized on dialog title/filter so it can
    // be reused for .pvrc files too.
    //
    // NOTE: shares the hardcoded build-dir temp path with the image picker
    // in panel.cpp. Worth factoring both into a shared util.h/.cpp at some
    // point so there's only one copy of this logic.
    std::string pickFilePathFromExplorer(const std::string& title,
                                          const std::string& filterName,
                                          const std::string& filterPattern) {
        const std::string tempPath = "C:/Users/wasif/Desktop/PianoVisualiser/build/selected_file_path.txt";
        const std::string command =
            "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
            "$dialog = New-Object System.Windows.Forms.OpenFileDialog; "
            "$dialog.Title = '" + title + "'; "
            "$dialog.Filter = '" + filterName + " (" + filterPattern + ")|" + filterPattern +
            "|All Files (*.*)|*.*'; "
            "if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $dialog.FileName }\" > \"" +
            tempPath + "\"";
        std::system(command.c_str());

        std::ifstream input(tempPath);
        std::string selectedPath;
        if (input.is_open()) {
            std::getline(input, selectedPath);
        }
        std::remove(tempPath.c_str());
        return selectedPath;
    }

    // Wraps a file-picker call so the app drops out of borderless fullscreen
    // first and restores it afterwards - otherwise the OS dialog can end up
    // stuck behind the undecorated fullscreen window.
    //
    // Accepts both our own .pvrc recordings and standard .mid/.midi files -
    // loadRecordingOrMidi() below picks the right loader based on extension.
    std::string pickRecordingPathFromExplorer() {
        bool wasFullscreen = window_state::isWindowedFullscreen();
        if (wasFullscreen) window_state::setWindowMode(false);

        std::string path = pickFilePathFromExplorer(
            "Select a recording or MIDI file",
            "Recording or MIDI file",
            "*.pvrc;*.mid;*.midi");

        if (wasFullscreen) window_state::setWindowMode(true);
        return path;
    }

    // Dispatches to playback::loadFromFile (.pvrc) or playback::loadFromMidFile
    // (.mid/.midi) based on the picked file's extension.
    void loadRecordingOrMidi(const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);

        if (ext == ".mid" || ext == ".midi") {
            playback::loadFromMidFile(path);
        } else {
            playback::loadFromFile(path); // .pvrc, or anything else - let it fail with its own error
        }
    }
}

namespace toolbar {

    void transposeUp() {
        if (transposeSemitones < TRANSPOSE_MAX) transposeSemitones++;
    }

    void transposeDown() {
        if (transposeSemitones > TRANSPOSE_MIN) transposeSemitones--;
    }

    int getTranspose() {
        return transposeSemitones;
    }

    void update(float dt) {
        if (IsKeyPressed(KEY_TAB)) {
            toolbarVisible = !toolbarVisible;
        }

        float targetToolbarSlide = toolbarVisible ? 1.0f : 0.0f;
        toolbarSlide += (targetToolbarSlide - toolbarSlide) * dt * toolbarAnimationSpeed;
        toolbarY = (toolbarSlide * toolbarHeight) - toolbarHeight;

        int screenWidth = GetScreenWidth();
        float clusterWidth = 100.0f; // width of the transpose button cluster
        minusButton = { (float)screenWidth - clusterWidth - 40, toolbarY + 15, 30, 30 };
        plusButton = { (float)screenWidth - 40, toolbarY + 15, 30, 30 };
        settingsButton = { 20, toolbarY + 15, 30, 30 };

        // Right-side cluster, right to left: transpose, playback, record -
        // i.e. left to right it reads record, playback, transpose.
        playbackButton = { minusButton.x - recordButtonGap - recordButtonSize, toolbarY + 15, recordButtonSize, recordButtonSize };
        recordButton = { playbackButton.x - recordButtonGap - recordButtonSize, toolbarY + 15, recordButtonSize, recordButtonSize };

        playPauseButton = {
            (float)screenWidth / 2.0f - playPauseButtonSize / 2.0f,
            toolbarY + (toolbarHeight - playPauseButtonSize) / 2.0f,
            playPauseButtonSize,
            playPauseButtonSize
        };

        // Spans the same footprint as the minus/plus cluster, so it slots
        // into the same visual "slot" when playback mode swaps transpose out.
        loadButton = { minusButton.x, toolbarY + 15,
                       (plusButton.x + plusButton.width) - minusButton.x, 30 };

        // Skip/speed cluster flanks the play/pause button, vertically
        // aligned with the other 30px-tall buttons.
        float smallButtonY = toolbarY + 15;
        skipBackButton = {
            playPauseButton.x - smallButtonGap - smallButtonSize,
            smallButtonY, smallButtonSize, smallButtonSize
        };
        skipForwardButton = {
            playPauseButton.x + playPauseButtonSize + smallButtonGap,
            smallButtonY, smallButtonSize, smallButtonSize
        };
        speedButton = {
            skipForwardButton.x + smallButtonSize + smallButtonGap,
            smallButtonY, speedButtonWidth, smallButtonSize
        };

        // Scrub bar spans the gap between the connection-status text and
        // the right-side cluster, sitting in a thin strip along the bottom
        // of the toolbar.
        float progressLeft = settingsButton.x + settingsButton.width + 130.0f;
        float progressRight = playbackButton.x - progressBarSideMargin;
        float progressWidth = std::max(0.0f, progressRight - progressLeft);
        float progressY = toolbarY + toolbarHeight - progressBarHeight - 3.0f;

        progressBarVisual = { progressLeft, progressY, progressWidth, progressBarHeight };
        progressBarHit = {
            progressLeft, progressY - progressBarHitPadding,
            progressWidth, progressBarHeight + progressBarHitPadding * 2.0f
        };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();

            if (CheckCollisionPointRec(mousePos, settingsButton)) {
                panel::toggle();
            }
            if (CheckCollisionPointRec(mousePos, playbackButton)) {
                playbackModeActive = !playbackModeActive;
                blocks::setReverseMode(playbackModeActive);
            }

            if (playbackModeActive) {
                // Secondary toolbar: only settings, connection status
                // (handled in draw(), no click target), and playback
                // controls are live.
                if (CheckCollisionPointRec(mousePos, playPauseButton)) {
                    playback::togglePlayback();
                }
                if (CheckCollisionPointRec(mousePos, skipBackButton)) {
                    playback::skip(-skipSeconds);
                }
                if (CheckCollisionPointRec(mousePos, skipForwardButton)) {
                    playback::skip(skipSeconds);
                }
                if (CheckCollisionPointRec(mousePos, speedButton)) {
                    cycleSpeed();
                }
                if (CheckCollisionPointRec(mousePos, progressBarHit)) {
                    draggingProgress = true;
                }
                if (CheckCollisionPointRec(mousePos, loadButton)) {
                    std::string path = pickRecordingPathFromExplorer();
                    if (!path.empty()) {
                        loadRecordingOrMidi(path); // internally stops any current playback first
                    }
                }
            }
            else {
                if (CheckCollisionPointRec(mousePos, minusButton)) {
                    transposeDown();
                }
                if (CheckCollisionPointRec(mousePos, plusButton)) {
                    transposeUp();
                }
                if (CheckCollisionPointRec(mousePos, recordButton)) {
                    recording::toggle();
                }
            }
        }

        // Scrub-drag tracking runs independently of the press check above so
        // it keeps following the mouse even once it leaves progressBarHit,
        // and stops the moment the button comes up.
        if (draggingProgress) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float mouseX = GetMousePosition().x;
                float t = (progressBarVisual.width > 0.0f)
                    ? std::clamp((mouseX - progressBarVisual.x) / progressBarVisual.width, 0.0f, 1.0f)
                    : 0.0f;
                playback::seek(t * playback::getDuration());
            }
            else {
                draggingProgress = false;
            }
        }
    }
    

        void draw(int screenWidth) {
            unsigned char toolbarAlpha = (unsigned char)(240 * toolbarSlide);
            DrawRectangle(0, (int)toolbarY, screenWidth, (int)toolbarHeight, { 30, 30, 35, toolbarAlpha });

            constexpr int buttonFontSize = 20;
            constexpr Color playbackAccent = { 90, 170, 240, 255 };

            // --- Always shown, in both modes: settings + connection status ---
            DrawRectangleRec(settingsButton, GRAY);
            {
                constexpr int iconScale = 1;
                constexpr int iconSizePx = 16.0f * iconScale; // raygui icons are 16px square at scale 1
                float iconX = settingsButton.x + (settingsButton.width - iconSizePx) / 2.0f;
                float iconY = settingsButton.y + (settingsButton.height - iconSizePx) / 2.0f;
                GuiDrawIcon(ICON_GEAR, (int)iconX, (int)iconY, iconScale, RAYWHITE);
            }

            const char* midiStatusText = midiInInitialized ? "Connected" : "Disconnected";
            Color midiStatusColor = midiInInitialized ? Color{ 80, 220, 100, 255 } : Color{ 240, 90, 90, 255 };
            const int midiStatusX = (int)(settingsButton.x + settingsButton.width + 20.0f);
            DrawText(midiStatusText, midiStatusX, (int)settingsButton.y + 5, 18, midiStatusColor);

            // --- Playback button: always a right-pointing triangle, doubles
            // as the entry/exit toggle for playback mode in both layouts ---
            DrawRectangleRec(playbackButton, GRAY);
            {
                Vector2 center = {
                    playbackButton.x + playbackButton.width / 2.0f,
                    playbackButton.y + playbackButton.height / 2.0f
                };
                drawPlayTriangle(center, 14.0f, playbackModeActive ? playbackAccent : RAYWHITE);
            }

            if (playbackModeActive) {
                // --- Secondary toolbar: playback controls only ---
                DrawRectangleRec(playPauseButton, GRAY);
                {
                    Vector2 center = {
                        playPauseButton.x + playPauseButton.width / 2.0f,
                        playPauseButton.y + playPauseButton.height / 2.0f
                    };
                    if (playback::isPlaying()) {
                        drawPauseBars(center, 16.0f, RAYWHITE);
                    } else {
                        drawPlayTriangle(center, 18.0f, RAYWHITE);
                    }
                }

                DrawRectangleRec(skipBackButton, GRAY);
                {
                    Vector2 center = {
                        skipBackButton.x + skipBackButton.width / 2.0f,
                        skipBackButton.y + skipBackButton.height / 2.0f
                    };
                    drawSkipIcon(center, 16.0f, RAYWHITE, false);
                }

                DrawRectangleRec(skipForwardButton, GRAY);
                {
                    Vector2 center = {
                        skipForwardButton.x + skipForwardButton.width / 2.0f,
                        skipForwardButton.y + skipForwardButton.height / 2.0f
                    };
                    drawSkipIcon(center, 16.0f, RAYWHITE, true);
                }

                DrawRectangleRec(speedButton, GRAY);
                {
                    std::string speedText = formatSpeedLabel(speedSteps[speedStepIndex]);
                    int speedTextWidth = MeasureText(speedText.c_str(), buttonFontSize);
                    float speedTextX = speedButton.x + (speedButton.width - speedTextWidth) / 2.0f;
                    float speedTextY = speedButton.y + (speedButton.height - buttonFontSize) / 2.0f;
                    DrawText(speedText.c_str(), (int)speedTextX, (int)speedTextY, buttonFontSize, RAYWHITE);
                }

                // --- Load recording button, anchored where the transpose
                // cluster would be in normal mode ---
                DrawRectangleRec(loadButton, GRAY);
                {
                    const char* loadText = "Load File";
                    int loadTextWidth = MeasureText(loadText, buttonFontSize);
                    float loadTextX = loadButton.x + (loadButton.width - loadTextWidth) / 2.0f;
                    float loadTextY = loadButton.y + (loadButton.height - buttonFontSize) / 2.0f;
                    DrawText(loadText, (int)loadTextX, (int)loadTextY, buttonFontSize, RAYWHITE);
                }

                // --- Scrub bar ---
                DrawRectangleRec(progressBarVisual, Color{ 60, 60, 66, 255 });
                double duration = playback::getDuration();
                float progressFraction = 0.0f;
                if (duration > 0.0) {
                    progressFraction = std::clamp((float)(playback::getPlaybackTime() / duration), 0.0f, 1.0f);
                }
                if (progressBarVisual.width > 0.0f) {
                    Rectangle filled = progressBarVisual;
                    filled.width *= progressFraction;
                    DrawRectangleRec(filled, playbackAccent);

                    Vector2 knobCenter = {
                        progressBarVisual.x + progressBarVisual.width * progressFraction,
                        progressBarVisual.y + progressBarVisual.height / 2.0f
                    };
                    DrawCircleV(knobCenter, 6.0f, RAYWHITE);

                    // Elapsed / total time, right above the bar.
                    std::string timeText = formatTime(playback::getPlaybackTime()) + " / " + formatTime(duration);
                    DrawText(timeText.c_str(), (int)progressBarVisual.x, (int)(progressBarVisual.y - 16.0f), 14, Color{ 200, 200, 205, 255 });
                }

                return;
            }

            // --- Normal toolbar: record, transpose ---
            DrawRectangleRec(recordButton, GRAY);
            {
                constexpr Color recordRed = { 220, 50, 50, 255 };
                Vector2 center = {
                    recordButton.x + recordButton.width / 2.0f,
                    recordButton.y + recordButton.height / 2.0f
                };

                if (recording::isRecording()) {
                    // Recording: solid rounded red square.
                    constexpr float squareSize = 16.0f;
                    Rectangle square = {
                        center.x - squareSize / 2.0f,
                        center.y - squareSize / 2.0f,
                        squareSize,
                        squareSize
                    };
                    DrawRectangleRounded(square, 0.3f, 6, recordRed);
                } else {
                    // Idle: unfilled outer ring with a filled dot inside.
                    constexpr float outerRadius = 8.0f;
                    constexpr float innerRadius = 4.0f;
                    DrawRingLines(center, outerRadius - 1.5f, outerRadius, 0, 360, 24, recordRed);
                    DrawCircleV(center, innerRadius, recordRed);
                }
            }

            DrawRectangleRec(minusButton, GRAY);
            {
                int minusWidth = MeasureText("-", buttonFontSize);
                float minusX = minusButton.x + (minusButton.width - minusWidth) / 2.0f;
                float minusY = minusButton.y + (minusButton.height - buttonFontSize) / 2.0f;
                DrawText("-", (int)minusX, (int)minusY, buttonFontSize, RAYWHITE);
            }

            std::string transposeText = std::to_string(getTranspose());
            int textWidth = MeasureText(transposeText.c_str(), buttonFontSize);
            float textX = minusButton.x + minusButton.width + ((plusButton.x - (minusButton.x + minusButton.width) - textWidth) / 2.0f);
            float textY = minusButton.y + (minusButton.height - buttonFontSize) / 2.0f;
            DrawText(transposeText.c_str(), (int)textX, (int)textY, buttonFontSize, RAYWHITE);

            DrawRectangleRec(plusButton, GRAY);
            {
                int plusWidth = MeasureText("+", buttonFontSize);
                float plusX = plusButton.x + (plusButton.width - plusWidth) / 2.0f;
                float plusY = plusButton.y + (plusButton.height - buttonFontSize) / 2.0f;
                DrawText("+", (int)plusX, (int)plusY, buttonFontSize, RAYWHITE);
            }
        }

}
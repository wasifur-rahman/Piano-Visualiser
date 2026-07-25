#include "blocks.h"
#include "layout.h"
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

constexpr float RISE_SPEED = 220.0f; // pixels/sec block travel speed

// Base neon glow values at amount == 1.0 (the default/"normal" setting).
// The actual per-frame values are these bases scaled by blockShaderAmount.
constexpr float NEON_BLUR_SPREAD_BASE = 5.0f;
constexpr float NEON_GLOW_BOOST_BASE = 4.0f;

namespace {
    std::vector<ActiveNote> activeNotes;
    float blockRoundedness = 0.0f;
    blocks::ShaderStyle currentShaderStyle = blocks::ShaderStyle::None;
    float blockShaderAmount = 1.0f; // generic "more" slider value for whichever style is active

    // --- Block material shader ---------------------------------------------
    Shader blockShader = { 0 };
    bool blockShaderLoaded = false;
    bool blockShaderLoadAttempted = false;
    int modeLoc = -1;
    int rectPosLoc = -1;
    int rectSizeLoc = -1;
    int screenHeightLoc = -1;
    int amountLoc = -1;

    // --- Neon glow: two-pass gaussian blur via offscreen render textures ----
    Shader blurShader = { 0 };
    bool blurShaderLoaded = false;
    bool blurShaderLoadAttempted = false;
    int blurDirectionLoc = -1;
    int blurTexelSizeLoc = -1;
    int blurBoostLoc = -1;

    RenderTexture2D glowSceneTex = { 0 };
    RenderTexture2D glowBlurTex = { 0 };
    int glowTexWidth = 0;
    int glowTexHeight = 0;
    bool glowTexturesReady = false;

    std::string resolveAssetShaderPath(const std::string& filename) {
        const std::filesystem::path currentDir = std::filesystem::current_path();
        const std::filesystem::path sourceDir = std::filesystem::path(__FILE__).parent_path();
        const std::filesystem::path projectRoot = sourceDir.parent_path();

        const std::vector<std::filesystem::path> searchRoots = {
            currentDir,
            sourceDir,
            projectRoot,
            std::filesystem::path("C:/Users/wasif/Desktop/PianoVisualiser")
        };

        for (const auto& root : searchRoots) {
            const std::filesystem::path path = root / "assets/shaders" / filename;
            if (std::filesystem::exists(path)) {
                return std::filesystem::absolute(path).string();
            }
        }
        return {};
    }

    void ensureShaderLoaded() {
        if (blockShaderLoadAttempted) return;
        blockShaderLoadAttempted = true;

        const std::string path = resolveAssetShaderPath("block.fs");
        if (path.empty()) {
            std::cerr << "Block shader not found. Expected assets/shaders/block.fs under the project root." << std::endl;
            return;
        }

        blockShader = LoadShader(nullptr, path.c_str());
        if (blockShader.id == 0) {
            std::cerr << "Failed to compile block shader: " << path << std::endl;
            return;
        }

        modeLoc         = GetShaderLocation(blockShader, "u_mode");
        rectPosLoc      = GetShaderLocation(blockShader, "u_rectPos");
        rectSizeLoc     = GetShaderLocation(blockShader, "u_rectSize");
        screenHeightLoc = GetShaderLocation(blockShader, "u_screenHeight");
        amountLoc       = GetShaderLocation(blockShader, "u_amount");
        blockShaderLoaded = true;
        std::cout << "Block shader loaded: " << path << std::endl;
    }

    void ensureBlurShaderLoaded() {
        if (blurShaderLoadAttempted) return;
        blurShaderLoadAttempted = true;

        const std::string path = resolveAssetShaderPath("blur.fs");
        if (path.empty()) {
            std::cerr << "Blur shader not found. Expected assets/shaders/blur.fs under the project root." << std::endl;
            return;
        }

        blurShader = LoadShader(nullptr, path.c_str());
        if (blurShader.id == 0) {
            std::cerr << "Failed to compile blur shader: " << path << std::endl;
            return;
        }

        blurDirectionLoc = GetShaderLocation(blurShader, "u_direction");
        blurTexelSizeLoc = GetShaderLocation(blurShader, "u_texelSize");
        blurBoostLoc     = GetShaderLocation(blurShader, "u_boost");
        blurShaderLoaded = true;
        std::cout << "Blur shader loaded: " << path << std::endl;
    }

    void ensureGlowTextures(int w, int h) {
        if (glowTexturesReady && glowTexWidth == w && glowTexHeight == h) return;

        if (glowTexturesReady) {
            UnloadRenderTexture(glowSceneTex);
            UnloadRenderTexture(glowBlurTex);
        }

        glowSceneTex = LoadRenderTexture(w, h);
        glowBlurTex  = LoadRenderTexture(w, h);
        glowTexWidth = w;
        glowTexHeight = h;
        glowTexturesReady = true;
    }
}

namespace blocks {
    void setRoundedness(float r) {
        blockRoundedness = std::clamp(r, 0.0f, 1.0f);
    }
    float getRoundedness() {
        return blockRoundedness;
    }

    void setShaderStyle(ShaderStyle style) {
        currentShaderStyle = style;
    }
    ShaderStyle getShaderStyle() {
        return currentShaderStyle;
    }

    void setShaderAmount(float amount) {
        blockShaderAmount = std::clamp(amount, 0.0f, 3.0f);
    }
    float getShaderAmount() {
        return blockShaderAmount;
    }

    void onNoteOn(int pitch, int velocity) {
        activeNotes.push_back({ pitch, velocity, (float)(GetScreenHeight() - PIANO_HEIGHT), true, 0.0f });
    }
    void onNoteOff(int pitch) {
        for (auto& n : activeNotes) {
            if (n.pitch == pitch && n.held) {
                n.held = false;
            }
        }
    }
    void update(float dt) {
        for (auto& n : activeNotes) {
            if (n.held) n.length += RISE_SPEED * dt;
            n.startY -= RISE_SPEED * dt;
        }
        activeNotes.erase(
            std::remove_if(activeNotes.begin(), activeNotes.end(),
                [](const ActiveNote& n) { return n.startY + n.length < 0; }),
            activeNotes.end());
    }

    void draw(int screenWidth) {
        ensureShaderLoaded();

        const bool useShader = blockShaderLoaded && currentShaderStyle != ShaderStyle::None;
        const bool isNeon = useShader && currentShaderStyle == ShaderStyle::Neon;

        if (isNeon) {
            ensureBlurShaderLoaded();

            if (blurShaderLoaded) {
                const int screenHeight = GetScreenHeight();
                ensureGlowTextures(screenWidth, screenHeight);

                const float effectiveSpread = NEON_BLUR_SPREAD_BASE * blockShaderAmount;
                const float effectiveBoost  = NEON_GLOW_BOOST_BASE * blockShaderAmount;

                BeginTextureMode(glowSceneTex);
                ClearBackground(BLANK);
                for (auto& n : activeNotes) {
                    KeyRect kr = layout::getKeyRect(n.pitch, screenWidth);
                    Color c = layout::colorForPitch(n.pitch, n.velocity);
                    DrawRectangleRounded({ kr.x, n.startY, kr.width - 2, n.length }, blockRoundedness, 8, c);
                }
                EndTextureMode();

                const float texelSize[2] = {
                    (1.0f / (float)glowTexWidth)  * effectiveSpread,
                    (1.0f / (float)glowTexHeight) * effectiveSpread
                };

                BeginTextureMode(glowBlurTex);
                ClearBackground(BLANK);
                BeginShaderMode(blurShader);
                {
                    const float dirH[2] = { 1.0f, 0.0f };
                    const float boostNone = 1.0f;
                    SetShaderValue(blurShader, blurDirectionLoc, dirH, SHADER_UNIFORM_VEC2);
                    SetShaderValue(blurShader, blurTexelSizeLoc, texelSize, SHADER_UNIFORM_VEC2);
                    SetShaderValue(blurShader, blurBoostLoc, &boostNone, SHADER_UNIFORM_FLOAT);
                    DrawTextureRec(glowSceneTex.texture,
                        { 0, 0, (float)glowTexWidth, -(float)glowTexHeight }, { 0, 0 }, WHITE);
                }
                EndShaderMode();
                EndTextureMode();

                rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM);
                BeginShaderMode(blurShader);
                {
                    const float dirV[2] = { 0.0f, 1.0f };
                    SetShaderValue(blurShader, blurDirectionLoc, dirV, SHADER_UNIFORM_VEC2);
                    SetShaderValue(blurShader, blurTexelSizeLoc, texelSize, SHADER_UNIFORM_VEC2);
                    SetShaderValue(blurShader, blurBoostLoc, &effectiveBoost, SHADER_UNIFORM_FLOAT);
                    DrawTextureRec(glowBlurTex.texture,
                        { 0, 0, (float)glowTexWidth, -(float)glowTexHeight }, { 0, 0 }, WHITE);
                }
                EndShaderMode();
                EndBlendMode();
            }
        }

        for (auto& n : activeNotes) {
            KeyRect kr = layout::getKeyRect(n.pitch, screenWidth);
            Color c = layout::colorForPitch(n.pitch, n.velocity);

            if (useShader) {
                BeginShaderMode(blockShader);
                const int modeInt = (int)currentShaderStyle;
                const float rectPos[2]  = { kr.x, n.startY };
                const float rectSize[2] = { kr.width - 2, n.length };
                const float screenHeightF = (float)GetScreenHeight();
                SetShaderValue(blockShader, modeLoc, &modeInt, SHADER_UNIFORM_INT);
                SetShaderValue(blockShader, rectPosLoc, rectPos, SHADER_UNIFORM_VEC2);
                SetShaderValue(blockShader, rectSizeLoc, rectSize, SHADER_UNIFORM_VEC2);
                SetShaderValue(blockShader, screenHeightLoc, &screenHeightF, SHADER_UNIFORM_FLOAT);
                SetShaderValue(blockShader, amountLoc, &blockShaderAmount, SHADER_UNIFORM_FLOAT);
            }

            DrawRectangleRounded({ kr.x, n.startY, kr.width - 2, n.length }, blockRoundedness, 8, c);

            if (useShader) {
                rlDrawRenderBatchActive();
                EndShaderMode();
            }
        }
    }

    const std::vector<ActiveNote>& getActive() {
        return activeNotes;
    }
}
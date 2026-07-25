#pragma once
namespace audio {
    void init();
    void noteOn(int pitch, int velocity);
    void noteOff(int pitch);
    void controlChange(int cc, int value);
    void shutdown();

    // --- Runtime-adjustable settings (safe to call after init()) ---
    void setGain(float gain);
    float getGain();

    void setReverb(double roomSize, double damping, double width, double level);
    void getReverb(double& roomSize, double& damping, double& width, double& level);

    void setChorus(int voiceCount, double level, double speed, double depth);
    void getChorus(int& voiceCount, double& level, double& speed, double& depth);
}
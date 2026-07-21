#pragma once

namespace audio {
	void init();
	void noteOn(int pitch, int velocity);
	void noteOff(int pitch);
	void controlChange(int cc, int value);
	void shutdown();
}
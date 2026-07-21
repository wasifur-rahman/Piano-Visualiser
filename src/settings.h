#pragma once

// User-adjustable settings. Right now just transpose, but this is the
// natural home for future customization (volume, colors, keybinds, etc.)
// plus load()/save() to persist them to disk.

namespace settings {
    void transposeUp();
    void transposeDown();
    int getTranspose();
}

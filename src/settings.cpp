#include "settings.h"

namespace {
    int transposeSemitones = 0;
    constexpr int TRANSPOSE_MIN = -6;
    constexpr int TRANSPOSE_MAX = 6;
}

namespace settings {

    void transposeUp() {
        if (transposeSemitones < TRANSPOSE_MAX) transposeSemitones++;
    }

    void transposeDown() {
        if (transposeSemitones > TRANSPOSE_MIN) transposeSemitones--;
    }

    int getTranspose() {
        return transposeSemitones;
    }

}

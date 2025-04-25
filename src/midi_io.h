#pragma once
#include <string>
#include <vector>
#include "dtw_aligner.h"

namespace MIDIIO {
    std::vector<NoteEvent> parse_midi(const std::string& path);
}
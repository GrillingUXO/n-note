#pragma once
#include <string>
#include <vector>
#include "common_defs.h" 

namespace MIDIIO {
    std::vector<NoteEvent> parse_midi(const std::string& path);
    double extract_bpm(const std::string& path); 
}
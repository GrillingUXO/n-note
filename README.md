
```powershell
g++ -std=c++17 -g -O0 -I. -Imidifile/include \
    tinyfiledialogs.o \
    dtw_core.cpp midi_io.cpp gui.cpp main.cpp \
    midifile/lib/libmidifile.a \
    -framework Cocoa \
    -o n-note
```

```powershell
g++ -std=c++17 -I. -Imidifile/include \
    tinyfiledialogs.o \
    dtw_aligner.cpp similarity_calculator.cpp midi_io.cpp main.cpp \
    midifile/lib/libmidifile.a \
    -framework Cocoa \
    -o n-note
    ```

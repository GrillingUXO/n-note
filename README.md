

![download (1)](https://github.com/user-attachments/assets/08ac5837-728c-455c-af33-2ead86b95142)







```powershell
g++ -std=c++17 -I. -Imidifile/include \
    tinyfiledialogs.o \
    dtw_aligner.cpp similarity_calculator.cpp midi_io.cpp main.cpp \
    midifile/lib/libmidifile.a \
    -framework Cocoa \
    -o n-note
```

```powershell
g++ -std=c++17 -I. -Imidifile/include \
    tinyfiledialogs.o \
    similarity_calculator.cpp midi_io.cpp main.cpp \
    midifile/lib/libmidifile.a \
    -framework Cocoa \
    -o n-note

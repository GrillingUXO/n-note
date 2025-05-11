
![download (2)](https://github.com/user-attachments/assets/cd5195df-bcec-4869-b520-3066a4e09557)



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

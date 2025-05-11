
![download (2)](https://github.com/user-attachments/assets/e76f2379-379a-43c5-94d7-04a586f228d9)


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

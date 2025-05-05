

![download](https://github.com/user-attachments/assets/4528b7ab-3f89-4432-adbc-7592ec33534a)
![download 2](https://github.com/user-attachments/assets/2f9bed8f-0cac-4f9b-843e-b6a3606d2e4d)





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

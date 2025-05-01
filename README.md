


<img width="1217" alt="Screen Shot 2025-05-01 at 6 59 36 PM" src="https://github.com/user-attachments/assets/559bf56c-57a5-490e-a796-8d14618c0f2f" />

<img width="1217" alt="Screen Shot 2025-05-01 at 7 00 09 PM" src="https://github.com/user-attachments/assets/4cab2e2f-c436-4172-b40a-c60dc4724244" />






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

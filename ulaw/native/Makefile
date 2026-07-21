TARGET = ulaw-native

CXX      = g++
CXXFLAGS = -std=c++14 -O2 -Wall $(shell pkg-config --cflags portaudio-2.0 sndfile)
LDFLAGS  = $(shell pkg-config --libs portaudio-2.0 sndfile)

SOURCES = main.cpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

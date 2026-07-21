TARGET      = chorus-native
DAISYSP_DIR = ../../../DaisyExamples/DaisySP

CXX      = g++
CXXFLAGS = -std=c++14 -O2 -Wall -I$(DAISYSP_DIR)/Source -I$(DAISYSP_DIR)/Source/Utility \
           $(shell pkg-config --cflags portaudio-2.0 sndfile)
LDFLAGS  = $(shell pkg-config --libs portaudio-2.0 sndfile)

SOURCES = main.cpp $(DAISYSP_DIR)/Source/Effects/chorus.cpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

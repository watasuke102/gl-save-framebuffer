CXXFLAGS := -Wall -Wextra -Wpedantic -O0 -g -lGL -lglfw
SRC := main.cpp shader.cpp

all: gl gles

gl: $(SRC)
	$(CXX) $(CXXFLAGS) -lGLEW      -o $@ $^
gles: $(SRC)
	$(CXX) $(CXXFLAGS) -DUSE_GLES -o $@ $^


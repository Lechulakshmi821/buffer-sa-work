CXX=g++
CXXFLAGS=-O3

all:
	$(CXX) $(CXXFLAGS) src/main.cpp src/dataset.cpp src/rle.cpp -o benchmark

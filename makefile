NVCC=nvcc
NVCCFLAGS=-O3 -std=c++17

all:
	$(NVCC) $(NVCCFLAGS) src/main.cpp src/dataset.cpp src/rle.cpp src/rle_gpu.cu -o benchmark

clean:
	rm -f benchmark results.csv

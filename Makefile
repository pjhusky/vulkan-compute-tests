EXE=mandelbrot-mac

all: $(EXE)

$(EXE): src/main.cpp src/lodepng.cpp src/lodepng.h shaders/mandelbrot.spv
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ src/main.cpp src/lodepng.cpp -o $(EXE) -L$(VULKAN_SDK)lib/ -lvulkan

shaders/mandelbrot.spv: shaders/mandelbrot.comp
	$(VULKAN_SDK)bin/glslangValidator -V shaders/mandelbrot.comp -o shaders/mandelbrot.spv

clean:
	rm -f $(EXE)

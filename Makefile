MANDEL_EXE=mandelbrot-mac
PATHTRACER_EXE=pocketpt-mac

all: $(MANDEL_EXE) $(PATHTRACER_EXE)

$(MANDEL_EXE): src/main.cpp src/lodepng.cpp src/lodepng.h shaders/mandelbrot.spv
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ -DMANDELBROT_MODE src/main.cpp src/lodepng.cpp -o $(MANDEL_EXE) -L$(VULKAN_SDK)lib/ -lvulkan

shaders/mandelbrot.spv: shaders/mandelbrot.comp
	$(VULKAN_SDK)bin/glslangValidator -V shaders/mandelbrot.comp -o shaders/mandelbrot.spv

$(PATHTRACER_EXE): src/main.cpp src/lodepng.cpp src/lodepng.h shaders/pathTracer.spv
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ -DPATHTRACER_MODE src/main.cpp src/lodepng.cpp -o $(PATHTRACER_EXE) -L$(VULKAN_SDK)lib/ -lvulkan

shaders/pathTracer.spv: shaders/pathTracer.comp
	$(VULKAN_SDK)bin/glslangValidator -V shaders/pathTracer.comp -o shaders/pathTracer.spv

clean:
	rm -f $(MANDEL_EXE) $(PATHTRACER_EXE)

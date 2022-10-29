MANDEL_EXE=mandelbrot-mac

DEBUG_FLAGS=
# DEBUG_FLAGS=-DNDEBUG

UTIL_HEADERS=src/lodepng.h
UTIL_CPPS=src/lodepng.cpp

all: $(MANDEL_EXE)

$(MANDEL_EXE): src/main.cpp $(UTIL_HEADERS) $(UTIL_CPPS) shaders/mandelbrot.generated.spv Makefile
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include -DMANDELBROT_MODE $(DEBUG_FLAGS) src/main.cpp $(UTIL_CPPS) -o $(MANDEL_EXE) -L$(VULKAN_SDK)lib -lvulkan

shaders/mandelbrot.generated.spv: shaders/mandelbrot.comp Makefile
	$(VULKAN_SDK)bin/glslangValidator -V shaders/mandelbrot.comp -o shaders/mandelbrot.generated.spv

run: $(MANDEL_EXE)
	./$(MANDEL_EXE) && qlmanage -p pathtracer.png >> /dev/null 2>&1 


clean:
	rm -f $(MANDEL_EXE) mandelbrot.png shaders/mandelbrot.generated.spv

MANDEL_EXE=mandelbrot-mac
PATHTRACER_EXE=pocketpt-mac

DEBUG_FLAGS=
# DEBUG_FLAGS=-DNDEBUG

UTIL_HEADERS=src/vulkanComputeApp.h src/external/lodepng/lodepng.h
UTIL_CPPS=src/vulkanComputeApp.cpp src/external/lodepng/lodepng.cpp

all: $(MANDEL_EXE) $(PATHTRACER_EXE)

$(MANDEL_EXE): src/main.cpp $(UTIL_HEADERS) $(UTIL_CPPS) src/mandelbrotApp.h shaders/mandelbrot.generated.spv Makefile
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include -DMANDELBROT_MODE $(DEBUG_FLAGS) src/main.cpp $(UTIL_CPPS) -o $(MANDEL_EXE) -L$(VULKAN_SDK)lib -lvulkan

shaders/mandelbrot.generated.spv: shaders/mandelbrot.comp Makefile
	$(VULKAN_SDK)bin/glslangValidator -V shaders/mandelbrot.comp -o shaders/mandelbrot.generated.spv

$(PATHTRACER_EXE): src/main.cpp $(UTIL_HEADERS) $(UTIL_CPPS) src/pathtracerApp.h shaders/pathtracer.generated.spv Makefile
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ -DPATHTRACER_MODE $(DEBUG_FLAGS) src/main.cpp $(UTIL_CPPS) -o $(PATHTRACER_EXE) -L$(VULKAN_SDK)lib/ -lvulkan

shaders/pathtracer.generated.spv: shaders/pathtracer.comp shaders/emulateDouble.h.glsl Makefile
	@#$(VULKAN_SDK)bin/glslc -E shaders/pathtracer.comp -o shaders/pathtracer.preprocessed.comp
	@#$(VULKAN_SDK)bin/glslangValidator -V shaders/pathtracer.preprocessed.comp -o shaders/pathtracer.generated.spv
	$(VULKAN_SDK)bin/glslc -O0 shaders/pathtracer.comp -o shaders/pathtracer.generated.spv
	rm -f shaders/pathtracer.preprocessed.comp

lofi-run: $(PATHTRACER_EXE)
	./$(PATHTRACER_EXE) 100 400 && qlmanage -p pathtracer.png >> /dev/null 2>&1 

run: $(PATHTRACER_EXE)
	./$(PATHTRACER_EXE) && qlmanage -p pathtracer.png >> /dev/null 2>&1 


clean:
	rm -f $(MANDEL_EXE) $(PATHTRACER_EXE) pathtracer.png mandelbrot.png shaders/pathtracer.generated.spv shaders/mandelbrot.generated.spv

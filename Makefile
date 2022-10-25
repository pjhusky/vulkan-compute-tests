MANDEL_EXE=mandelbrot-mac
PATHTRACER_EXE=pocketpt-mac

all: $(MANDEL_EXE) $(PATHTRACER_EXE)

$(MANDEL_EXE): src/main.cpp src/lodepng.cpp src/lodepng.h shaders/mandelbrot.generated.spv Makefile
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include -DMANDELBROT_MODE -DNDEBUG src/main.cpp src/lodepng.cpp -o $(MANDEL_EXE) -L$(VULKAN_SDK)lib -lvulkan

shaders/mandelbrot.generated.spv: shaders/mandelbrot.comp Makefile
	$(VULKAN_SDK)bin/glslangValidator -V shaders/mandelbrot.comp -o shaders/mandelbrot.generated.spv

$(PATHTRACER_EXE): src/main.cpp src/lodepng.cpp src/lodepng.h shaders/pathTracer.generated.spv Makefile
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ -DPATHTRACER_MODE -DNDEBUG src/main.cpp src/lodepng.cpp -o $(PATHTRACER_EXE) -L$(VULKAN_SDK)lib/ -lvulkan

#shaders/pathTracer.generated.spv: shaders/pathTracer.comp shaders/emulateDouble.h.glsl shaders/*.h.glsl Makefile
#shaders/pathTracer.generated.spv: shaders/pathTracer.comp shaders/*.h.glsl Makefile
shaders/pathTracer.generated.spv: shaders/pathTracer.comp shaders/emulateDouble.h.glsl Makefile
#$(VULKAN_SDK)bin/glslangValidator -V shaders/pathTracer.comp -o shaders/pathTracer.generated.spv
	@#$(VULKAN_SDK)bin/glslc -E shaders/pathTracer.comp -o shaders/pathTracer.preprocessed.comp
	@#$(VULKAN_SDK)bin/glslangValidator -V shaders/pathTracer.preprocessed.comp -o shaders/pathTracer.generated.spv
	$(VULKAN_SDK)bin/glslc -O0 shaders/pathTracer.comp -o shaders/pathTracer.generated.spv
	rm -f shaders/pathTracer.preprocessed.comp

lofi-run: $(PATHTRACER_EXE)
	./$(PATHTRACER_EXE) 100 400 && qlmanage -p pathTracer.png >> /dev/null 2>&1 

run: $(PATHTRACER_EXE)
	./$(PATHTRACER_EXE) && qlmanage -p pathTracer.png >> /dev/null 2>&1 


clean:
	rm -f $(MANDEL_EXE) $(PATHTRACER_EXE) pathTracer.png mandelbrot.png

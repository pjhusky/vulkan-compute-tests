EXE=mandelbrot-mac

$(EXE): src/main.cpp src/lodepng.cpp src/lodepng.h
#g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ example1.cpp -o $(EXE) -framework Vulkan
	g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ src/main.cpp src/lodepng.cpp -o $(EXE) -L$(VULKAN_SDK)lib/ -lvulkan

clean:
	rm -f $(EXE)

# Vulkan Minimal Compute

This is a simple demo that demonstrates how to use Vulkan for compute operations only.
In other words, this demo does nothing related to graphics, 
and only uses Vulkan to execute some computation on the GPU.
For this demo, Vulkan is used to path-trace a simple scene / calculate the Mandelbrot set on the GPU. The demos are very simple and act more as illustrative examples of how to use compute shaders with Vulkan. **The code is heavily commented, so it should be useful for people interested in learning Vulkan**.

The only depdendencies are Vulkan and `lodepng`. `lodepng` is merely used for png encoding. Vulkan can be installed
from `lunarg.com`

Since on macOS, Vulkan does not support double-precision floats, 64-bit floats are emulated using two 32-bit single-precision floats where necessary.

![](imageForReadme.png)

# Demo

The application launches a compute shader that renders the mandelbrot set or renders a simple scene using path tracing, by writing results into a storage buffer.
The storage buffer is then read from the GPU, and saved as `.png`. Check the source code comments
for further info.

## Building

The project uses a bare-bones Makefile, which can also be run on Windows using mingw32-make -f Makefile.win32. When running the programs,
the png files named `mandelbrot.png` and 'pathtracer.png' are be created. 
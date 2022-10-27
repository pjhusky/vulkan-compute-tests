# Vulkan Minimal Compute

This is a simple demo that demonstrates how to use Vulkan compute shaders to path-trace a simple scene / calculate the Mandelbrot set on the GPU. **The code is heavily commented, so it can be seen as a rough getting-started tutorial for using Vulkan compute shaders**.

The only depdendencies are Vulkan and `lodepng`, which is merely used for png encoding. Vulkan can be installed from `lunarg.com`

Since on macOS, Vulkan does not support double-precision floats, 64-bit floats are emulated using two 32-bit single-precision floats where necessary.

![](imageForReadme.png)

# Demo

The application launches a compute shader that renders a simple scene using path tracing / calculates the Mandelbrot set, and writes resulting pixels into a storage buffer.
The storage buffer is then transferred from GPU- to CPU memory, and saved as `.png`.

## Building

The project uses a bare-bones Makefile, which can also be run on Windows using `mingw32-make -f Makefile.win32` (tested with a fairly recent [tdm-gcc](https://jmeubank.github.io/tdm-gcc/) installation on Windows 10). When running the created programs, the png files named `mandelbrot.png` /  `pathtracer.png` are created. 
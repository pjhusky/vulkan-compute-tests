* in general, follow the guide at
    ```
    https://vulkan.lunarg.com/doc/view/1.1.108.0/mac/getting_started.html
    ```
    * NOTE: maybe follow these short instructions from
        ```
        https://github.com/Novum/vkQuake
        ```
        ==> To compile vkQuake, first install the build dependencies with Homebrew:
        ```
        brew install molten-vk vulkan-headers sdl2 libvorbis flac mad
        ```
        NOTE: it was enough to 
        ```
        brew install sdl2 libvorbis flac mad
        ```
        so it seems, that the Vulkan/Molten-VK part was installed in a fully compatible way - this seems to support the assumption, that Vulkan support could just as well be installed through homebrew as well, which would be a lot easier/faster!
        

* TL;DR:

    * download latest Vulkan SDK for Mac:
    ```
    https://vulkan.lunarg.com/sdk/home#mac
    ```

    * open the .dmg file and extract it to any folder

        * in the following, the files were put into
        ```
        ~/DEV/VulkanSDKs/<currentVulkanSdkDmgVersion>
        ```
        but you can really put them anywhere you like (maybe you'll need access rights etc., so choose wisely).

    * put the following lines into ~/.zshrc
    ```
    # VULKAN setup from
    # https://vulkan.lunarg.com/doc/view/1.1.108.0/mac/getting_started.html
    # NOTE: the etc/ path was changed to share/ apparently at some point after the
    #       tutorial was put together
    export VULKAN_VERSION=vulkansdk-macos-1.2.162.1
    export VULKAN_VERSION_BASE_DIR=~/DEV/VulkanSDKs/${VULKAN_VERSION}/
    export VULKAN_SDK=${VULKAN_VERSION_BASE_DIR}macOS/
    export PATH=${PATH}:${VULKAN_SDK}bin/
    export DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}:${VULKAN_SDK}lib/
    export VK_LAYER_PATH=${VULKAN_SDK}share/vulkan/explicit_layer.d
    export VK_ICD_FILENAMES=${VULKAN_SDK}share/vulkan/icd.d/MoltenVK_icd.json
    # https://github.com/KhronosGroup/Vulkan-Loader/blob/master/loader/LoaderAndLayerInterface.md#table-of-debug-environment-variables
    # export VK_LOADER_DEBUG=all
    export VK_LOADER_DEBUG=error
    ```

    * run 'source ~/zshrc', or open a new Terminal

    * add vulkan-header includePath to Visual Studio Code:

        * 'View|Command Palette...', or shortcut 'CMD+SHIFT+P' (opens command palette)

        * type 'Edit Configurations' and choose 'C/C++: Edit Configurations (JSON)', which opens '.vscode/c_cpp_properties.json' in your project

        * in that json file, under "configurations"|"includePath", add
        ```
        "${VULKAN_SDK}include/**"
        ```

        * ==> '.vscode/c_cpp_properties.json' should look like this:
        ```
        {
            "configurations": [
                {
                    "name": "Mac",
                    "includePath": [
                        "${workspaceFolder}/**",
                        "${VULKAN_SDK}include/**"
                    ],
                    ...
                }
            ...
        }
        ```

        * restart Visual Studio Code, and note how
        ```
        #include <vulkan/vulkan.h>
        ```
        is no longer wiggly underlined in red

    * building C/C++ vulkan applications should work along these lines:
    ```
    g++ -std=c++11 -O3 -I$(VULKAN_SDK)include/ src/main.cpp -o $(EXE) -L$(VULKAN_SDK)lib/ -lvulkan
    ```

    * good starting point for vulkan compute (calculates the mandelbrot set and writes it to a .png file):
    ```
    https://github.com/Erkaman/vulkan_minimal_compute
    ```

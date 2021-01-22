
#include <vulkan/vulkan.h>

#include <vector>
//#include <array>
#include <algorithm>

#include <string.h>
#include <assert.h>
#include <stdexcept>
#include <cmath>

#include "lodepng.h" //Used for png encoding.

#define BAIL_ON_BAD_RESULT(result) \
  if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }


#if !defined( MANDELBROT_MODE ) && !defined( PATHTRACER_MODE )
    #define PATHTRACER_MODE
#endif

#if defined ( MANDELBROT_MODE )
const int WIDTH = 3200; // Size of rendered mandelbrot set.
const int HEIGHT = 2400; // Size of renderered mandelbrot set.
const int WORKGROUP_SIZE = 32; // Workgroup size in compute shader.
#elif defined ( PATHTRACER_MODE )
const int WIDTH = 600; // Size of rendered mandelbrot set.
const int HEIGHT = 400; // Size of renderered mandelbrot set.
const int WORKGROUP_SIZE = 16; // Workgroup size in compute shader.

constexpr int spp = 500;    // samples per pixel 
// int resy = 600;    // vertical pixel resolution
// int resx = resy*3/2;	                    // horiziontal pixel resolution
int resx = WIDTH;
int resy = HEIGHT;

#define TEST_PRECISION_WITH_LARGE_SPHERE_WALLS  0

float planes[] = { // normal.xyz, distToOrigin  |  emmission.xyz, 0  |  color.rgb, refltype     
    #if ( TEST_PRECISION_WITH_LARGE_SPHERE_WALLS == 0 )
        -1.0f,  +0.0f,  +0.0f,  +2.6f,      0, 0, 0, 0,     .85, .25, .25,  1, // Left
        +1.0f,  +0.0f,  +0.0f,  +2.6f,      0, 0, 0, 0,     .25, .35, .85,  1, // Right
        +0.0f,  +1.0f,  +0.0f,  +2.0f,      0, 0, 0, 0,     .75, .75, .75,  1, // Top
        +0.0f,  -1.0f,  +0.0f,  +2.0f,      0, 0, 0, 0,     .75, .75, .75,  1, // Bottom
        +0.0f,  +0.0f,  -1.0f,  +2.8f,      0, 0, 0, 0,     .85, .85, .25,  1, // Back
        +0.0f,  +0.0f,  +1.0f,  +7.9f,      0, 0, 0, 0,     0.1, 0.7, 0.7,  1, // Front
    #else // must have at least one plane here
        1,0,0,1000,    0,0,0,0,     1,1,1,1, // can't allocate buffers of size 0 => insert one zero dummy plane
    #endif
};

float spheres[] = {  // center.xyz, radius  |  emmission.xyz, 0  |  color.rgb, refltype     
    #if ( TEST_PRECISION_WITH_LARGE_SPHERE_WALLS != 0 )
        1e5 - 2.6, 0, 0, 1e5,   0, 0, 0, 0,  .85, .25, .25,  1, // Left (DIFFUSE)
        1e5 + 2.6, 0, 0, 1e5,   0, 0, 0, 0,  .25, .35, .85,  1, // Right
        0, 1e5 + 2, 0, 1e5,     0, 0, 0, 0,  .75, .75, .75,  1, // Top
        0,-1e5 - 2, 0, 1e5,     0, 0, 0, 0,  .75, .75, .75,  1, // Bottom
        0, 0, -1e5 - 2.8, 1e5,  0, 0, 0, 0,  .85, .85, .25,  1, // Back 
        0, 0, 1e5 + 7.9, 1e5,   0, 0, 0, 0,  0.1, 0.7, 0.7,  1, // Front
    #endif
    -1.3, -1.2, -1.3, 0.8,  0, 0, 0, 0,  .999,.999,.999, 2, // REFLECTIVE
    1.3, -1.2, -0.2, 0.8,   0, 0, 0, 0,  .999,.999,.999, 3, // REFRACTIVE
    0, 2*0.8, 0, 0.2,       100,100,100,0,  0, 0, 0,   1, // Light
};

struct pushConst_t {
    uint32_t imgdim[2]{ WIDTH, HEIGHT };
    uint32_t samps[2]{ 0, spp };
} pushConst;

#endif

//#if 1
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Used for validating return values of Vulkan API calls.
#define VK_CHECK_RESULT(f) 																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);																		\
    }																									\
}

/*
The application launches a compute shader that renders the mandelbrot set,
by rendering it into a storage buffer.
The storage buffer is then read from the GPU, and saved as .png.
*/
class ComputeApplication {
private:
    // The pixels of the rendered mandelbrot set are in this format:
    struct Pixel {
        float r, g, b, a;
    };

    /*
    In order to use Vulkan, you must create an instance.
    */
    VkInstance instance;

    VkDebugReportCallbackEXT debugReportCallback;
    /*
    The physical device is some device on the system that supports usage of Vulkan.
    Often, it is simply a graphics card that supports Vulkan.
    */
    VkPhysicalDevice physicalDevice;
    /*
    Then we have the logical device VkDevice, which basically allows
    us to interact with the physical device.
    */
    VkDevice device;

    /*
    The pipeline specifies the pipeline that all graphics and compute commands pass though in Vulkan.

    We will be creating a simple compute pipeline in this application.
    */
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule computeShaderModule;

    /*
    The command buffer is used to record commands, that will be submitted to a queue.

    To allocate such command buffers, we use a command pool.
    */
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    /*

    Descriptors represent resources in shaders. They allow us to use things like
    uniform buffers, storage buffers and images in GLSL.

    A single descriptor represents a single resource, and several descriptors are organized
    into descriptor sets, which are basically just collections of descriptors.
    */
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    /*
    The mandelbrot set will be rendered to this buffer.

    The memory that backs the buffer is bufferMemory.
    */
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
    uint32_t bufferSize; // size of `buffer` in bytes.

#if defined( PATHTRACER_MODE )
    VkBuffer planeBuffer;
    VkDeviceMemory planeBufferMemory;
    uint32_t planeBufferSize = sizeof( planes ); // size of `buffer` in bytes.

    VkBuffer sphereBuffer;
    VkDeviceMemory sphereBufferMemory;
    uint32_t sphereBufferSize = sizeof( spheres ); // size of `buffer` in bytes.
#endif

    std::vector<const char *> enabledLayers;

    /*
    In order to execute commands on a device(GPU), the commands must be submitted
    to a queue. The commands are stored in a command buffer, and this command buffer
    is given to the queue.

    There will be different kinds of queues on the device. Not all queues support
    graphics operations, for instance. For this application, we at least want a queue
    that supports compute operations.
    */
    VkQueue queue; // a queue supporting compute operations.

    /*
    Groups of queues that have the same capabilities(for instance, they all supports graphics and computer operations),
    are grouped into queue families.

    When submitting a command buffer, you must specify to which queue in the family you are submitting to.
    This variable keeps track of the index of that queue in its family.
    */
    uint32_t queueFamilyIndex;

public:
    void run() {
        // Buffer size of the storage buffer that will contain the rendered mandelbrot set.
        bufferSize = sizeof(Pixel) * WIDTH * HEIGHT;

        // Initialize vulkan:
        createInstance();
        findPhysicalDevice();
        createDevice();
        printf( " * before createBuffer()\n" ); fflush( stdout );
        createBuffer();
        printf( " * before createDescriptorSetLayout()\n" ); fflush( stdout );
        createDescriptorSetLayout();
        printf( " * before createDescriptorSet()\n" ); fflush( stdout );
        createDescriptorSet();
        printf( " * before createComputePipeline()\n" ); fflush( stdout );
        createComputePipeline();
        printf( " * before createCommandBuffer()\n" ); fflush( stdout );
        createCommandBuffer();

        printf( " * before runCommandBuffer()\n" ); fflush( stdout );
        // Finally, run the recorded command buffer.
        runCommandBuffer();

        // The former command rendered a mandelbrot set to a buffer.
        // Save that buffer as a png on disk.
        saveRenderedImage();

        // Clean up all vulkan resources.
        cleanup();
    }

    void saveRenderedImage() {
        void* mappedMemory = NULL;
        // Map the buffer memory, so that we can read from it on the CPU.
        vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
        Pixel* pmappedMemory = (Pixel *)mappedMemory;

        // Get the color data from the buffer, and cast it to bytes.
        // We save the data to a vector.
        std::vector<unsigned char> image;
        image.reserve(WIDTH * HEIGHT * 4);
    #if defined( MANDELBROT_MODE )
        float floatScaleFactor = 255.0f;
    #elif defined ( PATHTRACER_MODE )
        float floatScaleFactor = 1.0f;
    #endif
        for (int i = 0; i < WIDTH*HEIGHT; i += 1) {
            image.push_back( (unsigned char) ( floatScaleFactor * ( pmappedMemory[ i ].r ) ) );
            image.push_back( (unsigned char) ( floatScaleFactor * ( pmappedMemory[ i ].g ) ) );
            image.push_back( (unsigned char) ( floatScaleFactor * ( pmappedMemory[ i ].b ) ) );
            //image.push_back( (unsigned char) ( floatScaleFactor * ( pmappedMemory[ i ].a ) ) );
            image.push_back( 255u );
        }        
        // Done reading, so unmap.
        vkUnmapMemory(device, bufferMemory);

        // Now we save the acquired color data to a .png.
    #if defined( MANDELBROT_MODE ) // see Makefile        
        unsigned error = lodepng::encode("mandelbrot.png", image, WIDTH, HEIGHT);
    #elif defined( PATHTRACER_MODE )
        // due to pinhole cam the image is upside-down and mirrored - undo that!
        //uint32_t* pRGBA = ( uint32_t* ) image.data();
        uint32_t* pRGBA = ( uint32_t* ) ( &image[ 0 ] );
        // for ( int i = 0; i < WIDTH * HEIGHT; i++ ) {
        //     std::swap( pRGBA[ i ], pRGBA[ ( WIDTH * HEIGHT - 1 ) - i ] );
        // }
        for ( int y = 0; y < HEIGHT; y++ ) {
            for ( int x = 0; x < WIDTH / 2; x++ ) {
                uint32_t from = x + y * WIDTH;
                //uint32_t to   = ( WIDTH - 1 ) - x + y * WIDTH;
                //uint32_t to   = x + ( ( HEIGHT - 1 ) - y ) * WIDTH;
                uint32_t to   = ( WIDTH - 1 ) - x + ( ( HEIGHT - 1 ) - y ) * WIDTH;
                std::swap( pRGBA[ from ], pRGBA[ to ] );
            }
        }
        unsigned error = lodepng::encode("pathtracer.png", image, WIDTH, HEIGHT);
    #endif
        if (error) printf("encoder error %d: %s", error, lodepng_error_text(error));
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
        VkDebugReportFlagsEXT                       flags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     messageCode,
        const char*                                 pLayerPrefix,
        const char*                                 pMessage,
        void*                                       pUserData) {

        printf("Debug Report: %s: %s\n", pLayerPrefix, pMessage);

        return VK_FALSE;
     }

    void createInstance() {
        std::vector<const char *> enabledInstanceExtensions;

        /*
        By enabling validation layers, Vulkan will emit warnings if the API
        is used incorrectly. We shall enable the layer VK_LAYER_LUNARG_standard_validation,
        which is basically a collection of several useful validation layers.
        */
        if (enableValidationLayers) {
            /*
            We get all supported layers with vkEnumerateInstanceLayerProperties.
            */
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, NULL);

            std::vector<VkLayerProperties> layerProperties(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

            /*
            And then we simply check if VK_LAYER_LUNARG_standard_validation is among the supported layers.
            */
            bool foundLayer = false;
            for (VkLayerProperties prop : layerProperties) {
                //if (strcmp("VK_LAYER_LUNARG_standard_validation", prop.layerName) == 0) {
                if (strcmp("VK_LAYER_KHRONOS_validation", prop.layerName) == 0) {
                    foundLayer = true;
                    break;
                }
            }

            if (!foundLayer) {
                //throw std::runtime_error("Layer VK_LAYER_LUNARG_standard_validation not supported\n");
                throw std::runtime_error("Layer VK_LAYER_KHRONOS_validation not supported\n");
            }
            //enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation"); // Alright, we can use this layer.
            enabledLayers.push_back("VK_LAYER_KHRONOS_validation"); // Alright, we can use this layer.

            /*
            We need to enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
            in order to be able to print the warnings emitted by the validation layer.

            So again, we just check if the extension is among the supported extensions.
            */

            uint32_t extensionCount;

            vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
            std::vector<VkExtensionProperties> extensionProperties(extensionCount);
            vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties.data());

            bool foundExtension = false;
            for (VkExtensionProperties prop : extensionProperties) {
                if (strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, prop.extensionName) == 0) {
                    foundExtension = true;
                    break;
                }

            }

            if (!foundExtension) {
                throw std::runtime_error("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");
            }
            enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            /*
            > Debug Report: Validation: Validation Error: [ VUID-vkCreateDevice-ppEnabledExtensionNames-01387 ] Object 0: 
              VK_NULL_HANDLE, type = VK_OBJECT_TYPE_INSTANCE; | MessageID = 0x12537a2c | 
              Missing extension required by the device extension VK_KHR_portability_subset: 
              VK_KHR_get_physical_device_properties2. The Vulkan spec states: All required extensions for each extension in the 
              VkDeviceCreateInfo::ppEnabledExtensionNames list must also be present in that list 
              (https://vulkan.lunarg.com/doc/view/1.2.162.1/mac/1.2-extensions/vkspec.html#VUID-vkCreateDevice-ppEnabledExtensionNames-01387)
            */
            // Reading device properties and features for multiview requires VK_KHR_get_physical_device_properties2 to be enabled
     		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

            //???
            //enabledExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            //enabledExtensions.push_back( "VK_KHR_portability_subset" );
        }

        /*
        Next, we actually create the instance.

        */

        /*
        Contains application info. This is actually not that important.
        The only real important field is apiVersion.
        */
        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Hello world app";
        applicationInfo.applicationVersion = 0;
        applicationInfo.pEngineName = "awesomeengine";
        applicationInfo.engineVersion = 0;
        applicationInfo.apiVersion = VK_API_VERSION_1_0;;

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.flags = 0;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;

        // Give our desired layers and extensions to vulkan.
        instanceCreateInfo.enabledLayerCount = enabledLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
        instanceCreateInfo.enabledExtensionCount = enabledInstanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

        /*
        Actually create the instance.
        Having created the instance, we can actually start using vulkan.
        */
        VK_CHECK_RESULT(vkCreateInstance(
            &instanceCreateInfo,
            NULL,
            &instance));

        /*
        Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
        layer are actually printed.
        */
        if (enableValidationLayers) {
            VkDebugReportCallbackCreateInfoEXT createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            createInfo.pfnCallback = &debugReportCallbackFn;

            // We have to explicitly load this function.
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
            if (vkCreateDebugReportCallbackEXT == nullptr) {
                throw std::runtime_error("Could not load vkCreateDebugReportCallbackEXT");
            }

            // Create and register callback.
            VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, &debugReportCallback));
        }

    }

    void findPhysicalDevice() {
        /*
        In this function, we find a physical device that can be used with Vulkan.
        */

        /*
        So, first we will list all physical devices on the system with vkEnumeratePhysicalDevices .
        */
        uint32_t deviceCount;
        vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
        if (deviceCount == 0) {
            throw std::runtime_error("could not find a device with vulkan support");
        }

        printf( "found %u Vulkan device%s\n", deviceCount, ( deviceCount != 1 ) ? "s" : "" );

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        printf( "found the following instance extensions:\n" );
        // Get extensions supported by the instance and store for later use
        uint32_t instanceExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
        if ( instanceExtensionCount > 0 )
        {
            std::vector<VkExtensionProperties> instanceExtensions( instanceExtensionCount );
            if (vkEnumerateInstanceExtensionProperties( nullptr, &instanceExtensionCount, &instanceExtensions.front()) == VK_SUCCESS )
            {
                for ( VkExtensionProperties instanceExtension : instanceExtensions )
                {
                    //supportedInstanceExtensions.push_back( instanceExtension.extensionName );
                    printf( "   %s \n", instanceExtension.extensionName );
                }
            }
        }
        printf( "\n" );

    #if 1
        for ( VkPhysicalDevice& pyhsicalDevice : devices ) {
            static uint32_t deviceNum = 0;

            // VkPhysicalDeviceProperties properties;
            // vkGetPhysicalDeviceProperties( physicalDevice, &properties );
            
            //printf( "device %u: %s\n", deviceNum, properties.deviceName );
            

            // !!! CRASHES !!!
            //uint32_t deviceExtensionCount;
            //vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);

            // std::vector<VkExtensionProperties> deviceExtensions ( deviceExtensionCount );
            // vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data() );
            // for ( auto& deviceExtension : deviceExtensions )
            // {
            //     // if ( !strcmp( deviceExtension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) )
            //     // {
            //     //     extensionPresent = true;
            //     // }

            //     printf( "%s ", deviceExtension.extensionName );
            // }
            // printf( "\n" );

            deviceNum++;
        }
    #endif  
        /*
        Next, we choose a device that can be used for our purposes.

        With VkPhysicalDeviceFeatures(), we can retrieve a fine-grained list of physical features supported by the device.
        However, in this demo, we are simply launching a simple compute shader, and there are no
        special physical features demanded for this task.

        With VkPhysicalDeviceProperties(), we can obtain a list of physical device properties. Most importantly,
        we obtain a list of physical device limitations. For this application, we launch a compute shader,
        and the maximum size of the workgroups and total number of compute shader invocations is limited by the physical device,
        and we should ensure that the limitations named maxComputeWorkGroupCount, maxComputeWorkGroupInvocations and
        maxComputeWorkGroupSize are not exceeded by our application.  Moreover, we are using a storage buffer in the compute shader,
        and we should ensure that it is not larger than the device can handle, by checking the limitation maxStorageBufferRange.

        However, in our application, the workgroup size and total number of shader invocations is relatively small, and the storage buffer is
        not that large, and thus a vast majority of devices will be able to handle it. This can be verified by looking at some devices at_
        http://vulkan.gpuinfo.org/

        Therefore, to keep things simple and clean, we will not perform any such checks here, and just pick the first physical
        device in the list. But in a real and serious application, those limitations should certainly be taken into account.

        */
        
        // for (VkPhysicalDevice device : devices) {
        //     if (true) { // As above stated, we do no feature checks, so just accept.
        //         physicalDevice = device;
        //         break;
        //     }
        // }
        
        physicalDevice = devices[ 0 ];
        //physicalDevice = devices[ 1 ]; // chose AMD GPU

        // for ( const VkPhysicalDevice& device : devices ) {
        //     VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
        //     //physicalDeviceFeatures.shaderFloat64 = VkBool32{ true };
        //     vkGetPhysicalDeviceFeatures( device, &physicalDeviceFeatures );
        //     printf( "shaderInt64 is%s supported\n", ( physicalDeviceFeatures.shaderInt64 == VK_TRUE ) ? "" : " not" );        
        // }

        // for ( const VkPhysicalDevice& device : devices ) {
        //     VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
        //     physicalDeviceFeatures.shaderInt64 = VkBool32{ true };
        // }
        
        // VkPhysicalDeviceFeatures* pPhysicalDeviceFeatures = nullptr;
        // vkGetPhysicalDeviceFeatures( physicalDevice, pPhysicalDeviceFeatures );
        // pPhysicalDeviceFeatures->shaderInt64 = true;

        VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
        vkGetPhysicalDeviceFeatures( physicalDevice, &physicalDeviceFeatures );
        printf( "shaderInt64 is%s supported\n", ( physicalDeviceFeatures.shaderInt64 == VK_TRUE ) ? "" : " not" );        
                
    }

    // Returns the index of a queue family that supports compute operations.
    uint32_t getComputeQueueFamilyIndex() {
        uint32_t queueFamilyCount;

        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

        // Retrieve all queue families.
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        // Now find a family that supports compute.
        uint32_t i = 0;
        for (; i < queueFamilies.size(); ++i) {
            VkQueueFamilyProperties props = queueFamilies[i];

            if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                // found a queue with compute. We're done!
                break;
            }
        }

        if (i == queueFamilies.size()) {
            throw std::runtime_error("could not find a queue family that supports operations");
        }

        return i;
    }

    void createDevice() {
        /*
        We create the logical device in this function.
        */

        /*
        When creating the device, we also specify what queues it has.
        */
        // typedef struct VkDeviceQueueCreateInfo {
        //     VkStructureType             sType;
        //     const void*                 pNext;
        //     VkDeviceQueueCreateFlags    flags;
        //     uint32_t                    queueFamilyIndex;
        //     uint32_t                    queueCount;
        //     const float*                pQueuePriorities;
        // } VkDeviceQueueCreateInfo;

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueFamilyIndex = getComputeQueueFamilyIndex(); // find queue family with compute capability.
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1; // create one queue in this family. We don't need more.
        float queuePriorities = 1.0;  // we only have one queue, so this is not that imporant.
        queueCreateInfo.pQueuePriorities = &queuePriorities;

        /*
        Now we create the logical device. The logical device allows us to interact with the physical
        device.
        */
        VkDeviceCreateInfo deviceCreateInfo = {};

        // Specify any desired device features here. We do not need any for this application, though.
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
        // Shader requires VkPhysicalDeviceFeatures::shaderFloat64 but is not enabled on the device
        // deviceFeatures.shaderFloat64 = VK_TRUE; // not supported on macOS where Vulkan is run on Metal through MoltenVk, since there is no double in Metal!

        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.enabledLayerCount = enabledLayers.size();  // need to specify validation layers here as well.
        deviceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo; // when creating the logical device, we also specify what queues it has.
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        //???
        //const char *const extensions[] = { "VK_KHR_portability_subset" };
        //deviceCreateInfo.ppEnabledExtensionNames = extensions;
        //deviceCreateInfo.enabledExtensionCount = 1;

        /*
        > Debug Report: Validation: Validation Error: [ VUID-VkDeviceCreateInfo-pProperties-04451 ] Object 0: 
          handle = 0x7fa10fd30cb0, type = VK_OBJECT_TYPE_PHYSICAL_DEVICE; | MessageID = 0x3a3b6ca0 | 
          vkCreateDevice: VK_KHR_portability_subset must be enabled because physical device VkPhysicalDevice 0x7fa10fd30cb0[] supports it 
          The Vulkan spec states: If the [VK_KHR_portability_subset] extension is included in pProperties of 
          vkEnumerateDeviceExtensionProperties, ppEnabledExtensions must include "VK_KHR_portability_subset". 
          (https://vulkan.lunarg.com/doc/view/1.2.162.1/mac/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451)        
        */
        std::vector<const char*> deviceExtensions;
        deviceExtensions.push_back( "VK_KHR_portability_subset" );
        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		// //if (pNextChain) {
		// 	physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		// 	physicalDeviceFeatures2.features = enabledFeatures;
		// 	physicalDeviceFeatures2.pNext = pNextChain;
		// 	deviceCreateInfo.pEnabledFeatures = nullptr;
		// 	deviceCreateInfo.pNext = &physicalDeviceFeatures2;
		// //}


        VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)); // create logical device.

        // Get a handle to the only member of the queue family.
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    }

    // find memory type with desired properties.
    uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memoryProperties;

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        /*
        How does this search work?
        See the documentation of VkPhysicalDeviceMemoryProperties for a detailed description.
        */
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if ((memoryTypeBits & (1 << i)) &&
                ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
                return i;
        }
        return -1;
    }

    void createBuffer() {
        /*
        We will now create a buffer. We will render the mandelbrot set into this buffer
        in a computer shade later.
        */

       printf( "buffer create!\n" ); fflush( stdout );

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize; // buffer size in bytes.
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.

        VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, NULL, &buffer)); // create buffer.

        /*
        But the buffer doesn't allocate memory for itself, so we must do that manually.
        */

        /*
        First, we find the memory requirements for the buffer.
        */
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

        /*
        Now use obtained memory requirements info to allocate the memory for the buffer.
        */
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size; // specify required memory.
        /*
        There are several types of memory that can be allocated, and we must choose a memory type that:

        1) Satisfies the memory requirements(memoryRequirements.memoryTypeBits).
        2) Satifies our own usage requirements. We want to be able to read the buffer memory from the GPU to the CPU
           with vkMapMemory, so we set VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
        Also, by setting VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memory written by the device(GPU) will be easily
        visible to the host(CPU), without having to call any extra flushing commands. So mainly for convenience, we set
        this flag.
        */
        allocateInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &bufferMemory)); // allocate memory on device.

        // Now associate that allocated memory with the buffer. With that, the buffer is backed by actual memory.
        VK_CHECK_RESULT(vkBindBufferMemory(device, buffer, bufferMemory, 0));


#if defined( PATHTRACER_MODE )
        printf( "planebuffer create!\n" ); fflush( stdout );
        VkBufferCreateInfo planeBufferCreateInfo = {};
        planeBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        planeBufferCreateInfo.size = planeBufferSize; // buffer size in bytes.
        planeBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.

        VK_CHECK_RESULT(vkCreateBuffer(device, &planeBufferCreateInfo, NULL, &planeBuffer)); // create buffer.

        VkMemoryRequirements planeBufferMemoryRequirements;
        vkGetBufferMemoryRequirements(device, planeBuffer, &planeBufferMemoryRequirements);

        VkMemoryAllocateInfo planeBufferAllocateInfo = {};
        planeBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        planeBufferAllocateInfo.allocationSize = planeBufferMemoryRequirements.size; // specify required memory.

        planeBufferAllocateInfo.memoryTypeIndex = findMemoryType(
            planeBufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &planeBufferAllocateInfo, NULL, &planeBufferMemory)); // allocate memory on device.

        // Now associate that allocated memory with the buffer. With that, the buffer is backed by actual memory.
        VK_CHECK_RESULT(vkBindBufferMemory(device, planeBuffer, planeBufferMemory, 0));

        // upload planes[] data from host to device
        {
            void* vpMappedMemory = NULL;
            // Map the buffer memory, so that we can read from it on the CPU.
            vkMapMemory(device, planeBufferMemory, 0, planeBufferSize, 0, &vpMappedMemory);
            float* pMappedMemory = (float *)vpMappedMemory;
            memcpy( pMappedMemory, planes, sizeof( planes ) );
            // Done writing, so unmap.
            vkUnmapMemory(device, planeBufferMemory);
        }



        printf( "spherebuffer create!\n" ); fflush( stdout );
        VkBufferCreateInfo sphereBufferCreateInfo = {};
        sphereBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sphereBufferCreateInfo.size = sphereBufferSize; // buffer size in bytes.
        sphereBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.

        VK_CHECK_RESULT(vkCreateBuffer(device, &sphereBufferCreateInfo, NULL, &sphereBuffer)); // create buffer.

        VkMemoryRequirements sphereBufferMemoryRequirements;
        vkGetBufferMemoryRequirements(device, sphereBuffer, &sphereBufferMemoryRequirements);

        VkMemoryAllocateInfo sphereBufferAllocateInfo = {};
        sphereBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        sphereBufferAllocateInfo.allocationSize = sphereBufferMemoryRequirements.size; // specify required memory.

        sphereBufferAllocateInfo.memoryTypeIndex = findMemoryType(
            sphereBufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &sphereBufferAllocateInfo, NULL, &sphereBufferMemory)); // allocate memory on device.

        // Now associate that allocated memory with the buffer. With that, the buffer is backed by actual memory.
        VK_CHECK_RESULT(vkBindBufferMemory(device, sphereBuffer, sphereBufferMemory, 0));

        // upload spheres[] data from host to device
        {
            void* vpMappedMemory = NULL;
            // Map the buffer memory, so that we can read from it on the CPU.
            vkMapMemory(device, sphereBufferMemory, 0, sphereBufferSize, 0, &vpMappedMemory);
            float* pMappedMemory = (float *)vpMappedMemory;
            memcpy( pMappedMemory, spheres, sizeof( spheres ) );
            // Done writing, so unmap.
            vkUnmapMemory(device, sphereBufferMemory);
        }
#endif

        printf( "leaving createBuffer!\n" ); fflush( stdout );
    }

    void createDescriptorSetLayout() {
        /*
        Here we specify a descriptor set layout. This allows us to bind our descriptors to
        resources in the shader.
        */

    #if defined( MANDELBROT_MODE )
        /*
        Here we specify a binding of type VK_DESCRIPTOR_TYPE_STORAGE_BUFFER to the binding point
        0. This binds to

          layout(std140, binding = 0) buffer buf

        in the compute shader.
        */
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
        descriptorSetLayoutBinding.binding = 0; // binding = 0
        descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1; // only a single binding in this descriptor set layout.
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        // Create the descriptor set layout.
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout));

    #elif defined( PATHTRACER_MODE )
        VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3] = {
            {
                0,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0
            },
            { // planes
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0
            },
            { // spheres
                2,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0
            },
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            0,
            0,
            3,
            descriptorSetLayoutBindings
        };

        //GLOBAL VAR!!!! VkDescriptorSetLayout descriptorSetLayout;
        BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout));
    #endif
    }

    void createDescriptorSet() {
        /*
        So we will allocate a descriptor set here.
        But we need to first create a descriptor pool to do that.
        */
    #if defined( MANDELBROT_MODE ) 
        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
        /*
        Our descriptor pool can only allocate a single storage buffer.
        */
        VkDescriptorPoolSize descriptorPoolSize = {};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorPoolSize.descriptorCount = 1;
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    #elif defined( PATHTRACER_MODE ) 
        //create a descriptor pool that will hold 3 storage buffers // image, planes, spheres
        VkDescriptorPoolSize descriptorPoolSize = {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            3
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            0,
            0,
            1,
            1,
            &descriptorPoolSize
        };
    #endif
        
        printf( "before vkCreateDescriptorPool()\n" ); fflush( stdout );
        // create descriptor pool.
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool));

        printf( "after vkCreateDescriptorPool()\n" ); fflush( stdout );

        /*
        With the pool allocated, we can now allocate the descriptor set.
        */
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool; // pool to allocate from.
        descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;


        printf( "before vkAllocateDescriptorSets()\n" ); fflush( stdout );
        // allocate descriptor set.
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
        printf( "after vkAllocateDescriptorSets()\n" ); fflush( stdout );

        /*
        Next, we need to connect our actual storage buffer with the descrptor.
        We use vkUpdateDescriptorSets() to update the descriptor set.
        */

    #if defined( MANDELBROT_MODE )
        // Specify the buffer to bind to the descriptor.
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = buffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = bufferSize; // VK_WHOLE_SIZE

        VkWriteDescriptorSet writeDescriptorSet = {};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet; // write to this descriptor set.
        writeDescriptorSet.dstBinding = 0; // write to the first, and only binding.
        writeDescriptorSet.descriptorCount = 1; // update a single descriptor.
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
        writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;

        printf( "before vkUpdateDescriptorSets MANDELBROT_MODE\n" ); fflush( stdout );

        // perform the update of the descriptor set.
        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

    #elif defined( PATHTRACER_MODE )

        printf( "before binding buffers to descriptors\n" ); fflush( stdout );
        // Specify the buffer to bind to the descriptor.
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = buffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = VK_WHOLE_SIZE;//bufferSize;

        VkDescriptorBufferInfo descriptorPlaneBufferInfo = {};
        descriptorPlaneBufferInfo.buffer = planeBuffer;
        descriptorPlaneBufferInfo.offset = 0;
        descriptorPlaneBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo descriptorSphereBufferInfo = {};
        descriptorSphereBufferInfo.buffer = sphereBuffer;
        descriptorSphereBufferInfo.offset = 0;
        descriptorSphereBufferInfo.range = VK_WHOLE_SIZE;//sphereBufferSize;

        VkWriteDescriptorSet writeDescriptorSet[3] = {
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                0,
                descriptorSet,
                0, // dstBinding - image
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &descriptorBufferInfo,
                0
            },
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                0,
                descriptorSet,
                1, // dstBinding - planes
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &descriptorPlaneBufferInfo,
                0
            },
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                0,
                descriptorSet,
                2, // dstBinding - spheres
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &descriptorSphereBufferInfo,
                0
            }
        };

        printf( "before vkUpdateDescriptorSets PATHTRACER_MODE\n" ); fflush( stdout );

        // perform the update of the descriptor set.
        vkUpdateDescriptorSets(device, 3, writeDescriptorSet, 0, 0); // 3 buffers: image, planes, spheres
    #endif

        printf( "after vkUpdateDescriptorSets\n" ); fflush( stdout );
    }

    // Read file into array of bytes, and cast to uint32_t*, then return.
    // The data has been padded, so that it fits into an array uint32_t.
    uint32_t* readFile(uint32_t& length, const char* filename) {

        FILE* fp = fopen(filename, "rb");
        if (fp == NULL) {
            printf("Could not find or open file: %s\n", filename);
        }

        // get file size.
        fseek(fp, 0, SEEK_END);
        long filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        long filesizepadded = long(ceil(filesize / 4.0)) * 4;

        // read file contents.
        char *str = new char[filesizepadded];
        fread(str, filesize, sizeof(char), fp);
        fclose(fp);

        // data padding.
        for (int i = filesize; i < filesizepadded; i++) {
            str[i] = 0;
        }

        length = filesizepadded;
        return (uint32_t *)str;
    }

    void createComputePipeline() {
        /*
        We create a compute pipeline here.
        */

        /*
        Create a shader module. A shader module basically just encapsulates some shader code.
        */
        uint32_t filelength;
    #if defined ( MANDELBROT_MODE )        
        // the code in mandelbrot.spv can be created by running the following command in the 'shaders' folder:
        // $(VULKAN_SDK)bin/glslangValidator -V mandelbrot.comp -o mandelbrot.spv
        // NOTE: if the instructions in mac-vulkan-setup-guide.md were followed, $(VULKAN_SDK)bin/ is
        //       actually part of the path, so you can just type 'glslangValidator -V mandelbrot.comp -o mandelbrot.spv'
        uint32_t* code = readFile(filelength, "shaders/mandelbrot.spv");
    #elif defined( PATHTRACER_MODE )
        uint32_t* code = readFile(filelength, "shaders/pathTracer.spv");
    #endif
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pCode = code;
        createInfo.codeSize = filelength;

        VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, NULL, &computeShaderModule));
        delete[] code;

        /*
        Now let us actually create the compute pipeline.
        A compute pipeline is very simple compared to a graphics pipeline.
        It only consists of a single stage with a compute shader.

        So first we specify the compute shader stage, and it's entry point(main).
        */
        VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageCreateInfo.module = computeShaderModule;
        shaderStageCreateInfo.pName = "main";

        // [husky]: Define the push constant range used by the pipeline layout
		// Note that the spec only requires a minimum of 128 bytes, so for passing larger blocks of data you'd use UBOs or SSBOs
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		pushConstantRange.offset = 0;
        #if defined( MANDELBROT_MODE )
		    pushConstantRange.size = 4 * sizeof( float );
        #elif defined( PATHTRACER_MODE )
            pushConstantRange.size = sizeof( pushConst_t );
        #endif

        /*
        The pipeline layout allows the pipeline to access descriptor sets.
        So we just specify the descriptor set layout we created earlier.
        */
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        // [husky]
        pipelineLayoutCreateInfo.pushConstantRangeCount  = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;        

        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout));

        VkComputePipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stage = shaderStageCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;

        /*
        Now, we finally create the compute pipeline.
        */
        VK_CHECK_RESULT(vkCreateComputePipelines(
            device, VK_NULL_HANDLE,
            1, &pipelineCreateInfo,
            NULL, &pipeline));
    }

    void createCommandBuffer() {
        /*
        We are getting closer to the end. In order to send commands to the device(GPU),
        we must first record commands into a command buffer.
        To allocate a command buffer, we must first create a command pool. So let us do that.
        */
        VkCommandPoolCreateInfo commandPoolCreateInfo = {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = 0;
        // the queue family of this command pool. All command buffers allocated from this command pool,
        // must be submitted to queues of this family ONLY.
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool));

        /*
        Now allocate a command buffer from the command pool.
        */
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = commandPool; // specify the command pool to allocate from.
        // if the command buffer is primary, it can be directly submitted to queues.
        // A secondary buffer has to be called from some primary command buffer, and cannot be directly
        // submitted to a queue. To keep things simple, we use a primary command buffer.
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1; // allocate a single command buffer.
        VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer)); // allocate command buffer.

        /*
        Now we shall start recording commands into the newly allocated command buffer.
        */
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // the buffer is only submitted and used once in this application.
        VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo)); // start recording commands.

        /*
        We need to bind a pipeline, AND a descriptor set before we dispatch.

        The validation layer will NOT give warnings if you forget these, so be very careful not to forget them.
        */
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

    #if defined( MANDELBROT_MODE ) // see Makefile
        float kColor[4]{ 0.1f, 0.7f, 0.6f, 0.0f };
        //float kColor[4]{ 0.9f, 0.1f, 0.3f, 0.0f };
        vkCmdPushConstants( commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( float ) * 4, kColor );

        /*
        Calling vkCmdDispatch basically starts the compute pipeline, and executes the compute shader.
        The number of workgroups is specified in the arguments.
        If you are already familiar with compute shaders from OpenGL, this should be nothing new to you.
        */
        vkCmdDispatch(commandBuffer, (uint32_t)ceil(WIDTH / float(WORKGROUP_SIZE)), (uint32_t)ceil(HEIGHT / float(WORKGROUP_SIZE)), 1);

    #elif defined( PATHTRACER_MODE )
        printf( "\n   ### entering spp loop ###\n\n" ); fflush( stdout );
        for ( int32_t sampNum = 0; sampNum < spp; sampNum++ ) {

            pushConst.samps[ 0 ] = sampNum;

            // printf( "pushConst.imgdim[0] = %u, pushConst.imgdim[1] = %u, pushConst.samps[0] = %u, pushConst.samps[1] = %u\n", 
            //     pushConst.imgdim[0], pushConst.imgdim[1],
            //     pushConst.samps[0], pushConst.samps[1] ); fflush( stdout );

            
            vkCmdPushConstants( commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConst_t ), &pushConst );

            /*
            Calling vkCmdDispatch basically starts the compute pipeline, and executes the compute shader.
            The number of workgroups is specified in the arguments.
            If you are already familiar with compute shaders from OpenGL, this should be nothing new to you.
            */
            vkCmdDispatch(commandBuffer, (uint32_t)ceil(WIDTH / float(WORKGROUP_SIZE)), (uint32_t)ceil(HEIGHT / float(WORKGROUP_SIZE)), 1);
        }
        printf( "\n   ### leaving spp loop ###\n\n" ); fflush( stdout );
    #endif


        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer)); // end recording commands.
    }

    void runCommandBuffer() {
        /*
        Now we shall finally submit the recorded command buffer to a queue.
        */

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1; // submit a single command buffer
        submitInfo.pCommandBuffers = &commandBuffer; // the command buffer to submit.

        /*
          We create a fence.
        */
        VkFence fence;
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = 0;
        VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, NULL, &fence));

        /*
        We submit the command buffer on the queue, at the same time giving a fence.
        */
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
        /*
        The command will not have finished executing until the fence is signalled.
        So we wait here.
        We will directly after this read our buffer from the GPU,
        and we will not be sure that the command has finished executing unless we wait for the fence.
        Hence, we use a fence here.
        */
        VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000));

        vkDestroyFence(device, fence, NULL);
    }

    void cleanup() {
        /*
        Clean up all Vulkan Resources.
        */

        if (enableValidationLayers) {
            // destroy callback.
            auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
            if (func == nullptr) {
                throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
            }
            func(instance, debugReportCallback, NULL);
        }

        vkFreeMemory(device, bufferMemory, NULL);
        vkDestroyBuffer(device, buffer, NULL);
    #if defined( PATHTRACER_MODE )
        vkFreeMemory(device, planeBufferMemory, NULL);
        vkDestroyBuffer(device, planeBuffer, NULL);
        vkFreeMemory(device, sphereBufferMemory, NULL);
        vkDestroyBuffer(device, sphereBuffer, NULL);
    #endif
        vkDestroyShaderModule(device, computeShaderModule, NULL);
        vkDestroyDescriptorPool(device, descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
        vkDestroyPipelineLayout(device, pipelineLayout, NULL);
        vkDestroyPipeline(device, pipeline, NULL);
        vkDestroyCommandPool(device, commandPool, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
    }
};

int main( int argc, char* argv[] ) {

#if defined( PATHTRACER_MODE )
    // spp = argc>1 ? atoi(argv[1]) : 100;    // samples per pixel 
    // resy = argc>2 ? atoi(argv[2]) : 600;    // vertical pixel resolution
    // resx = resy*3/2;	                    // horiziontal pixel resolution
#endif

    ComputeApplication app;

    try {
        app.run();
    }
    catch (const std::runtime_error& e) {
        printf("%s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

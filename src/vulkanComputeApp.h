#ifndef _VULKANCOMPUTEAPP_H_
#define _VULKANCOMPUTEAPP_H_

#include <vulkan/vulkan.h>
#include <vector>

#include <math.h>

#include <assert.h>

#define BAIL_ON_BAD_RESULT(result) \
  if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }


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


// The application launches a compute shader that renders the mandelbrot set / path tracer,
// by rendering it into a storage buffer.
// The storage buffer is then read from the GPU, and saved as .png.
struct VulkanComputeApp {
    
    virtual ~VulkanComputeApp() {
        cleanupVulkanResources();
    }

    void init();
    virtual void preRun() {}
    virtual void run();

    void createInstance();
    
    void findPhysicalDevice(); // In this function, we find a physical device that can be used with Vulkan.

    // Returns the index of a queue family that supports compute operations.
    uint32_t getComputeQueueFamilyIndex();
    
    void createDevice();
    
    // find memory type with desired properties.
    uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
    
    void createBuffer( const uint32_t bufferSize );
    
    void createDescriptorSetLayout();
    virtual void createDescriptorSet() {}
    void createShader( const char* pFilename, VkShaderModule& computeShaderModule );
    
    virtual void createComputePipeline() {}
    
    void createCommandBufferPre();
    virtual void createCommandBuffer() {}
    void createCommandBufferPost();

    void runCommandBuffer();

    //void getRenderedImage( std::vector<uint8_t> &image, const uint32_t bufferSize, const uint32_t resx, const uint32_t resy, float floatScaleFactor );
    virtual void saveRenderedImage( const char* png_filename ) = 0;

    void cleanupVulkanResources();
    
protected:

    // In order to use Vulkan, you must create an instance.
    VkInstance instance;

    VkDebugReportCallbackEXT debugReportCallback;

    // The physical device is some device on the system that supports usage of Vulkan.
    // Often, it is simply a graphics card that supports Vulkan.
    VkPhysicalDevice physicalDevice;

    // Then we have the logical device VkDevice, which basically allows
    // us to interact with the physical device.
    VkDevice device;

    // The pipeline specifies the pipeline that all graphics and compute commands pass though in Vulkan.
    // We will be creating a simple compute pipeline in this application.
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule computeShaderModule;

    // The command buffer is used to record commands, that will be submitted to a queue.
    // To allocate such command buffers, we use a command pool.
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    // Descriptors represent resources in shaders. They allow us to use things like
    // uniform buffers, storage buffers and images in GLSL.
    // A single descriptor represents a single resource, and several descriptors are organized
    // into descriptor sets, which are basically just collections of descriptors.
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // The mandelbrot set / path-traced scene will be rendered to this buffer.
    // The memory that backs the buffer is bufferMemory.
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;

    std::vector<const char *> enabledLayers;

    // In order to execute commands on a device(GPU), the commands must be submitted
    // to a queue. The commands are stored in a command buffer, and this command buffer
    // is given to the queue.

    // There will be different kinds of queues on the device. Not all queues support
    // graphics operations, for instance. For this application, we at least want a queue
    // that supports compute operations.
    VkQueue queue; // a queue supporting compute operations.

    // Groups of queues that have the same capabilities(for instance, they all supports graphics and computer operations),
    // are grouped into queue families.
    // When submitting a command buffer, you must specify to which queue in the family you are submitting to.
    // This variable keeps track of the index of that queue in its family.
    uint32_t queueFamilyIndex;
    
};

#endif // _VULKANCOMPUTEAPP_H_


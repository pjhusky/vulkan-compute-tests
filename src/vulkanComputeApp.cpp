#include "vulkanComputeApp.h"

#include <stdio.h>
#include <string.h>

#include <stdexcept>

namespace {

    #ifdef NDEBUG
    static const bool enableValidationLayers = false;
    #else
    static const bool enableValidationLayers = true;
    #endif

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


    // Read file into array of bytes, and cast to uint32_t*, then return.
    // The data has been padded, so that it fits into an array uint32_t.
    static int32_t readSpvFile(std::vector<uint32_t>& content, uint32_t& length, const char* filename) {

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
        content.resize( filesizepadded / 4 );
        
        char *str = reinterpret_cast<char*>( content.data() );
        fread(str, filesize, sizeof(char), fp);
        fclose(fp);

        printf( "read and closed file '%s'\n", filename );

        // data padding.
        for (int i = filesize; i < filesizepadded; i++) {
            str[i] = 0;
        }
        
        printf( "read and closed file '%s' - finished data padding\n", filename );

        length = filesizepadded;
        //return (uint32_t *)str;
        return 0;
    }

} // namespace


void VulkanComputeApp::findPhysicalDevice() { // In this function, we find a physical device that can be used with Vulkan.

    // So, first we will list all physical devices on the system with vkEnumeratePhysicalDevices .
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
    physicalDeviceFeatures.shaderFloat64 = VK_FALSE;
    physicalDeviceFeatures.shaderInt64 = VK_FALSE;
    vkGetPhysicalDeviceFeatures( physicalDevice, &physicalDeviceFeatures );
    printf( "shaderInt64 is%s supported\n", ( physicalDeviceFeatures.shaderInt64 == VK_TRUE ) ? "" : " not" );        
            
}


void VulkanComputeApp::createInstance() {
    std::vector<const char *> enabledInstanceExtensions;

    // By enabling validation layers, Vulkan will emit warnings if the API
    // is used incorrectly. We shall enable the layer VK_LAYER_LUNARG_standard_validation,
    // which is basically a collection of several useful validation layers.
    if (enableValidationLayers) {

        // We get all supported layers with vkEnumerateInstanceLayerProperties.
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, NULL);

        std::vector<VkLayerProperties> layerProperties(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

        // And then we simply check if VK_LAYER_LUNARG_standard_validation is among the supported layers.
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

        // We need to enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        // in order to be able to print the warnings emitted by the validation layer.
        // So again, we just check if the extension is among the supported extensions.
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

    // Next, we actually create the instance.

    // Contains application info. This is actually not that important.
    // The only real important field is apiVersion.
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Vulkan Compute Test App";
    applicationInfo.applicationVersion = 0;
    applicationInfo.pEngineName = "Vulkan Compute Test";
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

    // Actually create the instance.
    // Having created the instance, we can actually start using vulkan.
    VK_CHECK_RESULT(vkCreateInstance(
        &instanceCreateInfo,
        NULL,
        &instance));

    // Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
    // layer are actually printed.
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

uint32_t VulkanComputeApp::findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    // How does this search work?
    // See the documentation of VkPhysicalDeviceMemoryProperties for a detailed description.
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((memoryTypeBits & (1 << i)) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
            return i;
    }
    return -1;
}

uint32_t VulkanComputeApp::getComputeQueueFamilyIndex() {
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

void VulkanComputeApp::createDevice() {
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

    // Now we create the logical device. The logical device allows us to interact with the physical
    // device.
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
    
    //!!! deviceExtensions.push_back( "VK_KHR_portability_subset" );
        
    
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
    // //if (pNextChain) {
        // physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        // // physicalDeviceFeatures2.features = enabledFeatures;
        // // physicalDeviceFeatures2.pNext = pNextChain;
        // // deviceCreateInfo.pEnabledFeatures = nullptr;
        // deviceCreateInfo.pNext = &physicalDeviceFeatures2;
    // //}

    // VkPhysicalDeviceVulkan11Features vk11features;
    // //if (pNextChain) 
    // {
        // vk11features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        // //vk11features.features = enabledFeatures;
        // // vk11features.pNext = pNextChain;
        // //deviceCreateInfo.pEnabledFeatures = nullptr;
        // deviceCreateInfo.pNext = &vk11features;
    // }

    VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)); // create logical device.

    // Get a handle to the only member of the queue family.
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
}


void VulkanComputeApp::init() {
    // Initialize vulkan:
    createInstance();
    findPhysicalDevice();
    createDevice();
    printf( "created device\n" );
}

void VulkanComputeApp::run() {    
    printf( " * before createDescriptorSetLayout()\n" ); fflush( stdout );
    createDescriptorSetLayout();
    printf( " * before createDescriptorSet()\n" ); fflush( stdout );
    createDescriptorSet();
    printf( " * before createComputePipeline()\n" ); fflush( stdout );
    createComputePipeline();
    printf( " * before createCommandBuffer()\n" ); fflush( stdout );
    createCommandBufferPre();
    createCommandBuffer();
    createCommandBufferPost();

    printf( " * before runCommandBuffer()\n" ); fflush( stdout );
    // Finally, run the recorded command buffer.
    runCommandBuffer();
}

void VulkanComputeApp::createShader( const char* pFilename, VkShaderModule& computeShaderModule ) {
    printf( "in VulkanComputeApp::createShader!\n" );
    // Create a shader module. A shader module basically just encapsulates some shader code.
    uint32_t filelength;
    std::vector<uint32_t> codeV; 
    readSpvFile(codeV, filelength, pFilename);
    uint32_t* pCode = codeV.data();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = pCode;
    createInfo.codeSize = filelength;

    printf("now calling vkCreateShaderModule\n");

    VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, NULL, &computeShaderModule));
    
    printf( "leaving VulkanComputeApp::createShader!\n" );
}


void VulkanComputeApp::createBuffer( const uint32_t bufferSize ) {
    // We will now create a buffer. We will render the mandelbrot set into this buffer
    // in a computer shade later.

    printf( "buffer create!\n" ); fflush( stdout );

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize; // buffer size in bytes.
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, NULL, &buffer)); // create buffer.

    // But the buffer doesn't allocate memory for itself, so we must do that manually.

    // First, we find the memory requirements for the buffer.
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    // Now use obtained memory requirements info to allocate the memory for the buffer.
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


    printf( "leaving createBuffer!\n" ); fflush( stdout );
}

void VulkanComputeApp::createDescriptorSetLayout() {
    
    // Here we specify a descriptor set layout. This allows us to bind our descriptors to
    // resources in the shader.
    

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



void VulkanComputeApp::createCommandBufferPre() {

    // We are getting closer to the end. In order to send commands to the device(GPU),
    // we must first record commands into a command buffer.
    // To allocate a command buffer, we must first create a command pool. So let us do that.

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = 0;
    // the queue family of this command pool. All command buffers allocated from this command pool,
    // must be submitted to queues of this family ONLY.
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool));

    // Now allocate a command buffer from the command pool.
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool; // specify the command pool to allocate from.
    // if the command buffer is primary, it can be directly submitted to queues.
    // A secondary buffer has to be called from some primary command buffer, and cannot be directly
    // submitted to a queue. To keep things simple, we use a primary command buffer.
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1; // allocate a single command buffer.
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer)); // allocate command buffer.

    // Now we shall start recording commands into the newly allocated command buffer.
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // the buffer is only submitted and used once in this application.
    VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo)); // start recording commands.

    // We need to bind a pipeline, AND a descriptor set before we dispatch.
    // The validation layer will NOT give warnings if you forget these, so be very careful not to forget them.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
}

void VulkanComputeApp::createCommandBufferPost() {
    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer)); // end recording commands.
}

void VulkanComputeApp::runCommandBuffer() {
    // Now we shall finally submit the recorded command buffer to a queue.

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1; // submit a single command buffer
    submitInfo.pCommandBuffers = &commandBuffer; // the command buffer to submit.

    // We create a fence.
    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, NULL, &fence));

    // We submit the command buffer on the queue, at the same time giving a fence.
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
    
    // The command will not have finished executing until the fence is signalled.
    // So we wait here.
    // We will directly after this read our buffer from the GPU,
    // and we will not be sure that the command has finished executing unless we wait for the fence.
    // Hence, we use a fence here.
    VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000));

    vkDestroyFence(device, fence, NULL);
}

void VulkanComputeApp::cleanupVulkanResources() {

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

    vkDestroyShaderModule(device, computeShaderModule, NULL);
    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
}

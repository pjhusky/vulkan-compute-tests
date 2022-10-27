#ifndef _PATHTRACERAPP_H_
#define _PATHTRACERAPP_H_

#include "vulkanComputeApp.h"

#include "external/lodepng/lodepng.h" //Used for png encoding.


//#define PATHTRACER_MODE

#define TEST_PRECISION_WITH_LARGE_SPHERE_WALLS  0

namespace {
    static float planes[] = { // normal.xyz, distToOrigin  |  emmission.xyz, 0  |  color.rgb, refltype     
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

    static float spheres[] = {  // center.xyz, radius  |  emmission.xyz, 0  |  color.rgb, refltype     
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
} // namespace

struct PathtracerApp : public VulkanComputeApp {

    struct pushConst_t {
        uint32_t imgdim[2]; //{ WIDTH, HEIGHT };
        uint32_t samps[2]; //{ 0, spp };
    } pushConst;

    PathtracerApp( const uint32_t resx, const uint32_t resy, const int32_t spp, const uint32_t workgroupSize = 16 ) {
        this->resx = resx;
        this->resy = resy;
        this->spp = spp;
        this->workgroupSize = workgroupSize;
        // Buffer size of the storage buffer that will contain the rendered mandelbrot set.
        bufferSize = sizeof(Pixel) * resx * resy;
        printf("in PathtracerApp ctor\n");

        pushConst.imgdim[0] = resx;
        pushConst.imgdim[1] = resy;
        pushConst.samps[0] = 0;
        pushConst.samps[1] = spp;
    }
    
    virtual ~PathtracerApp() {        
        vkFreeMemory(device, planeBufferMemory, NULL);
        vkDestroyBuffer(device, planesBuffer, NULL);
        vkFreeMemory(device, sphereBufferMemory, NULL);
        vkDestroyBuffer(device, spheresBuffer, NULL);
    }
    
    virtual void createComputePipeline() override {

        createShader( "shaders/pathTracer.generated.spv", computeShaderModule );

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
        pushConstantRange.size = sizeof( pushConst_t );

        // The pipeline layout allows the pipeline to access descriptor sets.
        // So we just specify the descriptor set layout we created earlier.
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

        VK_CHECK_RESULT(vkCreateComputePipelines(
            device, VK_NULL_HANDLE,
            1, &pipelineCreateInfo,
            NULL, &pipeline));
    }
    
    virtual void preRun() override {
        printf( " * before createBuffer()\n" ); fflush( stdout );
        createBuffer( bufferSize ); // output buffer
        
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize; // buffer size in bytes.
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.
        
        printf( "planebuffer create!\n" ); fflush( stdout );
        VkBufferCreateInfo planeBufferCreateInfo = {};
        planeBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        planeBufferCreateInfo.size = planeBufferSize; // buffer size in bytes.
        planeBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // buffer is used as a storage buffer.
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer is exclusive to a single queue family at a time.

        VK_CHECK_RESULT(vkCreateBuffer(device, &planeBufferCreateInfo, NULL, &planesBuffer)); // create buffer.

        VkMemoryRequirements planeBufferMemoryRequirements;
        vkGetBufferMemoryRequirements(device, planesBuffer, &planeBufferMemoryRequirements);

        VkMemoryAllocateInfo planeBufferAllocateInfo = {};
        planeBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        planeBufferAllocateInfo.allocationSize = planeBufferMemoryRequirements.size; // specify required memory.

        planeBufferAllocateInfo.memoryTypeIndex = findMemoryType(
            planeBufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &planeBufferAllocateInfo, NULL, &planeBufferMemory)); // allocate memory on device.

        // Now associate that allocated memory with the buffer. With that, the buffer is backed by actual memory.
        VK_CHECK_RESULT(vkBindBufferMemory(device, planesBuffer, planeBufferMemory, 0));

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

        VK_CHECK_RESULT(vkCreateBuffer(device, &sphereBufferCreateInfo, NULL, &spheresBuffer)); // create buffer.

        VkMemoryRequirements sphereBufferMemoryRequirements;
        vkGetBufferMemoryRequirements(device, spheresBuffer, &sphereBufferMemoryRequirements);

        VkMemoryAllocateInfo sphereBufferAllocateInfo = {};
        sphereBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        sphereBufferAllocateInfo.allocationSize = sphereBufferMemoryRequirements.size; // specify required memory.

        sphereBufferAllocateInfo.memoryTypeIndex = findMemoryType(
            sphereBufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &sphereBufferAllocateInfo, NULL, &sphereBufferMemory)); // allocate memory on device.

        // Now associate that allocated memory with the buffer. With that, the buffer is backed by actual memory.
        VK_CHECK_RESULT(vkBindBufferMemory(device, spheresBuffer, sphereBufferMemory, 0));

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
        
    }
    
    void getRenderedImage( std::vector<uint8_t> &image, const uint32_t bufferSize, const uint32_t resx, const uint32_t resy, float floatScaleFactor ) {
        void* mappedMemory = NULL;
        // Map the buffer memory, so that we can read from it on the CPU.
        
        vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedMemory);
        Pixel* pmappedMemory = (Pixel*)mappedMemory;

        // Get the color data from the buffer, and cast it to bytes.
        // We save the data to a vector.
        
        image.reserve(resx * resy * 4);
        
        for (int i = 0; i < resx*resy; i += 1) {
            image.push_back( static_cast<uint8_t>( floatScaleFactor * ( pmappedMemory[ i ].r ) ) );
            image.push_back( static_cast<uint8_t>( floatScaleFactor * ( pmappedMemory[ i ].g ) ) );
            image.push_back( static_cast<uint8_t>( floatScaleFactor * ( pmappedMemory[ i ].b ) ) );
            image.push_back( 255u );
        }        
        
        // Done reading, so unmap.
        vkUnmapMemory(device, bufferMemory);
    }

    virtual void saveRenderedImage( const char* png_filename = "pathtracer.png" ) override {
        std::vector<uint8_t> image;
        constexpr float scaleFactor = 1.0f;
        const uint32_t bufferSize = sizeof(Pixel) * resx * resy;
        getRenderedImage( image, bufferSize, resx, resy, scaleFactor );
        
        printf( "writing %s\n", png_filename );
        
        // Now we save the acquired color data to a .png.

        // due to pinhole cam the image is upside-down and mirrored - undo that!
        uint32_t* pRGBA = ( uint32_t* ) ( &image[ 0 ] );
        for ( int y = 0; y < resy; y++ ) {
            for ( int x = 0; x < resx / 2; x++ ) {
                uint32_t from = x + y * resx;
                uint32_t to   = ( resx - 1 ) - x + ( ( resy - 1 ) - y ) * resx;
                std::swap( pRGBA[ from ], pRGBA[ to ] );
            }
        }

        unsigned error = lodepng::encode(png_filename, image, resx, resy);

        if (error) printf("encoder error %d: %s", error, lodepng_error_text(error));
    }

    virtual void createDescriptorSet() override {

        // So we will allocate a descriptor set here.
        // But we need to first create a descriptor pool to do that.

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

        
        printf( "before vkCreateDescriptorPool()\n" ); fflush( stdout );
        // create descriptor pool.
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool));

        printf( "after vkCreateDescriptorPool()\n" ); fflush( stdout );

        // With the pool allocated, we can now allocate the descriptor set.
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool; // pool to allocate from.
        descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;


        printf( "before vkAllocateDescriptorSets()\n" ); fflush( stdout );
        // allocate descriptor set.
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
        printf( "after vkAllocateDescriptorSets()\n" ); fflush( stdout );

        // Next, we need to connect our actual storage buffer with the descrptor.
        // We use vkUpdateDescriptorSets() to update the descriptor set.


        printf( "before binding buffers to descriptors\n" ); fflush( stdout );
        // Specify the buffer to bind to the descriptor.
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = buffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = VK_WHOLE_SIZE;//bufferSize;

        VkDescriptorBufferInfo descriptorPlaneBufferInfo = {};
        descriptorPlaneBufferInfo.buffer = planesBuffer;
        descriptorPlaneBufferInfo.offset = 0;
        descriptorPlaneBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo descriptorSphereBufferInfo = {};
        descriptorSphereBufferInfo.buffer = spheresBuffer;
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

        printf( "after vkUpdateDescriptorSets\n" ); fflush( stdout );
    }
    
    virtual void createCommandBuffer() override {

        printf( "\n   ### entering spp loop ###\n\n" ); fflush( stdout );
        for ( int32_t sampNum = 0; sampNum < spp; sampNum++ ) {

            pushConst.samps[ 0 ] = sampNum;

            // printf( "pushConst.imgdim[0] = %u, pushConst.imgdim[1] = %u, pushConst.samps[0] = %u, pushConst.samps[1] = %u\n", 
            //     pushConst.imgdim[0], pushConst.imgdim[1],
            //     pushConst.samps[0], pushConst.samps[1] ); fflush( stdout );

            
            vkCmdPushConstants( commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConst_t ), &pushConst );

            // Calling vkCmdDispatch basically starts the compute pipeline, and executes the compute shader.
            // The number of workgroups is specified in the arguments.
            // If you are already familiar with compute shaders from OpenGL, this should be nothing new to you.
            vkCmdDispatch(commandBuffer, (uint32_t)ceil(resx / float(workgroupSize)), (uint32_t)ceil(resy / float(workgroupSize)), 1);
        }
        printf( "\n   ### leaving spp loop ###\n\n" ); fflush( stdout );
    }

private:
    VkBuffer planesBuffer;
    VkDeviceMemory planeBufferMemory;
    uint32_t planeBufferSize = sizeof( planes ); // size of `buffer` in bytes.

    VkBuffer spheresBuffer;
    VkDeviceMemory sphereBufferMemory;
    uint32_t sphereBufferSize = sizeof( spheres ); // size of `buffer` in bytes.

    // The pixels of the rendered mandelbrot set are in this format:
    struct Pixel {
        float r, g, b, a;
    };
    uint32_t bufferSize; // size of `buffer` in bytes.
    uint32_t resx, resy;
    int32_t  spp;
    uint32_t workgroupSize;
};

#endif // _PATHTRACERAPP_H_

#ifndef _MANDELBROTAPP_H_
#define _MANDELBROTAPP_H_

#include "vulkanComputeApp.h"

#include "external/lodepng/lodepng.h" //Used for png encoding.

struct MandelbrotApp : public VulkanComputeApp {

    MandelbrotApp( const uint32_t resx, const uint32_t resy, const uint32_t workgroupSize = 32 ) {
        this->resx = resx;
        this->resy = resy;
        this->workgroupSize = workgroupSize;
        
        // Buffer size of the storage buffer that will contain the rendered mandelbrot set.
        bufferSize = sizeof(Pixel) * resx * resy;
    }

    virtual ~MandelbrotApp() {
    }
    
    virtual void preRun() override {
        printf( " * before createBuffer()\n" ); fflush( stdout );
        createBuffer( bufferSize ); // output buffer
    }

    virtual void createDescriptorSet() override {

        // So we will allocate a descriptor set here.
        // But we need to first create a descriptor pool to do that.

    
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

    
        printf( "after vkUpdateDescriptorSets\n" ); fflush( stdout );
    }

    virtual void createComputePipeline() override {

        createShader( "shaders/mandelbrot.generated.spv", computeShaderModule );

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
        pushConstantRange.size = 4 * sizeof( float );

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

    virtual void createCommandBuffer() override {
    
        float kColor[4]{ 0.1f, 0.7f, 0.6f, 0.0f };
        //float kColor[4]{ 0.9f, 0.1f, 0.3f, 0.0f };
        vkCmdPushConstants( commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( float ) * 4, kColor );

        // Calling vkCmdDispatch basically starts the compute pipeline, and executes the compute shader.
        // The number of workgroups is specified in the arguments.
        // If you are already familiar with compute shaders from OpenGL, this should be nothing new to you.
        vkCmdDispatch(commandBuffer, (uint32_t)ceil(resx / float(workgroupSize)), (uint32_t)ceil(resy / float(workgroupSize)), 1);
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
    
    virtual void saveRenderedImage( const char* png_filename = "mandelbrot.png" ) override {
        std::vector<uint8_t> image;
        constexpr float scaleFactor = 255.0f;
        const uint32_t bufferSize = sizeof(Pixel) * resx * resy;
        getRenderedImage( image, bufferSize, resx, resy, scaleFactor );
        
        printf( "writing %s\n", png_filename );
        
        // Now we save the acquired color data to a .png.
        unsigned error = lodepng::encode(png_filename, image, resx, resy);

        if (error) printf("encoder error %d: %s", error, lodepng_error_text(error));
    }

private:    
    // The pixels of the rendered mandelbrot set are in this format:
    struct Pixel {
        float r, g, b, a;
    };
    uint32_t bufferSize; // size of `buffer` in bytes.
    uint32_t resx, resy;    
    uint32_t workgroupSize;
};

#endif // _MANDELBROTAPP_H_

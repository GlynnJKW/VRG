#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "RenderPass.hpp"
#include "Mesh.hpp"
#include "ShaderManager.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace vrg;

class Engine {
public:
    inline Engine() {
        init();
    }
    inline ~Engine() {
        cleanup();
    }

    inline void Run() {
        loop();
    }



private:



    std::unique_ptr<Instance> _instance;
    std::unique_ptr<ShaderManager> _sm;

    inline void init() {
        glfwInit();
        std::vector<std::string> extensions = {};
        std::vector<std::string> layers = {};
        _instance = std::make_unique<Instance>();

        _sm = std::make_unique<ShaderManager>(*_instance->Device());
        _sm->FetchFile("E:/Desktop/VulkanEngineTry1/VulkanEngineTry1/testvert.hlsl", vk::ShaderStageFlagBits::eVertex, { "testvert" });
        _sm->FetchFile("E:/Desktop/VulkanEngineTry1/VulkanEngineTry1/testfrag.hlsl", vk::ShaderStageFlagBits::eFragment, { "testfrag" });
    }

    inline void loop() {

        vk::PipelineColorBlendAttachmentState blendOpaque;
        blendOpaque.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        auto resolveAttachment = vk::AttachmentDescription({}, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

        auto depthAttachment = vk::AttachmentDescription({}, vk::Format::eD32Sfloat, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        RenderPass::SubpassDescription main_render{};
        main_render.name = "main_render";
        main_render.bindPoint = vk::PipelineBindPoint::eGraphics;
        //main_render.attachmentDescriptions = { };
        main_render.attachmentDescriptions.emplace("swapchain_image", std::make_pair(resolveAttachment, std::make_pair(RenderPass::AttachmentType::Color, blendOpaque)));
        main_render.attachmentDescriptions.emplace("primary_depth", std::make_pair(depthAttachment, std::make_pair(RenderPass::AttachmentType::DepthStencil, blendOpaque)));

        auto renderPass = std::make_shared<RenderPass>(_instance->Device(), "main", std::vector<RenderPass::SubpassDescription>{ main_render });

        auto initCommandBuffer = _instance->Device().GetCommandBuffer("Init");
        auto triangle = Mesh::Cube(*initCommandBuffer);

        BufferVector<glm::mat4> camera(_instance->Device(), 2);
        const glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, -2.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 10.0f);
        proj[1][1] *= -1;
        const std::vector<glm::mat4> cam = { model, proj * view };
        memcpy(camera.Data(), cam.data(), cam.size() * sizeof(glm::mat4));
        Buffer::StrideView cambuffer = initCommandBuffer->CopyBuffer<glm::mat4>(camera, vk::BufferUsageFlagBits::eUniformBuffer);

        std::vector<std::shared_ptr<SpirvModule>> mainshaders = { _sm->Get({ "testvert" }), _sm->Get({ "testfrag" }) };

        _instance->Device().Execute(initCommandBuffer);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto t1 = std::chrono::high_resolution_clock::now();

        float totalTime = 0;

        while (!glfwWindowShouldClose(*_instance->Window())) {
            glfwPollEvents();

            auto commandBuffer = _instance->Device().GetCommandBuffer("Frame");

            std::shared_ptr<Semaphore> renderSemaphore = std::make_shared<Semaphore>(_instance->Device(), "RenderSemaphore");
            commandBuffer->SignalOnComplete(vk::PipelineStageFlagBits::eAllCommands, renderSemaphore);

            _instance->Window().AcquireNextImage(*commandBuffer);
            commandBuffer->WaitOn(vk::PipelineStageFlagBits::eAllCommands, _instance->Window().ImageAvailableSemaphore());



            t1 = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration_cast<std::chrono::duration<float>>(t1 - t0).count();
            totalTime += frameTime;
            t0 = t1;

            if (_instance->Window().Swapchain()) {
                //main render
                auto primary_depth = std::make_shared<Texture>(_instance->Device(), "primary_depth", vk::Extent3D(_instance->Window().Extent(), 1), depthAttachment, vk::ImageUsageFlagBits::eDepthStencilAttachment);

                vrg::Texture::View backbuffer = _instance->Window().BackBuffer();
                std::shared_ptr<Framebuffer> framebuffer = std::make_shared<Framebuffer>("framebuffer", *renderPass, backbuffer, Texture::View(primary_depth) );

                std::vector<vk::ClearValue> clearValues(framebuffer->size());
                clearValues[renderPass->AttachmentIndex("swapchain_image")].setColor(std::array<float, 4>{1.0f, 0.0f, 1.0f, 0.0f});
                clearValues[renderPass->AttachmentIndex("primary_depth")].setDepthStencil({ 1, 0 });

                std::vector<glm::mat4> cam = { glm::rotate(glm::mat4(1.0f), totalTime * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), proj * view };
                memcpy(camera.Data(), cam.data(), cam.size() * sizeof(glm::mat4));
                commandBuffer->CopyBuffer((Buffer::View<std::byte>)camera, cambuffer);

                commandBuffer->BeginRenderPass(renderPass, framebuffer, clearValues);
                (*commandBuffer)->setViewport(0, { vk::Viewport(0, (float)framebuffer->Extent().height, (float)framebuffer->Extent().width, -(float)framebuffer->Extent().height, 0, 1) });
                (*commandBuffer)->setScissor(0, { vk::Rect2D(vk::Offset2D(0,0), framebuffer->Extent()) });
                
                auto pipeline = std::shared_ptr<GraphicsPipeline>(new GraphicsPipeline(_instance->Device(), "test", *renderPass, mainshaders, triangle->Geometry(), 0, vk::CullModeFlagBits::eBack, vk::PolygonMode::eFill, { {}, true, true, vk::CompareOp::eLessOrEqual, 0U, 0U, {}, {}, 0, 1 }, { blendOpaque }, { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eLineWidth }));
                commandBuffer->BindPipeline(pipeline);
                commandBuffer->BindDescriptorSet(0, std::make_shared<DescriptorSet>(
                    pipeline->DescriptorSetLayouts()[0], "main", std::unordered_map<uint32_t, Descriptor> {
                        { pipeline->Binding("ubo").binding, cambuffer }
                    })
                );



                triangle->Draw(*commandBuffer);

                //(*commandBuffer)->draw(3, 1, 0, 0);
                commandBuffer->EndRenderPass();
            }

            _instance->Device().Execute(commandBuffer);

            if (_instance->Window().Swapchain()) {
                _instance->Window().Present({ **renderSemaphore });
            }

        }
    }
    
    inline void cleanup() {

        glfwTerminate();
        _sm.reset();
        _instance->Device().Flush();
        _instance.reset();
    }

};

int main() {
    Engine app;
    app.Run();
    //try {
    //    app.Run();
    //}
    //catch (const std::exception& e) {
    //    std::cerr << e.what() << std::endl;
    //    return EXIT_FAILURE;
    //}

    return EXIT_SUCCESS;
}
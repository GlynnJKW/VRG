#include "CommandBuffer.hpp"

using namespace vrg;

CommandBuffer::CommandBuffer(Device& device, const std::string& name, Device::QueueFamily* queueFamily, vk::CommandBufferLevel level)
	: DeviceResource(device, name), _queueFamily(queueFamily) {
	vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)_device.Instance()->getProcAddr("vkCmdBeginDebugUtilsLabelEXT");
	vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)_device.Instance()->getProcAddr("vkCmdEndDebugUtilsLabelEXT");

	_completionFence = std::make_unique<Fence>(device, Name() + "_fence");
	_commandPool = queueFamily->commandBuffers.at(std::this_thread::get_id()).first;

	vk::CommandBufferAllocateInfo cmdInfo;
	cmdInfo.commandPool = _commandPool;
	cmdInfo.level = level;
	cmdInfo.commandBufferCount = 1;
	_commandBuffer = _device->allocateCommandBuffers({ cmdInfo })[0];
	
	Clear();
	_commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	_state = CommandBufferState::Recording;
}

void CommandBuffer::BeginLabel(const std::string& text, const glm::vec4& color) {
	if (vkCmdBeginDebugUtilsLabelEXT) {
		vk::DebugUtilsLabelEXT label = {};
		memcpy(label.color, &color, sizeof(color));
		label.pLabelName = text.c_str();
		vkCmdBeginDebugUtilsLabelEXT(_commandBuffer, reinterpret_cast<VkDebugUtilsLabelEXT*>(&label));
	}
}

void CommandBuffer::EndLabel() {
	if (vkCmdEndDebugUtilsLabelEXT) {
		vkCmdEndDebugUtilsLabelEXT(_commandBuffer);
	}
}

void CommandBuffer::Clear() {
	_heldResources.clear();
	_signalSemaphores.clear();
	_waitSemaphores.clear();
	//_primitiveCount = 0;
	_currentFramebuffer.reset();
	_currentRenderPass.reset();
	_boundPipeline.reset();
	//_boundVertexBuffers.clear();
	//_boundIndexBuffer = {};
	//_boundDescriptorSets.clear();
}

void CommandBuffer::Reset(const std::string& name) {
	Clear();

	_commandBuffer.reset({});
	_device->resetFences({ **_completionFence });

	_commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	_state = CommandBufferState::Recording;
}

void CommandBuffer::BeginRenderPass(std::shared_ptr<RenderPass> renderPass, std::shared_ptr<Framebuffer> framebuffer, const std::vector<vk::ClearValue>& clearValues, vk::SubpassContents contents) {
	for (uint32_t i = 0; i < renderPass->AttachmentDescriptions().size(); ++i) {
		(*framebuffer)[i].Texture().TransitionBarrier(*this, std::get<vk::AttachmentDescription>(renderPass->AttachmentDescriptions()[i]).initialLayout);
	}

	vk::RenderPassBeginInfo passInfo = {};
	passInfo.renderPass = **renderPass;
	passInfo.framebuffer = **framebuffer;
	passInfo.renderArea = vk::Rect2D({ 0, 0 }, framebuffer->Extent());
	passInfo.setClearValues(clearValues);
	_commandBuffer.beginRenderPass(&passInfo, contents);

	_currentRenderPass = renderPass;
	_currentFramebuffer = framebuffer;
	_currentSubpassIndex = 0;
	HoldResource(renderPass);
	HoldResource(framebuffer);
}

void CommandBuffer::NextSubpass(vk::SubpassContents contents) {
	_commandBuffer.nextSubpass(contents);
	++_currentSubpassIndex;
}

void CommandBuffer::EndRenderPass() {
	_commandBuffer.endRenderPass();

	for (uint32_t i = 0; i < _currentRenderPass->AttachmentDescriptions().size(); ++i) {
		auto layout = std::get<vk::AttachmentDescription>(_currentRenderPass->AttachmentDescriptions()[i]).finalLayout;
		auto& attachment = (*_currentFramebuffer)[i];
		attachment.Texture()._trackedLayout = layout;
		attachment.Texture()._trackedStages = GuessStage(layout);
		attachment.Texture()._trackedAccessFlags = GuessAccessMask(layout);
	}

	_currentRenderPass = nullptr;
	_currentFramebuffer = nullptr;
	_currentSubpassIndex = -1;
}
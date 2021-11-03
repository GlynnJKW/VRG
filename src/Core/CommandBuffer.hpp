#pragma once

#include "Framebuffer.hpp"
#include "Pipeline.hpp"

namespace vrg {

	class CommandBuffer : public DeviceResource {
	public:
		enum CommandBufferState {
			Recording,
			InFlight,
			Done
		};

		CommandBuffer(Device& device, const std::string& name, Device::QueueFamily* queueFamily, vk::CommandBufferLevel leve = vk::CommandBufferLevel::ePrimary);

		inline ~CommandBuffer() {
			if (_state == CommandBufferState::InFlight) {
				errf_color(ConsoleColor::Yellow, "Destroying CommandBuffer %s in-flight\n", Name().c_str());
			}
			Clear();
			_device->freeCommandBuffers(_commandPool, { _commandBuffer });
		}

		inline const vk::CommandBuffer& operator*() const { return _commandBuffer; }
		inline const vk::CommandBuffer* operator->() const { return &_commandBuffer; }

		inline Fence& CompletionFence() const { return *_completionFence; }
		inline Device::QueueFamily* QueueFamiliy() const { return _queueFamily; }

		inline std::shared_ptr<RenderPass> CurrentRenderPass() const { return _currentRenderPass; }
		inline std::shared_ptr<Framebuffer> CurrentFramebuffer() const { return _currentFramebuffer; }
		inline uint32_t CurrentSubpassIndex() const { return _currentSubpassIndex; }
		inline std::shared_ptr<Pipeline> BoundPipeline() const { return _boundPipeline; }

		void Reset(const std::string& name = "Command Buffer");

		inline void WaitOn(vk::PipelineStageFlags stage, Semaphore& waitSemaphore) {
			_waitSemaphores.emplace_back(stage, std::forward<Semaphore&>(waitSemaphore));
		}
		inline void SignalOnComplete(vk::PipelineStageFlags flags, std::shared_ptr<Semaphore> semaphore) {
			_signalSemaphores.push_back(semaphore);
		}

		template<std::derived_from<DeviceResource> T>
		inline T& HoldResource(const std::shared_ptr<T>& r) {
			return *static_cast<T*>(_heldResources.emplace(std::static_pointer_cast<DeviceResource>(r)).first->get());
		}


		void BeginLabel(const std::string& label, const glm::vec4& color = { 1, 1, 1, 0 });
		void EndLabel();

		inline void Barrier(vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, const vk::MemoryBarrier& barrier) {
			_commandBuffer.pipelineBarrier(srcStage, dstStage, {}, { barrier }, {}, {});
		}
		inline void Barrier(vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, const vk::BufferMemoryBarrier& barrier) {
			_commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, { barrier }, {});
		}
		inline void Barrier(vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, const vk::ImageMemoryBarrier& barrier) {
			_commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, { barrier });
		}

		inline void TransitionBarrier(vk::Image image, const vk::ImageSubresourceRange& subresourceRange, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
			TransitionBarrier(image, subresourceRange, GuessStage(oldLayout), GuessStage(newLayout), oldLayout, newLayout);
		}
		inline void TransitionBarrier(vk::Image image, const vk::ImageSubresourceRange& subresourceRange, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
			if (oldLayout == newLayout) return;
			vk::ImageMemoryBarrier barrier = {};
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange = subresourceRange;
			barrier.srcAccessMask = GuessAccessMask(oldLayout);
			barrier.dstAccessMask = GuessAccessMask(newLayout);
			Barrier(srcStage, dstStage, barrier);
		}

		void BeginRenderPass(std::shared_ptr<RenderPass> renderPass, std::shared_ptr<Framebuffer> frameBuffer, const std::vector<vk::ClearValue>& clearValues, vk::SubpassContents contents = vk::SubpassContents::eInline);
		void NextSubpass(vk::SubpassContents contents = vk::SubpassContents::eInline);
		void EndRenderPass();

		inline void BindPipeline(std::shared_ptr<Pipeline> pipeline) {
			if (_boundPipeline == pipeline) return;
			_commandBuffer.bindPipeline(pipeline->BindPoint(), **pipeline);
			_boundPipeline = pipeline;
			_boundDescriptorSets.clear();
			_boundVertexBuffers.clear();
			_boundIndexBuffer = {};
			HoldResource(pipeline);
		}

		template<typename T> 
		inline void BindVertexBuffer(uint32_t index, const Buffer::View<T>& view) {
			if (_boundVertexBuffers[index] != view) {
				_boundVertexBuffers[index] = view;
				_commandBuffer.bindVertexBuffers(index, { *view.Buffer() }, { view.Offset() });
			}
		}

		inline void BindIndexBuffer(const Buffer::StrideView& view) {
			if (_boundIndexBuffer != view) {
				_boundIndexBuffer = view;
				vk::IndexType type = stride_to_index_type(view.Stride());
				_commandBuffer.bindIndexBuffer(*view.Buffer(), view.Offset(), type);
			}
		}

		inline void BindDescriptorSet(uint32_t index, std::shared_ptr<DescriptorSet> descriptorSet) {
			if (!_boundPipeline) throw std::runtime_error("Cannot bind descriptor set without a pipeline bound");
			descriptorSet->FlushWrites();
			HoldResource(descriptorSet);
			//if(!_boundFramebuffer) TransitionImages(*descriptorSet);

			if (index >= _boundDescriptorSets.size()) {
				_boundDescriptorSets.resize(index + 1);
			}
			_boundDescriptorSets[index] = descriptorSet;
			_commandBuffer.bindDescriptorSets(_boundPipeline->BindPoint(), _boundPipeline->Layout(), index, **descriptorSet, nullptr);
		}

		template<typename T, typename S>
		inline const Buffer::View<S>& CopyBuffer(const Buffer::View<T>& src, const Buffer::View<S>& dst) {
			if (src.ByteSize() != dst.ByteSize()) throw std::invalid_argument("src and dst must be the same size");
			_commandBuffer.copyBuffer(*HoldResource(src.BufferPtr()), *HoldResource(dst.BufferPtr()), { vk::BufferCopy(src.Offset(), dst.Offset(), src.ByteSize()) });
		}

		template<typename T>
		inline Buffer::View<T> CopyBuffer(const Buffer::View<T>& src, vk::BufferUsageFlagBits bufferUsage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) {
			auto dst = std::make_shared<Buffer>(_device, src.Buffer().Name(), src.ByteSize(), bufferUsage | vk::BufferUsageFlagBits::eTransferDst, memoryUsage);
			_commandBuffer.copyBuffer(*HoldResource(src.BufferPtr()), *HoldResource(dst), { vk::BufferCopy(src.Offset(), 0, src.ByteSize()) });
			return dst;
		}

	private:
		friend class Device;

		PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = 0;
		PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = 0;

		void Clear();
		inline bool CheckDone() {
			if (_state == CommandBufferState::InFlight) {
				if (_completionFence->Status() == vk::Result::eSuccess) {
					_state = CommandBufferState::Done;
					Clear();
				}
			}
			return _state == CommandBufferState::Done;
		}


		vk::CommandBuffer _commandBuffer;
		
		Device::QueueFamily* _queueFamily;
		vk::CommandPool _commandPool;
		CommandBufferState _state;

		std::unique_ptr<Fence> _completionFence;

		std::vector<std::shared_ptr<Semaphore>> _signalSemaphores;
		std::vector<std::pair<vk::PipelineStageFlags, Semaphore&>> _waitSemaphores;

		std::unordered_set<std::shared_ptr<DeviceResource>> _heldResources;

		std::shared_ptr<Framebuffer> _currentFramebuffer;
		std::shared_ptr<RenderPass> _currentRenderPass;
		uint32_t _currentSubpassIndex = 0;
		std::shared_ptr<Pipeline> _boundPipeline = nullptr;
		std::unordered_map<uint32_t, Buffer::View<std::byte>> _boundVertexBuffers;
		Buffer::StrideView _boundIndexBuffer;
		std::vector<std::shared_ptr<DescriptorSet>> _boundDescriptorSets;
	};
}
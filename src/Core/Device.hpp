#pragma once

#include "Instance.hpp"

namespace vrg {

	class CommandBuffer;

	class DeviceResource {
	private:
		std::string _name;
	public:
		Device& _device;
		inline DeviceResource(Device& device, std::string name) : _device(device), _name(name) {}
		inline virtual ~DeviceResource() {};
		inline const std::string& Name() const { return _name; };
	};

	class Device {
	public:
		struct QueueFamily {
			uint32_t familyIndex;
			std::vector<vk::Queue> queues;
			vk::QueueFamilyProperties properties;
			bool surfaceSupport;

			std::unordered_map<std::thread::id, std::pair<vk::CommandPool, std::list<std::shared_ptr<CommandBuffer>>>> commandBuffers;
		};

		Device(vrg::Instance& instance, uint32_t deviceIndex = 0, std::vector<std::string> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }, std::vector<std::string> validationLayers = {});
		~Device();

		inline const vk::Device& operator*() const { return _device; }
		inline const vk::Device* operator->() const { return &_device; }

		inline vk::PhysicalDevice PhysicalDevice() const { return _physicalDevice; }
		inline const vk::PhysicalDeviceLimits& Limits() const { return _limits; }
		//inline vk::PipelineCache PipelineCache() const { return _pipelineCache; }
		inline const std::vector<uint32_t>& QueueFamilies(uint32_t index) const { return _queueFamilyIndices; }

		inline vrg::Instance& Instance() const { return _instance; }

		QueueFamily* FindQueueFamily(vk::SurfaceKHR surface);

		std::shared_ptr<CommandBuffer> GetCommandBuffer(const std::string& name, vk::QueueFlags queueFlags = vk::QueueFlagBits::eGraphics, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
		void Execute(std::shared_ptr<CommandBuffer> commandBuffer);
		void Flush();


		inline const VmaAllocator& Allocator() const { return _memoryAllocator; }
		VmaAllocation AllocateMemory(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN);
		VmaAllocation AllocateImage(vk::Image image, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN);
		VmaAllocation AllocateBuffer(vk::Buffer buffer, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN);
		VmaAllocation AllocateUnmappedBuffer(vk::Buffer buffer, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN);

		inline const VmaAllocationInfo& AllocationInfo(VmaAllocation alloc) const { return _allocationInfo.at(alloc); }

		void FreeBuffer(vk::Buffer buffer, VmaAllocation alloc);
		void FreeImage(vk::Image image, VmaAllocation alloc);

	private:
		friend class DescriptorSet;
		friend class Instance;
		friend class CommandBuffer;

		vrg::Instance& _instance;
		vk::Device _device;
		vk::PhysicalDevice _physicalDevice;

		vk::PhysicalDeviceLimits _limits;
		vk::PhysicalDeviceProperties _properties;
		vk::PhysicalDeviceFeatures _features;

		std::vector<uint32_t> _queueFamilyIndices;
		std::unordered_map<uint32_t, QueueFamily> _queueFamilies;
		vk::DescriptorPool _descriptorPool;

		VmaAllocator _memoryAllocator;
		std::unordered_map<VmaAllocation, VmaAllocationInfo> _allocationInfo;
	};

	class Fence : public DeviceResource {
	private:
		vk::Fence _fence;

	public:
		inline Fence(Device& device, const std::string& name)
			: DeviceResource(device, name) {
			_fence = _device->createFence({});
		}
		inline ~Fence() { _device->destroyFence(_fence); }
		inline const vk::Fence& operator*() const { return _fence; }
		inline const vk::Fence* operator->() const { return &_fence; }
		inline vk::Result Status() { return _device->getFenceStatus(_fence); }
		inline vk::Result Wait(uint64_t timeout = UINT64_MAX) {
			return _device->waitForFences({ _fence }, true, timeout);
		}
	};

	class Semaphore : public DeviceResource {
	private:
		vk::Semaphore _semaphore;

	public:
		inline Semaphore(Device& device, const std::string& name)
			: DeviceResource(device, name) {
			_semaphore = _device->createSemaphore({});
		}
		inline ~Semaphore() { _device->destroySemaphore(_semaphore); }
		inline const vk::Semaphore& operator*() const { return _semaphore; }
		inline const vk::Semaphore* operator->() const { return &_semaphore; }

	};
}

static_assert(vrg::Hashable<VmaAllocation>);
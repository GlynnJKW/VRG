#include "Device.hpp"
#include "Window.hpp"
#include "CommandBuffer.hpp"

using namespace vrg;

Device::Device(vrg::Instance& instance, uint32_t deviceIndex, std::vector<std::string> extensions, std::vector<std::string> validationLayers) 
	: _instance(instance) {

#pragma region Physical device
	printf("Finding physical device...");

	std::vector<vk::PhysicalDevice> devices = _instance->enumeratePhysicalDevices();
	if (devices.empty()) {
		errf_color(ConsoleColor::Red, "No Vulkan devices available\n");
		throw std::runtime_error("No Vulkan devices available");
	}
	if (deviceIndex >= devices.size()) {
		printf_color(ConsoleColor::Yellow, "Device index %u out of bounds, defaulting to 0...", deviceIndex);
		deviceIndex = 0;
	}
	_physicalDevice = devices[deviceIndex];
	PrintSuccessMessage();

	_properties = _physicalDevice.getProperties();
	printf_color(ConsoleColor::Cyan, "Using physical device %u: %s\nVulkan %u.%u.%u\n", deviceIndex, _properties.deviceName.data(), VK_VERSION_MAJOR(_properties.apiVersion), VK_VERSION_MINOR(_properties.apiVersion), VK_VERSION_PATCH(_properties.apiVersion));
	_features = _physicalDevice.getFeatures();
	_limits = _properties.limits;
#pragma endregion

#pragma region Queue Families
	printf("Checking device queues...");
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	float queuePriority = 1.0f;

	for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
		if (queueFamilyProperties[i].queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer)) {
			vk::DeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.queueFamilyIndex = i;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}
	}
	if (queueCreateInfos.empty()) {
		errf_color(ConsoleColor::Red, "No requested queues available\n");
		throw std::runtime_error("No requested queues available");
	}
	PrintSuccessMessage();
#pragma endregion

#pragma region Logical Device
	printf("Creating logical device...");
	vk::PhysicalDeviceFeatures deviceFeatures{};

	vk::DeviceCreateInfo deviceInfo = {};
	deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceInfo.queueCreateInfoCount = queueCreateInfos.size();
	deviceInfo.pEnabledFeatures = &deviceFeatures;

	std::vector<const char*> deviceExts;
	for (std::string& s : extensions) {
		deviceExts.push_back(s.c_str());
	}
	deviceInfo.ppEnabledExtensionNames = deviceExts.data();
	deviceInfo.enabledExtensionCount = deviceExts.size();

	std::vector<const char*> deviceLayers;
	for (std::string& s : validationLayers) {
		deviceLayers.push_back(s.c_str());
	}
	deviceInfo.ppEnabledLayerNames = deviceLayers.data();
	deviceInfo.enabledLayerCount = deviceLayers.size();


	if (_physicalDevice.createDevice(&deviceInfo, nullptr, &_device) != vk::Result::eSuccess) {
		errf_color(ConsoleColor::Red, "Could not create logical device\n");
		throw std::runtime_error("Could not create logical device");
	}
	PrintSuccessMessage();
#pragma endregion

#pragma region Descriptor Pool
	std::vector<vk::DescriptorPoolSize> poolSizes{
	vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 					std::min(1024u, _limits.maxDescriptorSetSamplers)),
	vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 		std::min(1024u, _limits.maxDescriptorSetSampledImages)),
	vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 			std::min(1024u, _limits.maxDescriptorSetInputAttachments)),
	vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 				std::min(1024u, _limits.maxDescriptorSetSampledImages)),
	vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 				std::min(1024u, _limits.maxDescriptorSetStorageImages)),
	vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 				std::min(1024u, _limits.maxDescriptorSetUniformBuffers)),
	vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 		std::min(1024u, _limits.maxDescriptorSetUniformBuffersDynamic)),
	vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 				std::min(1024u, _limits.maxDescriptorSetStorageBuffers)),
	vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 		std::min(1024u, _limits.maxDescriptorSetStorageBuffersDynamic))
	};

	_descriptorPool = _device.createDescriptorPool(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 8192, poolSizes));
#pragma endregion

#pragma region Create Queues
	for (const auto& info : queueCreateInfos) {
		QueueFamily q = {};
		q.familyIndex = info.queueFamilyIndex;
		q.properties = queueFamilyProperties[info.queueFamilyIndex];
		q.surfaceSupport = _physicalDevice.getSurfaceSupportKHR(info.queueFamilyIndex, _instance.Window().Surface());
		for (uint32_t i = 0; i < 1; ++i) {
			q.queues.push_back(_device.getQueue(info.queueFamilyIndex, i));
		}
		_queueFamilies.emplace(q.familyIndex, q);
		_queueFamilyIndices.push_back(q.familyIndex);
	}
#pragma endregion

#pragma region Create Memory Allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
	allocatorInfo.physicalDevice = _physicalDevice;
	allocatorInfo.device = _device;
	allocatorInfo.instance = *_instance;

	if (vmaCreateAllocator(&allocatorInfo, &_memoryAllocator) != VK_SUCCESS) {
		throw std::runtime_error("Could not create memory allocator");
	}
#pragma endregion
}

Device::~Device() {
	Flush();
	
	for (auto& [index, queueFamily] : _queueFamilies) {
		for (auto& [threadid, pool] : queueFamily.commandBuffers) {
			pool.second.clear();
			_device.destroyCommandPool(pool.first);
		}
	}
	_queueFamilies.clear();

	_device.destroyDescriptorPool(_descriptorPool);
	_allocationInfo.clear();

	vmaDestroyAllocator(_memoryAllocator);
	_device.destroy();
}

VmaAllocation Device::AllocateMemory(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage) {
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.requiredFlags = (VkMemoryPropertyFlags)properties;
	allocInfo.usage = memoryUsage;

	VkMemoryRequirements req = requirements;
	VmaAllocation allocation;
	VmaAllocationInfo info;
	vmaAllocateMemory(_memoryAllocator, &req, &allocInfo, &allocation, &info);
	_allocationInfo.emplace(allocation, info);
	return allocation;
}

VmaAllocation Device::AllocateImage(vk::Image image, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage) {
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.requiredFlags = (VkMemoryPropertyFlags)properties;
	allocInfo.usage = memoryUsage;

	VmaAllocation allocation;
	VmaAllocationInfo info;
	vmaAllocateMemoryForImage(_memoryAllocator, image, &allocInfo, &allocation, &info);
	vmaBindImageMemory(_memoryAllocator, allocation, image);
	_allocationInfo.emplace(allocation, info);
	return allocation;
}

VmaAllocation Device::AllocateBuffer(vk::Buffer buffer, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage) {
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.requiredFlags = (VkMemoryPropertyFlags)properties;
	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocInfo.usage = memoryUsage;

	VmaAllocation allocation;
	VmaAllocationInfo info;
	vmaAllocateMemoryForBuffer(_memoryAllocator, buffer, &allocInfo, &allocation, &info);
	vmaBindBufferMemory(_memoryAllocator, allocation, buffer);
	_allocationInfo.emplace(allocation, info);
	return allocation;
}

VmaAllocation Device::AllocateUnmappedBuffer(vk::Buffer buffer, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties, VmaMemoryUsage memoryUsage) {
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.requiredFlags = (VkMemoryPropertyFlags)properties;
	allocInfo.usage = memoryUsage;

	VmaAllocation allocation;
	VmaAllocationInfo info;
	vmaAllocateMemoryForBuffer(_memoryAllocator, buffer, &allocInfo, &allocation, &info);
	vmaBindBufferMemory(_memoryAllocator, allocation, buffer);
	_allocationInfo.emplace(allocation, info);
	return allocation;
}

void Device::FreeBuffer(vk::Buffer buffer, VmaAllocation alloc) {
	_allocationInfo.erase(alloc);
	vmaDestroyBuffer(_memoryAllocator, buffer, alloc);
}

void Device::FreeImage(vk::Image image, VmaAllocation alloc) {
	_allocationInfo.erase(alloc);
	vmaDestroyImage(_memoryAllocator, image, alloc);
}


Device::QueueFamily* Device::FindQueueFamily(vk::SurfaceKHR surface) {
	for (auto& [familyIndex, family] : _queueFamilies) {
		if (_physicalDevice.getSurfaceSupportKHR(familyIndex, surface)) {
			return &family;
		}
	}
	return nullptr;
}

std::shared_ptr<CommandBuffer> Device::GetCommandBuffer(const std::string& name, vk::QueueFlags queueFlags, vk::CommandBufferLevel level) {
	QueueFamily* queueFamily = nullptr;
	for (auto& [queueFamilyIndex, family] : _queueFamilies)
		if (family.properties.queueFlags & queueFlags)
			queueFamily = &family;

	if (queueFamily == nullptr) throw std::invalid_argument("Invalid QueueFlags");

	auto& [commandPool, commandBuffers] = queueFamily->commandBuffers[std::this_thread::get_id()];
	//std::list<std::shared_ptr<CommandBuffer>> commandBuffers = 

	if (!commandPool) {
		commandPool = _device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamily->familyIndex));
	}

	//Searching for done commandbuffers with only one reference (if only one reference, it exists only as part of command buffer pool for this device)
	std::shared_ptr<CommandBuffer> commandBuffer;
	if (level == vk::CommandBufferLevel::ePrimary) {
		//remove finished command buffers
		auto finished = std::remove_if(commandBuffers.begin(), commandBuffers.end(), [](auto c) { return c->CheckDone(); });

		if (finished != commandBuffers.end()) {
			auto unused = std::find_if(finished, commandBuffers.end(), [](auto c) { return c.use_count() == 1; });
			if (unused != commandBuffers.end()) {
				commandBuffer = *unused;
			}
			commandBuffers.erase(finished, commandBuffers.end());
		}
	}

	if (commandBuffer) {
		commandBuffer->Reset(name);
		return commandBuffer;
	}
	else {
		return std::make_shared<CommandBuffer>(*this, name, queueFamily, level);
	}

}

void Device::Execute(std::shared_ptr<CommandBuffer> commandBuffer) {
	std::vector<vk::PipelineStageFlags> waitStages;
	std::vector<vk::Semaphore> waitSemaphores;
	std::vector<vk::Semaphore> signalSemaphores;
	std::vector<vk::CommandBuffer> commandBuffers = { **commandBuffer };
	for (auto& [stage, semaphore] : commandBuffer->_waitSemaphores) {
		waitStages.push_back(stage);
		waitSemaphores.push_back(*semaphore);
	}
	for (auto& semaphore : commandBuffer->_signalSemaphores) {
		signalSemaphores.push_back(**semaphore);
	}
	(*commandBuffer)->end();
	commandBuffer->_queueFamily->queues[0].submit({ vk::SubmitInfo(waitSemaphores, waitStages, commandBuffers, signalSemaphores) }, **commandBuffer->_completionFence);
	commandBuffer->_state = CommandBuffer::CommandBufferState::InFlight;

	commandBuffer->_queueFamily->commandBuffers.at(std::this_thread::get_id()).second.emplace_back(commandBuffer);
}

void Device::Flush() {
	_device.waitIdle();
	for (auto& [index, queueFamily] : _queueFamilies) {
		for (auto& [threadid, cmdpair] : queueFamily.commandBuffers) {
			for (auto& commandBuffer : cmdpair.second) {
				commandBuffer->CheckDone();
			}
		}
	}
}
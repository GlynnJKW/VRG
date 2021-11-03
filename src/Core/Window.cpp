#include "Window.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"


using namespace vrg;

Window::Window(Instance& instance, uint32_t w, uint32_t h)
	: _instance(instance), _width(w), _height(h){
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	printf("Creating window and surface...");
	_window = glfwCreateWindow(_width, _height, "Vulkan", nullptr, nullptr);

	if (glfwCreateWindowSurface(*_instance, _window, nullptr, (VkSurfaceKHR*)&_surface) != (VkResult)vk::Result::eSuccess) {
		errf_color(ConsoleColor::Red, "Could not create surface\n");
		throw std::runtime_error("Could not create surface");
	}
	PrintSuccessMessage();
}

Window::~Window() {
	if (_swapchain) {
		DestroySwapchain();
	}
	_instance->destroySurfaceKHR(_surface);
	glfwDestroyWindow(_window);
}

void Window::AcquireNextImage(CommandBuffer& commandBuffer) {
	if (!_swapchain) CreateSwapchain(commandBuffer);

	_imageAvailableSemaphoreIndex = (_imageAvailableSemaphoreIndex + 1) % _imageAvailableSemaphores.size();

	auto result = (*_swapchainDevice)->acquireNextImageKHR(_swapchain, UINT64_MAX, **_imageAvailableSemaphores[_imageAvailableSemaphoreIndex], {});
	
	if (result.result == vk::Result::eErrorOutOfDateKHR || result.result == vk::Result::eSuboptimalKHR) {
		CreateSwapchain(commandBuffer);
		if (!_swapchain) {
			errf_color(ConsoleColor::Red, "Failed to recreate swap chain\n");
			throw std::runtime_error("Failed to recreate swap chain");
		}
		result = (*_swapchainDevice)->acquireNextImageKHR(_swapchain, UINT64_MAX, **_imageAvailableSemaphores[_imageAvailableSemaphoreIndex], {});
	}
	_backbufferIndex = result.value;
}

void Window::Present(const std::vector<vk::Semaphore>& waitSemaphores) {
	std::vector<vk::SwapchainKHR> swapchains{ _swapchain };
	std::vector<uint32_t> imageIndices{ _backbufferIndex };
	auto result = _presentQueueFamily->queues[0].presentKHR(vk::PresentInfoKHR(waitSemaphores, swapchains, imageIndices));
	if (result != vk::Result::eSuccess) {
		errf_color(ConsoleColor::Yellow, "Failed to present\n");
	}
	++_presentCount;
}

void Window::CreateSwapchain(CommandBuffer& commandBuffer) {
	if (_swapchain) DestroySwapchain();
	_swapchainDevice = &commandBuffer._device;
	
	_presentQueueFamily = _swapchainDevice->FindQueueFamily(_surface);
	if (!_presentQueueFamily) {
		throw std::runtime_error("Window surface not supported by selected device");
	}

#pragma region Swapchain
	SwapchainSupportDetails supportDetails;
	supportDetails.formats = _swapchainDevice->PhysicalDevice().getSurfaceFormatsKHR(_surface);
	supportDetails.presentModes = _swapchainDevice->PhysicalDevice().getSurfacePresentModesKHR(_surface);
	supportDetails.capabilities = _swapchainDevice->PhysicalDevice().getSurfaceCapabilitiesKHR(_surface);

	if (supportDetails.formats.empty() && supportDetails.presentModes.empty()) {
		throw std::runtime_error("Swapchain has no supported formats/present modes");
	}
	_surfaceFormat = supportDetails.formats[0];
	for (const vk::SurfaceFormatKHR& format : supportDetails.formats) {
		if (format.format == vk::Format::eR8G8B8A8Unorm) {
			_surfaceFormat = format;
			break;
		}
		else if (format.format == vk::Format::eB8G8R8A8Unorm) {
			_surfaceFormat = format;
		}

	}
	std::vector<vk::PresentModeKHR> preferredPresentModes{ vk::PresentModeKHR::eMailbox };
	//if (mAllowTearing) preferredPresentModes = { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifoRelaxed };
	_surfacePresentMode = vk::PresentModeKHR::eFifo;
	for (auto mode : preferredPresentModes) {
		if (std::find(supportDetails.presentModes.begin(), supportDetails.presentModes.end(), mode) != supportDetails.presentModes.end()) {
			_surfacePresentMode = mode;
		}
	}

	int width, height;
	glfwGetFramebufferSize(_window, &width, &height);
	_extent.width = static_cast<uint32_t>(width);
	_extent.height = static_cast<uint32_t>(height);

	if (supportDetails.capabilities.currentExtent.width != UINT32_MAX) {
		_extent = supportDetails.capabilities.currentExtent;
	}
	else {
		_extent.width = std::max(supportDetails.capabilities.minImageExtent.width, std::min(supportDetails.capabilities.maxImageExtent.width, _extent.width));
		_extent.height = std::max(supportDetails.capabilities.minImageExtent.height, std::min(supportDetails.capabilities.maxImageExtent.height, _extent.height));
	}
	uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
	if (supportDetails.capabilities.maxImageCount > 0 && imageCount > supportDetails.capabilities.maxImageCount) {
		imageCount = supportDetails.capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.surface = _surface;
	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = _surfaceFormat.format;
	swapchainInfo.imageColorSpace = _surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = _extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainInfo.preTransform = supportDetails.capabilities.currentTransform;
	swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainInfo.presentMode = _surfacePresentMode;
	swapchainInfo.clipped = VK_TRUE;

	printf("Creating swapchain...");
	if ((*_swapchainDevice)->createSwapchainKHR(&swapchainInfo, nullptr, &_swapchain) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create swap chain");
	}
	PrintSuccessMessage();
#pragma endregion

#pragma region Backbuffers/Images
	std::vector<vk::Image> images = (*_swapchainDevice)->getSwapchainImagesKHR(_swapchain);
	_swapchainImages.resize(images.size());
	_imageAvailableSemaphores.clear();
	_imageAvailableSemaphores.resize(images.size());
	_backbufferIndex = 0;

	for (uint32_t i = 0; i < images.size(); ++i) {
		auto texture = std::make_shared<Texture>(images[i], *_swapchainDevice, "swapchain_image", vk::Extent3D(_extent, 1), swapchainInfo.imageFormat, swapchainInfo.imageArrayLayers, 1, vk::SampleCountFlagBits::e1, swapchainInfo.imageUsage);
		_swapchainImages[i] = Texture::View( texture, 0, 1, 0, 1, vk::ImageAspectFlagBits::eColor);
		_swapchainImages[i].Texture().TransitionBarrier(commandBuffer, vk::ImageLayout::ePresentSrcKHR);
		_imageAvailableSemaphores[i] = std::make_unique<Semaphore>(*_swapchainDevice, "Swapchain ImageAvailableSemaphore" + std::to_string(i));
	}
#pragma endregion
}


void Window::DestroySwapchain() {
	_swapchainImages.clear();
	(*_swapchainDevice)->destroySwapchainKHR(_swapchain);
	_swapchain = nullptr;
}
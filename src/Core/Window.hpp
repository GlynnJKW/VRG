#pragma once

#include "Texture.hpp"

namespace vrg {
	class Window {
	public:

		struct SwapchainSupportDetails {
			vk::SurfaceCapabilitiesKHR capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR> presentModes;
		};

		Window(vrg::Instance& instance, uint32_t w = 800, uint32_t h = 600);
		~Window();

		inline GLFWwindow* operator*() const { return _window; }

		inline const vk::SurfaceKHR Surface() const { return _surface; }

		void AcquireNextImage(vrg::CommandBuffer& commandBuffer);
		void Present(const std::vector<vk::Semaphore>& waitSemaphores);

		inline vrg::Device::QueueFamily* PresentQueueFamily() const { return _presentQueueFamily; }

		inline vk::SwapchainKHR Swapchain() const { return _swapchain; }
		inline vk::SurfaceFormatKHR SurfaceFormat() const { return _surfaceFormat; }
		inline vk::Extent2D Extent() const { return _extent; }

		inline Texture::View BackBuffer() const { return _swapchainImages.empty() ? Texture::View() : _swapchainImages[_backbufferIndex]; }
		inline Texture::View BackBuffer(uint32_t i) const { return _swapchainImages.empty() ? Texture::View() : _swapchainImages[i]; }

		inline uint32_t BackBufferCount() const { return (uint32_t)_swapchainImages.size(); }
		inline uint32_t BackBufferIndex() const { return _backbufferIndex; }

		inline Semaphore& ImageAvailableSemaphore() const { return *_imageAvailableSemaphores[_imageAvailableSemaphoreIndex]; }

	private:
		friend class instance;

		void CreateSwapchain(CommandBuffer& commandBuffer);
		void DestroySwapchain();

		Instance& _instance;
		GLFWwindow* _window;
		vk::SurfaceKHR _surface;
		vk::SurfaceFormatKHR _surfaceFormat;
		vk::PresentModeKHR _surfacePresentMode;
		vk::SwapchainKHR _swapchain;
		vk::Extent2D _extent;
		vrg::Device* _swapchainDevice;

		uint32_t _backbufferIndex = 0;
		std::vector<Texture::View> _swapchainImages;

		vrg::Device::QueueFamily* _presentQueueFamily = nullptr;

		uint32_t _width;
		uint32_t _height;
		size_t _presentCount = 0;

		uint32_t _imageAvailableSemaphoreIndex = 0;
		std::vector<std::unique_ptr<Semaphore>> _imageAvailableSemaphores;
	};
}

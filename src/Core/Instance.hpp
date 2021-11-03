#pragma once

#include "Util.hpp"

namespace vrg {

	class Device;
	class Window;

	enum DebugLevel {
		Error = 0,
		Warning = 1,
		Info = 2,
		Verbose = 4
	};

	class Instance {
	public:
		static bool debug_callback;
		static DebugLevel debug_level;

		Instance(std::vector<std::string> extensions = {}, std::vector<std::string> validationLayers = { "VK_LAYER_KHRONOS_validation" });
		~Instance();

		inline const vk::Instance operator*() const { return _instance; }
		inline const vk::Instance* operator->() const { return &_instance; }

		inline const std::vector<vk::ExtensionProperties> SupportedExtensions() const { return _supportedExtensions; }

		inline vrg::Device& Device() const { return *_device; }
		inline vrg::Window& Window() const { return *_window; }

	private:
		vk::Instance _instance;
		std::unique_ptr<vrg::Device> _device;
		std::unique_ptr<vrg::Window> _window;
		std::vector<vk::ExtensionProperties> _supportedExtensions;
		std::unordered_set<std::string> _enabledExtensions;
		std::vector<vk::LayerProperties> _supportedLayers;
		std::unordered_set<std::string> _enabledLayers;

		vk::DebugUtilsMessengerEXT _debugMessenger;
	};
}


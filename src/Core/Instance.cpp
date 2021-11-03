#include "Instance.hpp"
#include "Window.hpp"

using namespace vrg;

bool Instance::debug_callback = false;
vrg::DebugLevel Instance::debug_level = vrg::DebugLevel::Warning;

VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
		errf_color(ConsoleColor::Red, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		throw std::runtime_error(pCallbackData->pMessage);
	}
	else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		if (vrg::Instance::debug_level <= vrg::DebugLevel::Warning)
			errf_color(ConsoleColor::Yellow, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	}
	else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
		if (vrg::Instance::debug_level <= vrg::DebugLevel::Info)
			errf_color(ConsoleColor::Bold, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	}
	else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
		if (vrg::Instance::debug_level <= vrg::DebugLevel::Verbose)
			printf("%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	}

	return VK_FALSE;
}

Instance::Instance(std::vector<std::string> extensions, std::vector<std::string> validationLayers) {
	vk::ApplicationInfo appInfo = {};
	appInfo.pApplicationName = "Vulkan Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.pEngineName = "Gengine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	vk::InstanceCreateInfo instanceInfo = {};
	instanceInfo.pApplicationInfo = &appInfo;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	for (int i = 0; i < glfwExtensionCount; ++i) {
		_enabledExtensions.insert(glfwExtensions[i]);
	}
	for (std::string s : extensions) {
		_enabledExtensions.insert(s);
	}

	if (!validationLayers.empty()) {
		_enabledExtensions.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

#pragma region Extensions
	_supportedExtensions = vk::enumerateInstanceExtensionProperties();
	printf("Checking required extensions...");
	for (const std::string& extension : _enabledExtensions) {
		if (std::find_if(_supportedExtensions.begin(), _supportedExtensions.end(), [extension](vk::ExtensionProperties e) { return std::strcmp(extension.c_str(), e.extensionName) == 0; }) == _supportedExtensions.end()) {
			errf_color(ConsoleColor::Red, "Failure!\n");
			throw std::runtime_error("Extension " + extension + " not supported");
		}
	}
	PrintSuccessMessage();

	std::vector<const char*> exts;
	for (const std::string& s : _enabledExtensions) {
		exts.push_back(s.c_str());
	}
	if (!exts.empty()) {
		instanceInfo.enabledExtensionCount = exts.size();
		instanceInfo.ppEnabledExtensionNames = exts.data();
	}
#pragma endregion

#pragma region Validation Layers
	for (std::string s : validationLayers) {
		_enabledLayers.insert(s);
	}

	printf("Checking requested layers...");
	_supportedLayers = vk::enumerateInstanceLayerProperties();
	for (const std::string& layer : _enabledLayers) {
		if (std::find_if(_supportedLayers.begin(), _supportedLayers.end(), [layer](vk::LayerProperties l) { return std::strcmp(layer.c_str(), l.layerName) == 0; }) == _supportedLayers.end()) {
			errf_color(ConsoleColor::Red, "Failure!\n");
			throw std::runtime_error("Layer " + layer + " not available");
		}
	}
	PrintSuccessMessage();

	std::vector<const char*> layers;
	for (const std::string& s : _enabledLayers) {
		layers.push_back(s.c_str());
	}
	if (!layers.empty()) {
		instanceInfo.enabledLayerCount = layers.size();
		instanceInfo.ppEnabledLayerNames = layers.data();
	}

#pragma endregion

#pragma region Debug Callback
	if (debug_callback) {
		printf("Creating Debug Messenger...");
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");

		vk::DebugUtilsMessengerCreateInfoEXT debugInfo = {};
		debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
		debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
		debugInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(DebugCallback);

		vk::Result result = (vk::Result)func(_instance, reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugInfo), nullptr, reinterpret_cast<VkDebugUtilsMessengerEXT*>(&_debugMessenger));
		if (result == vk::Result::eSuccess) {
			PrintSuccessMessage();
		}
		else {
			errf_color(ConsoleColor::Red, "Failed!\n");
			_debugMessenger = nullptr;
		}
	}
#pragma endregion

	printf("Creating vk::Instance...");
	if (vk::createInstance(&instanceInfo, nullptr, &_instance) != vk::Result::eSuccess) {
		errf_color(ConsoleColor::Red, "Failure!\n");
		throw std::runtime_error("Could not create vk::Instance");
	}
	PrintSuccessMessage();

	_window = std::make_unique<vrg::Window>(*this);

	std::vector<std::string> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME
	};
	_device = std::make_unique<vrg::Device>(*this, 0, deviceExtensions, validationLayers);

}

Instance::~Instance() {
	_window.reset();
	_device.reset();
	if (debug_callback) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(_instance, _debugMessenger, nullptr);
		}
	}
	_instance.destroy();
};
#include "SpirvModule.hpp"

using namespace vrg;

SpirvModule::SpirvModule(const vk::Device& device, const vk::ShaderStageFlagBits stage, const std::string filename, const std::string entryPoint)
: _stage(stage), _device(device), _entryPoint(entryPoint) {
	//printf("Reading Spirv file...");
	std::ifstream filep(filename, std::ios::ate | std::ios::binary);

	if (!filep.is_open()) {
		//errf_color(ConsoleColor::Red, "Failed!\,");
		throw std::runtime_error("Failed to open file " + filename);
	}

	size_t fileSize = (size_t)filep.tellg();
	_spirv.resize(fileSize);
	filep.seekg(0);
	filep.read(_spirv.data(), fileSize);
	filep.close();

	vk::ShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.codeSize = _spirv.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t*>(_spirv.data());

	if (_device.createShaderModule(&moduleInfo, nullptr, &_module) != vk::Result::eSuccess) {
		//errf_color(ConsoleColor::Red, "Failed!\,");
		throw std::runtime_error("Failed to create shader module " + filename);
	}

	//HARDCODED FOR TESTING, REMOVE
	if (_stage == vk::ShaderStageFlagBits::eVertex) {
		RasterStageVariable r;
		r.attributeId.type = VertexAttributeType::Position;
		r.attributeId.typeIndex = 0;
		r.format = vk::Format::eR32G32B32Sfloat;
		r.location = 0;
		_stageInputs.emplace("inPosition", r);

		RasterStageVariable r2;
		r2.attributeId.type = VertexAttributeType::Normal;
		r2.attributeId.typeIndex = 0;
		r2.format = vk::Format::eR32G32B32Sfloat;
		r2.location = 1;
		_stageInputs.emplace("inColor", r2);

		vrg::DescriptorBinding binding;
		binding.set = 0;
		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		binding.stageFlags = _stage;
		binding.descriptorCount = 1;

		_descriptorBindings.emplace("ubo", binding);
	}

	//PrintSuccessMessage();
}


SpirvModule::SpirvModule(const vk::Device& device, const vk::ShaderStageFlagBits stage, const std::vector<uint32_t>& source, const std::string entryPoint)
	: _stage(stage), _device(device), _entryPoint(entryPoint) {

	vk::ShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.codeSize = source.size() * sizeof(uint32_t);
	moduleInfo.pCode = source.data();

	if (_device.createShaderModule(&moduleInfo, nullptr, &_module) != vk::Result::eSuccess) {
		//errf_color(ConsoleColor::Red, "Failed!\,");
		throw std::runtime_error("Failed to create shader module");
	}

}
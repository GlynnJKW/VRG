#pragma once

#include "Util.hpp"
#include "ShaderDefines.h"

namespace vrg {

	enum VertexAttributeType {
		Position = VAT_POSITION,
		Normal = VAT_NORMAL,
		Color = VAT_COLOR,
		TexCoord = VAT_TEXCOORD,
		SystemValue = VAT_SYSTEMVALUE
	};

	struct DescriptorBinding {
		uint32_t set = 0;
		uint32_t binding = 0;
		uint32_t descriptorCount = 0;
		vk::DescriptorType descriptorType = vk::DescriptorType::eSampler;
		vk::ShaderStageFlags stageFlags = {};
	};

	struct VertexAttributeId {
		VertexAttributeType type;
		uint32_t typeIndex;
		bool operator==(const VertexAttributeId&) const = default;
	};

	struct RasterStageVariable {
		uint32_t location;
		vk::Format format;
		VertexAttributeId attributeId;
	};


	class SpirvModule {
	public:
		vk::ShaderModule _module;
		vk::Device _device;

		vk::ShaderStageFlagBits _stage;
		std::vector<char> _spirv;
		std::string _entryPoint;

		std::unordered_map<std::string, DescriptorBinding> _descriptorBindings;
		std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> _pushConstants;
		std::unordered_map<std::string, RasterStageVariable> _stageInputs;
		std::unordered_map<std::string, RasterStageVariable> _stageOutputs;


		SpirvModule(const vk::Device& device, const vk::ShaderStageFlagBits stage, const std::string filename, const std::string entryPoint = "main");
		SpirvModule(const vk::Device& device, const vk::ShaderStageFlagBits stage, const std::vector<uint32_t>& source, const std::string entryPoint = "main");

		inline ~SpirvModule() {
			_device.destroyShaderModule(_module);
		}
	};
}

template<> struct std::hash<vrg::VertexAttributeId> {
	inline size_t operator()(const vrg::VertexAttributeId& v) const {
		return vrg::hash_combine(v.type, v.typeIndex);
	}
};

template<> struct std::hash<vrg::RasterStageVariable> {
	inline size_t operator()(const vrg::RasterStageVariable& r) const {
		return vrg::hash_combine(r.attributeId, r.format, r.location);
	}
};

static_assert(vrg::Hashable<vrg::VertexAttributeId>);
static_assert(vrg::Hashable<vrg::RasterStageVariable>);

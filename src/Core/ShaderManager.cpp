#include "ShaderManager.hpp"
#include "spirv_cross/spirv_cross.hpp"

using namespace vrg;

static const std::unordered_map<spirv_cross::SPIRType::BaseType, std::vector<vk::Format>> formatMap{
	{ spirv_cross::SPIRType::BaseType::SByte, 	{ vk::Format::eR8Snorm, 	vk::Format::eR8G8Snorm, 	vk::Format::eR8G8B8Snorm, 			vk::Format::eR8G8B8A8Snorm } },
	{ spirv_cross::SPIRType::BaseType::UByte, 	{ vk::Format::eR8Unorm, 	vk::Format::eR8G8Unorm, 	vk::Format::eR8G8B8Unorm, 			vk::Format::eR8G8B8A8Unorm } },
	{ spirv_cross::SPIRType::BaseType::Short, 	{ vk::Format::eR16Sint, 	vk::Format::eR16G16Sint, 	vk::Format::eR16G16B16Sint, 		vk::Format::eR16G16B16A16Sint } },
	{ spirv_cross::SPIRType::BaseType::UShort, 	{ vk::Format::eR16Uint, 	vk::Format::eR16G16Uint, 	vk::Format::eR16G16B16Uint, 		vk::Format::eR16G16B16A16Uint } },
	{ spirv_cross::SPIRType::BaseType::Int, 	{ vk::Format::eR32Sint, 	vk::Format::eR32G32Sint, 	vk::Format::eR32G32B32Sint, 		vk::Format::eR32G32B32A32Sint } },
	{ spirv_cross::SPIRType::BaseType::UInt, 	{ vk::Format::eR32Uint, 	vk::Format::eR32G32Uint, 	vk::Format::eR32G32B32Uint, 		vk::Format::eR32G32B32A32Uint } },
	{ spirv_cross::SPIRType::BaseType::Int64, 	{ vk::Format::eR64Sint, 	vk::Format::eR64G64Sint, 	vk::Format::eR64G64B64Sint, 		vk::Format::eR64G64B64A64Sint } },
	{ spirv_cross::SPIRType::BaseType::UInt64, 	{ vk::Format::eR32Uint, 	vk::Format::eR32G32Uint, 	vk::Format::eR32G32B32Uint, 		vk::Format::eR32G32B32A32Uint } },
	{ spirv_cross::SPIRType::BaseType::Half, 	{ vk::Format::eR16Sfloat,	vk::Format::eR16G16Sfloat,	vk::Format::eR16G16B16Sfloat,		vk::Format::eR16G16B16A16Sfloat } },
	{ spirv_cross::SPIRType::BaseType::Float, 	{ vk::Format::eR32Sfloat,	vk::Format::eR32G32Sfloat,	vk::Format::eR32G32B32Sfloat,		vk::Format::eR32G32B32A32Sfloat } },
	{ spirv_cross::SPIRType::BaseType::Double, 	{ vk::Format::eR64Sfloat,	vk::Format::eR64G64Sfloat,	vk::Format::eR64G64B64Sfloat,		vk::Format::eR64G64B64A64Sfloat } }

};

static const std::unordered_map<std::string, VertexAttributeType> semanticMap = {
	{ "position", VertexAttributeType::Position },
	{ "sv_position", VertexAttributeType::Position },
	{ "normal", VertexAttributeType::Normal },
	{ "color", VertexAttributeType::Color }
};

static const std::unordered_map<vk::ShaderStageFlagBits, shaderc_shader_kind> stageMap{
	{vk::ShaderStageFlagBits::eVertex, shaderc_vertex_shader},
	{vk::ShaderStageFlagBits::eFragment, shaderc_fragment_shader},
	{vk::ShaderStageFlagBits::eCompute, shaderc_compute_shader},
	{vk::ShaderStageFlagBits::eGeometry, shaderc_geometry_shader},
	{vk::ShaderStageFlagBits::eTessellationControl, shaderc_tess_control_shader},
	{vk::ShaderStageFlagBits::eTessellationEvaluation, shaderc_tess_evaluation_shader}
};


VertexAttributeId semanticToAttribute(const std::string& semantic) {
	printf("%s", semantic.c_str());
	VertexAttributeId id;
	size_t indexPos = semantic.find_first_of("0123456789");
	if (indexPos != std::string::npos) {
		id.typeIndex = std::atoi(semantic.c_str() + indexPos);
	}
	else {
		id.typeIndex = 0;
		indexPos = semantic.length();
	}

	if (indexPos > 0 && semantic[indexPos - 1] == '_') {
		--indexPos;
	}
	std::string semanticType(semantic, 0, indexPos); //Isolate semantic type from index - EG POSITION from POSITION0 or POSITION_0
	std::transform(semanticType.begin(), semanticType.end(), semanticType.begin(), [](auto c) { return std::tolower(c); });
	auto it = semanticMap.find(semanticType);
	if (it != semanticMap.end()) {
		id.type = it->second;
	}
	else {
		if (semanticType.starts_with("sv_")) {
			id.type = VertexAttributeType::SystemValue;
		}
		else {
			id.type = VertexAttributeType::TexCoord;
		}
	}
	return id;
}

std::shared_ptr<SpirvModule> ShaderManager::FetchFile(const std::string& filename, vk::ShaderStageFlagBits stage, ShaderCompileOptions options) {
	auto it = _shaders.find(options);
	if (it != _shaders.end()) {
		errf_color(ConsoleColor::Yellow, "Shader %s already exists with options {}. Returning existing copy");
		return it->second;
	}
	else {
		auto spirv = _compiler.CompileToSpirv(filename, stageMap.at(stage), options, false);
		auto shader = Fetch(spirv, stage);
		_shaders.emplace(options, shader);
		return shader;
	}
}

std::shared_ptr<SpirvModule> ShaderManager::Fetch(const std::vector<uint32_t>& source, vk::ShaderStageFlagBits stage) {

	auto spvmod = std::make_shared<SpirvModule>(_device, stage, source);

	spirv_cross::Compiler shadersource(source);

	spirv_cross::ShaderResources resources = shadersource.get_shader_resources();

	for (auto& resource : resources.stage_inputs) {
		RasterStageVariable r;
		r.location = shadersource.get_decoration(resource.id, spv::DecorationLocation);
		auto type = shadersource.get_type(resource.type_id);
		r.format = formatMap.at(type.basetype)[type.vecsize - 1];
		r.attributeId = semanticToAttribute(shadersource.get_decoration_string(resource.id, spv::DecorationHlslSemanticGOOGLE));

		auto lastdot = resource.name.find_last_of('.');
		auto cleanname = resource.name.substr(lastdot + 1);

		spvmod->_stageInputs.emplace(cleanname, r);
		printf("Found input \"%s\" at location %d\n", cleanname.c_str(), r.location);
	}

	for (auto& resource : resources.stage_outputs) {
		RasterStageVariable r;
		r.location = shadersource.get_decoration(resource.id, spv::DecorationLocation);
		auto type = shadersource.get_type(resource.type_id);
		r.format = formatMap.at(type.basetype)[type.vecsize - 1];
		r.attributeId = semanticToAttribute(shadersource.get_decoration_string(resource.id, spv::DecorationHlslSemanticGOOGLE));

		auto lastdot = resource.name.find_last_of('.');
		auto cleanname = resource.name.substr(lastdot + 1);

		spvmod->_stageOutputs.emplace(cleanname, r);
		printf("Found output \"%s\" at location %d\n", cleanname.c_str(), r.location);
	}

	for (auto& resource : resources.uniform_buffers) {
		unsigned set = shadersource.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = shadersource.get_decoration(resource.id, spv::DecorationBinding);

		vrg::DescriptorBinding db;
		db.set = set;
		db.binding = binding;
		db.descriptorType = vk::DescriptorType::eUniformBuffer;
		db.stageFlags = stage;
		db.descriptorCount = 1;

		spvmod->_descriptorBindings.emplace(resource.name.c_str(), db);
		printf("Found UBO \"%s\" at set:binding %d:%d\n", resource.name.c_str(), set, binding);
	}

	for (auto& resource : resources.storage_buffers) {
		auto name = shadersource.get_name(resource.id);
		printf("Found storage buffer \"%s\"", resource.name.c_str());
	}

	for (auto& resource : resources.subpass_inputs) {
		auto name = shadersource.get_name(resource.id);
		printf("Found subpass input \"%s\"", resource.name.c_str());
	}

	for (auto& resource : resources.push_constant_buffers) {
		auto name = shadersource.get_name(resource.id);
		printf("Found push constant buffer \"%s\"", resource.name.c_str());
	}

	return spvmod;
	//return "";
}

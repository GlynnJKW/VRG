#pragma once

#include "RenderPass.hpp"
#include "Geometry.hpp"
#include "DescriptorSet.hpp"

namespace vrg {

	class Pipeline : public DeviceResource {
	protected:
		std::vector<std::shared_ptr<SpirvModule>> _modules;
		std::vector<vk::PipelineShaderStageCreateInfo> _stages;
		std::vector<std::shared_ptr<const DescriptorSetLayout>> _descriptorSetLayouts;
		std::unordered_map<std::string, DescriptorBinding> _descriptorBindings;
		std::unordered_map<std::string, vk::PushConstantRange> _pushConstants;
		vk::PipelineLayout _layout;
		vk::Pipeline _pipeline;
		size_t _hash = 0;

		inline void createStages() {
			std::vector<vk::PushConstantRange> pushConstantRanges;
			for (auto spirv : _modules) {
				vk::PipelineShaderStageCreateInfo stageInfo = {};
				stageInfo.stage = spirv->_stage;
				stageInfo.module = spirv->_module;
				stageInfo.pName = spirv->_entryPoint.c_str();
				_stages.push_back(stageInfo);
				_hash = hash_combine(_hash, spirv);

				if (spirv->_pushConstants.size()) {
					uint32_t first = ~0;
					uint32_t last = 0;
					for (const auto& [id, pushConstant] : spirv->_pushConstants) {
						auto it = _pushConstants.find(id);
						if (it == _pushConstants.end()) {
							//pushconstant doesnt exist yet, create it
							_pushConstants.emplace(id, vk::PushConstantRange(spirv->_stage, pushConstant.first, pushConstant.second));
						}
						else {
							if (it->second.offset != pushConstant.first || it->second.size != pushConstant.second) {
								throw std::runtime_error("Spirv modules share push constant names with differents offsets and sizes");
							}
							first = std::min(first, pushConstant.first);
							last = std::max(last, pushConstant.first + pushConstant.second);
						}
						pushConstantRanges.emplace_back(spirv->_stage, first, last - first);
					}
				}

				for (const auto& [id, binding] : spirv->_descriptorBindings) {
					auto it = _descriptorBindings.find(id);
					if (it != _descriptorBindings.end()) {
						if (it->second.binding == binding.binding && it->second.set == binding.set) {
							if (it->second.descriptorType != binding.descriptorType) throw std::invalid_argument("modules share descriptor binding with different descriptor types");
							it->second.stageFlags |= binding.stageFlags;
							it->second.descriptorCount = std::max(it->second.descriptorCount, binding.descriptorCount);
						}
						else {
							_descriptorBindings[std::to_string(binding.set) + "." + std::to_string(binding.binding) + id] = binding;
						}
					}
					else {
						_descriptorBindings[id] = binding;
					}
				}
			}

			if (_descriptorSetLayouts.empty()) {
				std::unordered_map<uint32_t, std::unordered_map<uint32_t, DescriptorSetLayout::Binding>> setBindings;
				for (const auto& [name, binding] : _descriptorBindings) {
					if (binding.set >= _descriptorSetLayouts.size()) _descriptorSetLayouts.resize(binding.set + 1); //if set is out of range, expand vector to include set
					setBindings[binding.set][binding.binding] = DescriptorSetLayout::Binding(binding.descriptorType, binding.stageFlags, binding.descriptorCount);
					//todo: immutable samplers
				}
				for (uint32_t set = 0; set < _descriptorSetLayouts.size(); ++set) {
					if (setBindings.count(set)) {
						_descriptorSetLayouts[set] = std::make_shared<DescriptorSetLayout>(_device, Name(), setBindings.at(set));
					}
					else {
						_descriptorSetLayouts[set] = std::make_shared<DescriptorSetLayout>(_device, Name());
					}
				}
			}

			//for (const auto& layout : _descriptorSetLayouts) {
			//	for (const auto& [index, binding] : layout->Bindings()) {
			//		for (const auto& sampler : binding.immutableSamplers) {
			//			_hash = hash_combine(_hash, sampler->create_info());
			//		}
			//	}
			//}

			std::vector<vk::DescriptorSetLayout> tmp(_descriptorSetLayouts.size());
			std::ranges::transform(_descriptorSetLayouts, tmp.begin(), &DescriptorSetLayout::operator*);

			auto layoutInfo = vk::PipelineLayoutCreateInfo({}, tmp, pushConstantRanges);
			if (_device->createPipelineLayout(&layoutInfo, nullptr, &_layout) != vk::Result::eSuccess) {
				//errf_color(ConsoleColor::Red, "Failed to create pipeline layout");
				throw std::runtime_error("Failed to create pipeline layout");
			}
		}


	public:

		inline Pipeline(vrg::Device& device, std::string name, std::vector<std::shared_ptr<SpirvModule>>& modules)
			: DeviceResource(device, name), _modules(modules) {
			createStages();
		}

		inline Pipeline(vrg::Device& device, std::string name, const std::vector<std::pair<vk::ShaderStageFlagBits, std::string>> modulepaths)
			: DeviceResource(device, name) {
			for (auto& stage : modulepaths) {
				_modules.push_back(std::make_shared<SpirvModule>(*_device, stage.first, stage.second));
			}
			createStages();
		}

		inline virtual ~Pipeline() {
			if (_layout) _device->destroyPipelineLayout(_layout);
			if (_pipeline) _device->destroyPipeline(_pipeline);
		}

		virtual vk::PipelineBindPoint BindPoint() const = 0;

		inline const vk::Pipeline& operator*() const { return _pipeline; }
		inline const vk::Pipeline* operator->() const { return &_pipeline; }
		inline operator bool() const { return _pipeline; }

		inline bool operator==(const Pipeline& rhs) const { return rhs._pipeline == _pipeline; }

		inline size_t Hash() const { return _hash; }
		inline const auto& SpirvModules() const { return _modules; }
		inline vk::PipelineLayout Layout() const { return _layout; }
		inline const auto& DescriptorSetLayouts() const { return _descriptorSetLayouts; }
		inline const auto& DescriptorBindings() const { return _descriptorBindings; }
		inline const auto& PushConstants() const { return _pushConstants; }

		inline const DescriptorBinding& Binding(const std::string& name) const {
			auto it = _descriptorBindings.find(name);
			if (it == _descriptorBindings.end()) {
				throw std::invalid_argument("No descriptor named " + name);
			}
			return it->second;
		}
	};

	class GraphicsPipeline : public Pipeline {
	private:
		std::vector<vk::PipelineColorBlendAttachmentState> _blendStates;
		vk::PipelineDepthStencilStateCreateInfo _depthStencilState;
		std::vector<vk::DynamicState> _dynamicStates;
		vk::PipelineInputAssemblyStateCreateInfo _inputAssemblyState;
		vk::PipelineMultisampleStateCreateInfo _multiSampleState;
		vk::PipelineRasterizationStateCreateInfo _rasterizationState;
		vk::PipelineViewportStateCreateInfo _viewportState;
		vk::CullModeFlags _cullMode;
		vk::PolygonMode _polygonMode;
		uint32_t _renderQueue;
		uint32_t _subpassIndex;

		inline virtual void createPipeline(const vrg::RenderPass& renderPass, const vrg::Geometry& geometry) {

			vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

			std::vector<vk::PipelineColorBlendAttachmentState> blendStates;
			blendStates.resize(_blendStates.size());
			
			for (const auto& [desc, tup] : renderPass.SubpassDescriptions()[_subpassIndex].attachmentDescriptions | std::views::values) {
				const auto type = tup.first;
				const auto blendState = tup.second;

				if (type == RenderPass::AttachmentType::Color || type == RenderPass::AttachmentType::DepthStencil) {
					sampleCount = desc.samples;
				}
				if (blendStates.empty() && type == RenderPass::AttachmentType::Color) {
					_blendStates.emplace_back(blendState);
				}
			}

			vk::GraphicsPipelineCreateInfo pipelineInfo = {};

#pragma region Vertex Input Attributes
			std::pair<std::vector<vk::VertexInputAttributeDescription>, std::vector<vk::VertexInputBindingDescription>> vertexInput = {};
			auto spirvit = std::ranges::find_if(_modules, [&](const std::shared_ptr<vrg::SpirvModule> m) { return m->_stage == vk::ShaderStageFlagBits::eVertex; });
			if (spirvit == _modules.end()) {
				throw std::runtime_error("Could not find vertex stage in graphics pipeline");
			}

			const vrg::SpirvModule& vs = **spirvit;
			for (auto& [id, attribute] : geometry.attributes) {
				auto it = std::ranges::find_if(vs._stageInputs, [&](const auto& p) { return p.second.attributeId == id; });
				if (it != vs._stageInputs.end()) {
					vertexInput.first.emplace_back(it->second.location, attribute.binding, attribute.format, (uint32_t)attribute.offset);
				}
			}
			vertexInput.second.resize(geometry.bindings.size());
			for (uint32_t i = 0; auto& [binding, buffer] : geometry.bindings) {
				vertexInput.second[i].binding = binding;
				vertexInput.second[i].stride = buffer.first.Stride();
				vertexInput.second[i].inputRate = buffer.second;
				++i;
			}
			std::ranges::sort(vertexInput.first, {}, &vk::VertexInputAttributeDescription::location);
#pragma endregion

			vk::PipelineVertexInputStateCreateInfo vertexInfo({}, vertexInput.second, vertexInput.first);

			_inputAssemblyState = { {}, geometry.primitiveTopology };
			_viewportState = { {}, 1, nullptr, 1, nullptr };
			_rasterizationState = { {}, false, false, _polygonMode, _cullMode };
			_multiSampleState = { {}, sampleCount, false };
			vk::PipelineColorBlendStateCreateInfo blendState({}, false, vk::LogicOp::eCopy, _blendStates);
			vk::PipelineDynamicStateCreateInfo dynamicState({}, _dynamicStates);

			pipelineInfo.stageCount = _stages.size();
			pipelineInfo.pStages = _stages.data();
			pipelineInfo.pInputAssemblyState = &_inputAssemblyState;
			pipelineInfo.pViewportState = &_viewportState;
			pipelineInfo.pRasterizationState = &_rasterizationState;
			pipelineInfo.pMultisampleState = &_multiSampleState;
			pipelineInfo.pDepthStencilState = &_depthStencilState;
			pipelineInfo.pColorBlendState = &blendState;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.pVertexInputState = &vertexInfo;
			pipelineInfo.layout = _layout;
			pipelineInfo.renderPass = *renderPass;
			pipelineInfo.subpass = _subpassIndex;

			pipelineInfo.basePipelineHandle = nullptr;
			pipelineInfo.basePipelineIndex = -1;

			vk::ResultValue<vk::Pipeline> result = _device->createGraphicsPipeline(nullptr, pipelineInfo);
			_pipeline = result.value;
			if(result.result != vk::Result::eSuccess) {
				//errf_color(ConsoleColor::Red, "Failed to create graphics pipeline");
				throw std::runtime_error("Failed to create graphics pipeline");
			}
			//PrintSuccessMessage();
		}

	public:
		inline GraphicsPipeline(vrg::Device& device, std::string name, const vrg::RenderPass& renderPass, std::vector<std::shared_ptr<SpirvModule>>& modules, const Geometry& geometry,
			uint32_t subpassIndex = 0, vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack, vk::PolygonMode polygonMode = vk::PolygonMode::eFill,
			const vk::PipelineDepthStencilStateCreateInfo& depthStencilState = { {}, true, true, vk::CompareOp::eLessOrEqual, {}, {}, {}, {}, 0, 1 },
			const std::vector<vk::PipelineColorBlendAttachmentState>& blendStates = {},
			const std::vector<vk::DynamicState>& dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eLineWidth })
			: Pipeline(device, name, modules), _subpassIndex(subpassIndex), _cullMode(cullMode), _polygonMode(polygonMode),
			_depthStencilState(depthStencilState), _blendStates(blendStates), _dynamicStates(dynamicStates) {
			createPipeline(renderPass, geometry);
		}
		inline GraphicsPipeline(vrg::Device& device, std::string name, const vrg::RenderPass& renderPass, const std::vector<std::pair<vk::ShaderStageFlagBits, std::string>> modulepaths, const Geometry& geometry,
			uint32_t subpassIndex = 0, vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack, vk::PolygonMode polygonMode = vk::PolygonMode::eFill,
			const vk::PipelineDepthStencilStateCreateInfo& depthStencilState = { {}, true, true, vk::CompareOp::eLessOrEqual, {}, {}, {}, {}, 0, 1 }, 
			const std::vector<vk::PipelineColorBlendAttachmentState>& blendStates = {},
			const std::vector<vk::DynamicState>& dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eLineWidth })
			: Pipeline(device, name, modulepaths), _subpassIndex(subpassIndex), _cullMode(cullMode), _polygonMode(polygonMode), 
			_depthStencilState(depthStencilState), _blendStates(blendStates), _dynamicStates(dynamicStates) {
			createPipeline(renderPass, geometry);
		}

		inline GraphicsPipeline(vrg::Device& device, std::string name, const vrg::RenderPass& renderPass, std::string vs, std::string ps, const Geometry& geometry,
			uint32_t subpassIndex = 0, vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack, vk::PolygonMode polygonMode = vk::PolygonMode::eFill,
			const vk::PipelineDepthStencilStateCreateInfo& depthStencilState = { {}, true, true, vk::CompareOp::eLessOrEqual, {}, {}, {}, {}, 0, 1 },
			const std::vector<vk::PipelineColorBlendAttachmentState>& blendStates = {},
			const std::vector<vk::DynamicState>& dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eLineWidth })
			: Pipeline(device, name, { {vk::ShaderStageFlagBits::eVertex, vs}, {vk::ShaderStageFlagBits::eFragment, ps} }), _subpassIndex(subpassIndex), _cullMode(cullMode), _polygonMode(polygonMode),
			_depthStencilState(depthStencilState), _blendStates(blendStates), _dynamicStates(dynamicStates) {
			createPipeline(renderPass, geometry);
		}


		inline vk::PipelineBindPoint BindPoint() const override { return vk::PipelineBindPoint::eGraphics; }
	};
}

template<> struct std::hash<vk::PipelineShaderStageCreateInfo> {
	inline size_t operator()(const vk::PipelineShaderStageCreateInfo& p) { return vrg::hash_combine(p.allowDuplicate, p.flags, p.module, p.pName, p.pNext, p.pSpecializationInfo, p.stage, p.structureType, p.sType); }
};
static_assert(vrg::Hashable<vk::PipelineShaderStageCreateInfo>);
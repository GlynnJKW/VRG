#pragma once
#include "SpirvModule.hpp"
#include "ShaderCompiler.hpp"

//This is dumb but it works


template<> struct std::hash<vrg::ShaderCompileOptions> {
	inline size_t operator()(const vrg::ShaderCompileOptions& o) const {
		return vrg::hash_combine(o.name);
	}
};

static_assert(vrg::Hashable<vrg::ShaderCompileOptions>);

namespace vrg {
	class ShaderManager {
	private:
		vk::Device _device;
		std::unordered_map<ShaderCompileOptions, std::shared_ptr<SpirvModule>> _shaders;
	public:
		//File must be in SPIRV format
		std::shared_ptr<SpirvModule> FetchFile(const std::string& filename, vk::ShaderStageFlagBits stage, ShaderCompileOptions options);
		std::shared_ptr<SpirvModule> Fetch(const std::vector<uint32_t>& source, vk::ShaderStageFlagBits stage);

		inline std::shared_ptr<SpirvModule> Get(ShaderCompileOptions options) {
			auto it = _shaders.find(options);
			if (it != _shaders.end()) {
				return it->second;
			}
			else {
				throw std::runtime_error("Shader with specified options does not exist");
			}
		}
		ShaderCompiler _compiler;

		inline ShaderManager(const vk::Device& device)
			: _device(device) {}

		inline ~ShaderManager() {
			_shaders.clear();
		}

	};
}

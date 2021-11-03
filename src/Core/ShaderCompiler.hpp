#pragma once
#include <shaderc/shaderc.hpp>
#include <unordered_map>

namespace vrg {
	struct ShaderCompileOptions {
		std::string name;
		bool operator==(const ShaderCompileOptions& rhs) const {
			return name == rhs.name;
		};
	};

	class ShaderCompiler {
	private:
		std::unordered_map<std::string, size_t> _filehashes;
	public:

		// Compile to spirv
		// Returns filepath of spirv file
		// If force is false, check stored filename hash to see if file has been changed since last compile
		std::vector<uint32_t> CompileToSpirv(const std::string& filename, shaderc_shader_kind stage, ShaderCompileOptions options, bool force = false);
		std::string CompileToSpirvText(const std::string& filename, shaderc_shader_kind stage, ShaderCompileOptions options, bool force = false);
	};
}


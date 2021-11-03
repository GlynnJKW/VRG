#include "ShaderCompiler.hpp"
#include "FileIncluder.hpp"
#include <iostream>

using namespace vrg;


std::vector<uint32_t> ShaderCompiler::CompileToSpirv(const std::string& filename, shaderc_shader_kind stage, ShaderCompileOptions options, bool force) {
	bool hashcompare = true;
	//Get file contents
	std::ifstream f(filename);
	std::stringstream buf;
	buf << f.rdbuf();

	if (!force) {
		//Check file hash
		size_t newhash = std::hash<std::string>{}(buf.str());
		auto it = _filehashes.find(filename);
		if (it != _filehashes.end()) {
			hashcompare = it->second == newhash;
		}
	}

	if (hashcompare) {
		shaderc::Compiler compiler;
		shaderc::CompileOptions compileoptions;
		compileoptions.SetSourceLanguage(shaderc_source_language::shaderc_source_language_hlsl);
		compileoptions.SetHlslFunctionality1(true);
		compileoptions.SetTargetSpirv(shaderc_spirv_version::shaderc_spirv_version_1_5);
		compileoptions.SetIncluder(std::make_unique<FileIncluder>(new FileFinder()));
		//compileoptions.SetHlslIoMapping(true);
		compileoptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
		shaderc::SpvCompilationResult compiled;
		try {
			compiled = compiler.CompileGlslToSpv(buf.str(), stage, filename.c_str(), compileoptions);
		}
		catch (const std::exception& e) {
			errf_color(ConsoleColor::Red, e.what());
			return std::vector<uint32_t>();
		}

		if (compiled.GetCompilationStatus() != shaderc_compilation_status_success) {
			errf_color(ConsoleColor::Red, compiled.GetErrorMessage().c_str());
			return std::vector<uint32_t>();
		}

		return { compiled.cbegin(), compiled.cend() };
	}
}

std::string ShaderCompiler::CompileToSpirvText(const std::string& filename, shaderc_shader_kind stage, ShaderCompileOptions options, bool force) {
	bool hashcompare = true;
	//Get file contents
	std::ifstream f(filename);
	std::stringstream buf;
	buf << f.rdbuf();

	if (!force) {
		//Check file hash
		size_t newhash = std::hash<std::string>{}(buf.str());
		auto it = _filehashes.find(filename);
		if (it != _filehashes.end()) {
			hashcompare = it->second == newhash;
		}
	}

	if (hashcompare) {
		shaderc::Compiler compiler;
		shaderc::CompileOptions compileoptions;
		compileoptions.SetSourceLanguage(shaderc_source_language::shaderc_source_language_hlsl);
		compileoptions.SetHlslFunctionality1(true);
		compileoptions.SetTargetSpirv(shaderc_spirv_version::shaderc_spirv_version_1_5);
		compileoptions.SetIncluder(std::make_unique<FileIncluder>(new FileFinder()));
		compileoptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
		shaderc::AssemblyCompilationResult compiled;
		try {
			compiled = compiler.CompileGlslToSpvAssembly(buf.str(), stage, filename.c_str(), compileoptions);
		}
		catch (const std::exception& e) {
			errf_color(ConsoleColor::Red, e.what());
			return "";
		}

		if (compiled.GetCompilationStatus() != shaderc_compilation_status_success) {
			errf_color(ConsoleColor::Red, compiled.GetErrorMessage().c_str());
			return "";
		}

		return { compiled.cbegin(), compiled.cend() };
	}
}
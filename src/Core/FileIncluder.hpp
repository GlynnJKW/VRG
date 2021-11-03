#pragma once

#include <shaderc/shaderc.hpp>
#include "Util.hpp"

//cpp implementation of FileIncluder from shaderc_util

namespace vrg {

	class FileFinder {
	public:
		std::string FindReadableFilepath(const std::string& filename) const;

		std::string FindRelativeReadableFilepath(const std::string& requesting_file, const std::string& filename) const;

		std::vector<std::string>& SearchPath() { return _searchPath; }

	private:
		std::vector<std::string> _searchPath;
	};

	class FileIncluder : public shaderc::CompileOptions::IncluderInterface {
	public:
		explicit FileIncluder(const FileFinder* fileFinder) :
			_fileFinder(*fileFinder) {}

		~FileIncluder() override;

		shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override;

		void ReleaseInclude(shaderc_include_result* include_result) override;

		const std::unordered_set<std::string>& filepathTrace() const {
			return _includedFiles;
		}

	private:
		const FileFinder& _fileFinder;
		struct FileInfo {
			const std::string fullPath;
			std::vector<char> contents;
		};

		std::unordered_set<std::string> _includedFiles;
	};
}
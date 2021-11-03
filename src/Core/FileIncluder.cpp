#include "FileIncluder.hpp"
#include "StringPiece.hpp"

#include <iostream>
#include <io.h>
#define R_OK 4

using namespace vrg;



// Returns "" if path is empty or ends in '/'.  Otherwise, returns "/".
std::string MaybeSlash(const string_piece& path) {
	return (path.empty() || path.back() == '/') ? "" : "/";
}

void OutputFileErrorMessage(int errno_value) {
#ifdef _MSC_VER
	// If the error message is more than 1023 bytes it will be truncated.
	char buffer[1024];
	strerror_s(buffer, errno_value);
	std::cerr << ": " << buffer << std::endl;
#else
	std::cerr << ": " << strerror(errno_value) << std::endl;
#endif
}

bool ReadFile(const std::string& input_file_name,
	std::vector<char>* input_data) {
	std::istream* stream = &std::cin;
	std::ifstream input_file;
	if (input_file_name != "-") {
		input_file.open(input_file_name, std::ios_base::binary);
		stream = &input_file;
		if (input_file.fail()) {
			std::cerr << "glslc: error: cannot open input file: '" << input_file_name
				<< "'";
			if (_access(input_file_name.c_str(), R_OK) != 0) {
				OutputFileErrorMessage(errno);
				return false;
			}
			std::cerr << std::endl;
			return false;
		}
	}
	*input_data = std::vector<char>((std::istreambuf_iterator<char>(*stream)),
		std::istreambuf_iterator<char>());
	return true;
}


namespace vrg {

	std::string FileFinder::FindReadableFilepath(
		const std::string& filename) const {
		assert(!filename.empty());
		static const auto for_reading = std::ios_base::in;
		std::filebuf opener;
		for (const auto& prefix : _searchPath) {
			const std::string prefixed_filename =
				prefix + MaybeSlash(prefix) + filename;
			if (opener.open(prefixed_filename, for_reading)) return prefixed_filename;
		}
		return "";
	}

	std::string FileFinder::FindRelativeReadableFilepath(
		const std::string& requesting_file, const std::string& filename) const {
		assert(!filename.empty());

		string_piece dir_name(requesting_file);

		size_t last_slash = requesting_file.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			dir_name = string_piece(requesting_file.c_str(),
				requesting_file.c_str() + last_slash);
		}

		if (dir_name.size() == requesting_file.size()) {
			dir_name.clear();
		}

		static const auto for_reading = std::ios_base::in;
		std::filebuf opener;
		const std::string relative_filename =
			dir_name.str() + MaybeSlash(dir_name) + filename;
		if (opener.open(relative_filename, for_reading)) return relative_filename;

		return FindReadableFilepath(filename);
	}

	shaderc_include_result* MakeErrorIncludeResult(const char* message) {
		return new shaderc_include_result{ "", 0, message, strlen(message) };
	}

	FileIncluder::~FileIncluder() = default;

	shaderc_include_result* FileIncluder::GetInclude(
		const char* requested_source, shaderc_include_type include_type,
		const char* requesting_source, size_t) {

		const std::string full_path =
			(include_type == shaderc_include_type_relative)
			? _fileFinder.FindRelativeReadableFilepath(requesting_source,
				requested_source)
			: _fileFinder.FindReadableFilepath(requested_source);

		if (full_path.empty())
			return MakeErrorIncludeResult("Cannot find or open include file.");

		// In principle, several threads could be resolving includes at the same
		// time.  Protect the included_files.

		// Read the file and save its full path and contents into stable addresses.
		FileInfo* new_file_info = new FileInfo{ full_path, {} };
		if (!ReadFile(full_path, &(new_file_info->contents))) {
			return MakeErrorIncludeResult("Cannot read file");
		}

		_includedFiles.insert(full_path);

		return new shaderc_include_result{
			new_file_info->fullPath.data(), new_file_info->fullPath.length(),
			new_file_info->contents.data(), new_file_info->contents.size(),
			new_file_info };
	}

	void FileIncluder::ReleaseInclude(shaderc_include_result* include_result) {
		FileInfo* info = static_cast<FileInfo*>(include_result->user_data);
		delete info;
		delete include_result;
	}

}
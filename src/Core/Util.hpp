#pragma once

#include "hash_combine.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

#include <Windows.h>

#undef MemoryBarrier

#define ENUM_OP(opspec) template<typename T> static inline \
		T operator opspec(const T& lhs, const T& rhs) { \
		using B = typename std::underlying_type_t<T>; \
		return static_cast<T>(static_cast<B>(lhs) | static_cast<B>(rhs)); }
#define ENUM_ASSIGN_OP(opspec) template<typename T> static inline \
		T& operator opspec ## =(T& lhs, const T& rhs) { \
		lhs = lhs opspec rhs; return lhs; }
#define ENUM_OPS(opspec) ENUM_OP(opspec) ENUM_ASSIGN_OP(opspec)

// Provides |, |= on enums
ENUM_OPS(| )
// Provides &, &= on enums
ENUM_OPS(&)

//returns empty vec on error
template<typename T>
inline std::vector<T> UtilReadFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) return {};
	std::vector<T> dst;
	dst.resize((size_t)file.tellg() / sizeof(T), '\0');
	if (dst.empty()) return dst;
	file.seekg(0);
	file.clear();
	file.read(reinterpret_cast<char*>(dst.data()), dst.size() * sizeof(T));
	return dst;
}

//Console color implementation taken from https://github.com/Shmaug/Stratum/blob/af0580f2f849b533f74aaf7f39e9b7fd81705357/src/Stratum.hpp
const enum ConsoleColor {
	Black = 0,
	Red = 1,
	Green = 2,
	Blue = 4,
	Bold = 8,
	Yellow = 3,
	Cyan = 6,
	Magenta = 5,
	White = 7,
};

template<typename... Args>
inline void fprintf_color(ConsoleColor color, FILE* file, const char* format, Args&&... a) {
	int c = 0;
	if ((color & (int)ConsoleColor::Red)!= 0) {
		c |= FOREGROUND_RED;
	}
	if ((color & (int)ConsoleColor::Green) != 0) {
		c |= FOREGROUND_GREEN;
	}
	if ((color & (int)ConsoleColor::Blue) != 0) {
		c |= FOREGROUND_BLUE;
	}
	if ((color & (int)ConsoleColor::Bold) != 0) {
		c |= FOREGROUND_INTENSITY;
	}

	if (file == stdin)  SetConsoleTextAttribute(GetStdHandle(STD_INPUT_HANDLE), c);
	else if (file == stdout) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
	else if (file == stderr) SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), c);

	fprintf(file, format, std::forward<Args>(a)...);

	if (file == stdin)  SetConsoleTextAttribute(GetStdHandle(STD_INPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	else if (file == stdout) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	else if (file == stderr) SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

template<typename... Args> inline void printf_color(ConsoleColor color, const char* format, Args&&... a) { fprintf_color(color, stdout, format, std::forward<Args>(a)...); }
template<typename... Args> inline void errf_color(ConsoleColor color, const char* format, Args&&... a) { fprintf_color(color, stderr, format, std::forward<Args>(a)...); }
inline void PrintSuccessMessage(bool newline = true) { printf_color(ConsoleColor::Green, newline ? "Done!\n" : "Done!"); }

inline constexpr uint32_t verts_per_prim(vk::PrimitiveTopology topo) {
	switch (topo) {
	default:
	case vk::PrimitiveTopology::ePatchList:
	case vk::PrimitiveTopology::ePointList:
		return 1;
	case vk::PrimitiveTopology::eLineList:
	case vk::PrimitiveTopology::eLineStrip:
	case vk::PrimitiveTopology::eLineListWithAdjacency:
	case vk::PrimitiveTopology::eLineStripWithAdjacency:
		return 2;
	case vk::PrimitiveTopology::eTriangleList:
	case vk::PrimitiveTopology::eTriangleStrip:
	case vk::PrimitiveTopology::eTriangleFan:
	case vk::PrimitiveTopology::eTriangleListWithAdjacency:
	case vk::PrimitiveTopology::eTriangleStripWithAdjacency:
		return 3;
	}
}

inline constexpr vk::IndexType stride_to_index_type(vk::DeviceSize stride) {
	if (stride == sizeof(uint32_t)) {
		return vk::IndexType::eUint32;
	}
	else if (stride == sizeof(uint16_t)) {
		return vk::IndexType::eUint16;
	}
	else if (stride == sizeof(uint8_t)) {
		return vk::IndexType::eUint8EXT;
	}
	else {
		throw std::runtime_error("Invalid stride for index buffer: " + std::to_string(stride));
		return vk::IndexType::eNoneKHR;
	}
}

inline uint64_t to_set_binding(uint32_t binding, uint32_t index) {
	return (uint64_t(binding) << 32 | index);
}

inline std::pair<uint32_t, uint32_t> from_set_binding(uint64_t setBinding) {
	uint32_t binding = setBinding >> 32;
	uint32_t index = setBinding & uint64_t(~uint32_t(0));
	return { binding, index };
}
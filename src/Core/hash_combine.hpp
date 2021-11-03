#pragma once

//taken from https://github.com/Shmaug/Stratum/blob/src/hash_combine.hpp

#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <optional>
#include <bit>
#include <string>
#include <variant>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <ranges>
#include <span>
#include <fstream>
#include <random>

#include <bitset>
#include <locale>

#define WIN32_LEAN_AND_MEAN
#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
#define NOMINMAX
#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "vulkan/vulkan.hpp"

#include "vk_mem_alloc.h"


namespace vrg {

	template<typename T> 
	concept Hashable = requires(T a) { 
		{ std::hash<T>{}(a) } -> std::convertible_to<size_t>; 
	};
	
	template<typename T, typename... Rest>
	inline size_t hash_combine(const T& t, const Rest&... rest) {
		std::hash<T> hasher;
		size_t x = hasher(t);
		size_t y;
		if constexpr (sizeof...(rest) == 0)
			return x;
		else
			y = hash_combine(rest...);
		return x ^ (y + 0x9e3779b9 + (x << 6) + (x >> 2));
	}

	template<typename _Tuple, size_t... Inds>
	inline size_t hash_tuple(const _Tuple& t, std::index_sequence<Inds...>) {
		return hash_combine(get<Inds>(t)...);
	}

	template<typename _Tuple>
	inline size_t hash_tuple(const _Tuple& t) {
		return hash_tuple(t, std::make_index_sequence<tuple_size_v<_Tuple>-1>());
	}

	// accepts string literals
	template<typename T, size_t N>
	constexpr size_t hash_array(const T(&arr)[N], size_t n = N - 1) {
		return (n <= 1) ? hash_combine(arr[0]) : hash_combine(hash_array(arr, n - 1), arr[n - 1]);
	}

}

namespace std {

	template<vrg::Hashable Tx, vrg::Hashable Ty>
	struct hash<pair<Tx, Ty>> {
		constexpr size_t operator()(const pair<Tx, Ty>& p) const { return vrg::hash_combine(p.first, p.second); }
	};

	template<vrg::Hashable... Args>
	struct hash<tuple<Args...>> {
		constexpr size_t operator()(const tuple<Args...>& t) const { return vrg::hash_tuple(t); }
	};

	template<vrg::Hashable T, size_t N> struct hash<std::array<T, N>> {
		constexpr size_t operator()(const std::array<T, N>& a) const { return vrg::hash_array<T, N>(a.data()); }
	};

	template<ranges::range R> requires(vrg::Hashable<ranges::range_value_t<R>>)
		struct hash<R> {
		inline size_t operator()(const R& r) const {
			size_t h = 0;
			for (const auto& i : r) h = vrg::hash_combine(h, i);
			return h;
		}
	};

	template<typename BitType> requires(convertible_to<typename vk::Flags<BitType>::MaskType, size_t>)
		struct hash<vk::Flags<BitType>> {
		inline size_t operator()(vk::Flags<BitType> rhs) const {
			return (typename vk::Flags<BitType>::MaskType)rhs;
		}
	};

	template<>
	struct hash<vk::ComponentMapping> {
		inline size_t operator()(vk::ComponentMapping rhs) const {
			return vrg::hash_combine(rhs.r, rhs.g, rhs.b, rhs.a);
		}
	};

	static_assert(vrg::Hashable<size_t>);
	static_assert(vrg::Hashable<string>);
	//static_assert(vrg::Hashable<vector<seed_seq::result_type>>);
	//static_assert(vrg::Hashable<std::unordered_map<std::string, vk::SpecializationMapEntry>>);
	static_assert(vrg::Hashable<vk::DeviceSize>);
	static_assert(vrg::Hashable<vk::Format>);
	static_assert(vrg::Hashable<vk::SampleCountFlags>);
	static_assert(vrg::Hashable<vk::SampleCountFlagBits>);
	static_assert(vrg::Hashable<vk::ShaderStageFlagBits>);
	static_assert(vrg::Hashable<vk::ComponentMapping>);
}
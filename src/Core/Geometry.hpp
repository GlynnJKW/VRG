#pragma once

#include "Buffer.hpp"
#include "SpirvModule.hpp"

namespace vrg {

	struct Geometry {
		struct Attribute {
			uint32_t binding;
			vk::Format format;
			vk::DeviceSize offset;
		};

		vk::PrimitiveTopology primitiveTopology;
		std::unordered_map<uint32_t, std::pair<vrg::Buffer::StrideView, vk::VertexInputRate>> bindings;
		std::unordered_map<VertexAttributeId, Attribute> attributes;

		struct VertexAttributeArrayView {
			Geometry& attributes;
			VertexAttributeType type;
			inline Attribute& at(uint32_t index) { return attributes[VertexAttributeId(type, index)]; }
			inline Attribute& operator[](uint32_t index) { return  attributes[VertexAttributeId(type, index)]; }
		};

		inline Attribute& operator[](const VertexAttributeId& id) { return attributes[id]; }
		inline VertexAttributeArrayView operator[](const VertexAttributeType& type) { return VertexAttributeArrayView(*this, type); }
		inline Attribute& at(const VertexAttributeId& id) { return attributes.at(id); }
		inline Attribute& at(const VertexAttributeType& type, uint32_t index = 0) { return attributes.at(VertexAttributeId(type, index)); }

	};

}
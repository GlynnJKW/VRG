#pragma once

#include "Geometry.hpp"
#include "CommandBuffer.hpp"

namespace vrg {

	class Mesh {
	public:
		struct Submesh {
			uint32_t primitiveCount;
			uint32_t firstVertex;
			uint32_t firstIndex;
		};

		static std::shared_ptr<Mesh> Cube(CommandBuffer& commandBuffer);

		inline Mesh(const std::string& name, const Geometry& geometry = { vk::PrimitiveTopology::eTriangleList })
			: _name(name), _geometry(geometry) {}

		inline Buffer::StrideView& Indices() { return _indices; }
		inline vrg::Geometry& Geometry() { return _geometry; }
		inline std::vector<Submesh>& Submeshes() { return _submeshes; }

		inline uint32_t Index(uint32_t i, uint32_t baseIndex = 0, uint32_t baseVertex = 0) const {
			if (_indices) {
				std::byte* address = _indices.Data() + _indices.Stride() * (baseIndex + i);
				switch (_indices.Stride()) {
				default:
					case sizeof(uint32_t) : return baseVertex + *reinterpret_cast<uint32_t*>(address);
						case sizeof(uint16_t) : return baseVertex + *reinterpret_cast<uint16_t*>(address);
							case sizeof(uint8_t) : return baseVertex + *reinterpret_cast<uint8_t*>(address);
				}
			}
			else {
				return baseVertex + i;
			}
		}

		inline uint32_t operator[](uint32_t i) const {
			return Index(i);
		}

		inline void Draw(CommandBuffer& commandBuffer, uint32_t instanceCount = 1, uint32_t firstInstance = 0) {
			auto pipeline = std::dynamic_pointer_cast<GraphicsPipeline>(commandBuffer.BoundPipeline());
			if (!pipeline) throw std::runtime_error("Cannot draw a mesh without a graphics pipeline bound");

			for (auto& [binding, buffer] : _geometry.bindings) {
				commandBuffer.BindVertexBuffer(binding, buffer.first);
			}
			if (_indices) {
				commandBuffer.BindIndexBuffer(_indices);
			}

			for (const Submesh& s : _submeshes) {
				if (_indices) {
					commandBuffer->drawIndexed(s.primitiveCount * verts_per_prim(_geometry.primitiveTopology), instanceCount, s.firstIndex, s.firstVertex, firstInstance);
				}
				else {
					commandBuffer->draw(s.primitiveCount * verts_per_prim(_geometry.primitiveTopology), instanceCount, s.firstVertex, firstInstance);
				}
			}
		}

	private:
		std::string _name;
		vrg::Geometry _geometry;
		Buffer::StrideView _indices;
		std::vector<Submesh> _submeshes;
	};
}
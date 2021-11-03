#include "Mesh.hpp"

using namespace vrg;

std::shared_ptr<Mesh> Mesh::Cube(CommandBuffer& commandBuffer) {
	Device& device = commandBuffer._device;

	auto m = std::make_shared<Mesh>("Triangle");

	const std::vector<glm::vec3> verts = {
	glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.5f, 0.5f, -0.5f),
	glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.5f, 0.5f, 0.5f)
	};
	const std::vector<glm::vec3> cols = {
		{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
		{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}
	};
	const std::vector<uint32_t> inds = {
		0, 1, 2,
		1, 3, 2,
		5, 4, 6,
		7, 5, 6,
		4, 0, 2,
		6, 4, 2,
		1, 5, 7,
		3, 1, 7,
		7, 6, 2, 
		3, 7, 2,
		1, 0, 4,
		5, 1, 4
	};


	BufferVector<glm::vec3> vertices(device, verts.size());
	BufferVector<glm::vec3> colors(device, cols.size());
	BufferVector<uint32_t> indices(device, inds.size());


	//vertices.Resize(3);
	//colors.Resize(3);
	//indices.Resize(3);

	memcpy(vertices.Data(), verts.data(), verts.size() * sizeof(glm::vec3));
	memcpy(colors.Data(), cols.data(), cols.size() * sizeof(glm::vec3));
	memcpy(indices.Data(), inds.data(), inds.size() * sizeof(uint32_t));

	m->_indices = commandBuffer.CopyBuffer<uint32_t>(indices, vk::BufferUsageFlagBits::eIndexBuffer);

	//m->_geometry.bindings.resize(2);
	m->_geometry.bindings[VertexAttributeType::Position].first = commandBuffer.CopyBuffer<glm::vec3>(vertices, vk::BufferUsageFlagBits::eVertexBuffer);
	m->_geometry.bindings[VertexAttributeType::Color].first = commandBuffer.CopyBuffer<glm::vec3>(colors, vk::BufferUsageFlagBits::eVertexBuffer);

	m->_geometry[VertexAttributeType::Position][0] = Geometry::Attribute(VertexAttributeType::Position, vk::Format::eR32G32B32Sfloat, 0);
	m->_geometry[VertexAttributeType::Color][0] = Geometry::Attribute(VertexAttributeType::Color, vk::Format::eR32G32B32Sfloat, 0);

	m->_submeshes.emplace_back(inds.size() / 3, 0, 0);
	return m;
}
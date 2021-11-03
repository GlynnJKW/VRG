#pragma once

#include "SpirvModule.hpp"
#include "Texture.hpp"
#include "Buffer.hpp"

namespace vrg {

	using Descriptor = std::variant<
		Buffer::View<std::byte>, //storage and uniform buffers
		vk::AccelerationStructureKHR // acceleration structure
	>;

	template<typename T> requires(std::is_same_v<T, Texture::View>)
	constexpr T& get(vrg::Descriptor& d) { return std::get<T>(std::get<0>(d)); }



	class DescriptorSetLayout : public DeviceResource {
	public:
		struct Binding {
			vk::DescriptorType descriptorType;
			vk::ShaderStageFlags stageFlags;
			uint32_t descriptorCount;
		};
	private:
		vk::DescriptorSetLayout _layout;
		vk::DescriptorSetLayoutCreateFlags _flags;
		std::unordered_map<uint32_t, Binding> _bindings;
	public:
		inline DescriptorSetLayout(Device& device, const std::string& name, const std::unordered_map<uint32_t, Binding>& bindings = {}, vk::DescriptorSetLayoutCreateFlags flags = {})
			: DeviceResource(device, name), _bindings(bindings), _flags(flags) {
			std::vector<vk::DescriptorSetLayoutBinding> b;
			for (auto& [index, binding] : _bindings) {
				b.emplace_back(index, binding.descriptorType, binding.descriptorCount, binding.stageFlags);
			}
			_layout = _device->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(_flags, b));
		}

		inline ~DescriptorSetLayout() {
			_device->destroyDescriptorSetLayout(_layout);
		}

		inline const vk::DescriptorSetLayout& operator*() const { return _layout; }
		inline const vk::DescriptorSetLayout* operator->() const { return &_layout; }

		inline const Binding& At(uint32_t binding) const { return _bindings.at(binding); }
		inline const Binding& operator[](uint32_t binding) { return At(binding); }

		inline const std::unordered_map<uint32_t, Binding>& Bindings() const { return _bindings; }
	};

	class DescriptorSet : public DeviceResource {
	private:
		friend class Device;
		friend class CommandBuffer;
		vk::DescriptorSet _descriptorSet;
		std::shared_ptr<const DescriptorSetLayout> _layout;

		std::unordered_map<uint64_t, Descriptor> _boundDescriptors;
		std::unordered_set<uint64_t> _pendingWrites;

	public:
		inline DescriptorSet(std::shared_ptr<const DescriptorSetLayout> layout, const std::string& name)
			: DeviceResource(layout->_device, name), _layout(layout) {
			auto descriptorPool = _device._descriptorPool;
			vk::DescriptorSetAllocateInfo allocInfo = {};
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &**_layout;
			_descriptorSet = _device->allocateDescriptorSets(allocInfo)[0];
		}

		inline DescriptorSet(std::shared_ptr<const DescriptorSetLayout> layout, const std::string& name, const std::unordered_map<uint32_t, Descriptor>& bindings)
			: DescriptorSet(layout, name) {
			for (const auto& [binding, desc] : bindings) {
				InsertOrAssign(binding, desc);
			}
		}

		inline ~DescriptorSet() {
			_boundDescriptors.clear();
			_pendingWrites.clear();
			_layout.reset();
			_device->freeDescriptorSets(_device._descriptorPool, { _descriptorSet });
		}


		inline void InsertOrAssign(uint32_t binding, const Descriptor& desc) {
			InsertOrAssign(binding, 0, desc);
		}
		inline void InsertOrAssign(uint32_t binding, uint32_t index, const Descriptor& desc) {
			_pendingWrites.emplace(to_set_binding(binding, index));
			_boundDescriptors[to_set_binding(binding, index)] = desc;
		}

		inline std::shared_ptr<const DescriptorSetLayout> Layout() const { return _layout; }
		inline const DescriptorSetLayout::Binding& LayoutAt(uint32_t binding) const { return _layout->At(binding); }
		inline const Descriptor& Find(uint32_t binding, uint32_t index) const {
			auto it = _boundDescriptors.find(to_set_binding(binding, index));
			if (it == _boundDescriptors.end()) return {};
			return it->second;
		}

		inline const vk::DescriptorSet& operator*() { return _descriptorSet; }
		inline const vk::DescriptorSet* operator->() { return &_descriptorSet; }

		inline const Descriptor& At(uint32_t binding, uint32_t index = 0) const {
			return _boundDescriptors.at(to_set_binding(binding, index));
		}
		inline const Descriptor& operator[](uint32_t binding) const { return At(binding); }

		inline void FlushWrites() {
			if (_pendingWrites.empty()) {
				return;
			}

			struct WriteInfo {
				vk::DescriptorImageInfo imageInfo;
				vk::DescriptorBufferInfo bufferInfo;
				vk::WriteDescriptorSetInlineUniformBlockEXT inlineInfo;
				vk::WriteDescriptorSetAccelerationStructureKHR accelerationStructureInfo;
			};

			std::vector<WriteInfo> infos;
			std::vector<vk::WriteDescriptorSet> writes;
			std::vector<vk::CopyDescriptorSet> copies;
			writes.reserve(_pendingWrites.size());
			infos.reserve(_pendingWrites.size());
			for (auto& setBinding : _pendingWrites) {
				auto [binding, index] = from_set_binding(setBinding);
				auto& descBinding = _layout->At(binding);
				auto& desc = _boundDescriptors.at(to_set_binding(binding, index));
				auto& info = infos.emplace_back(WriteInfo{});
				auto& write = writes.emplace_back(vk::WriteDescriptorSet(_descriptorSet, binding, index, 1, descBinding.descriptorType));
				switch (write.descriptorType) {
					case vk::DescriptorType::eInputAttachment:
					case vk::DescriptorType::eSampledImage:
					case vk::DescriptorType::eStorageImage:
					case vk::DescriptorType::eCombinedImageSampler:
					case vk::DescriptorType::eSampler: {
						//sampler / image / sampler + image
						break;
					}
					case vk::DescriptorType::eUniformBuffer:
					case vk::DescriptorType::eStorageBuffer:
					case vk::DescriptorType::eStorageBufferDynamic:
					case vk::DescriptorType::eUniformBufferDynamic: {
						//uniform / storage buffer
						const auto& view = get<Buffer::View<std::byte>>(desc);

						info.bufferInfo.buffer = *view.Buffer();
						info.bufferInfo.offset = view.Offset();
						info.bufferInfo.range = view.ByteSize();
						write.pBufferInfo = &info.bufferInfo;
						break;
					}
					case vk::DescriptorType::eUniformTexelBuffer:
					case vk::DescriptorType::eStorageTexelBuffer: {
						//texel buffer
						break;
					}
					case vk::DescriptorType::eInlineUniformBlockEXT: {
						//inline
					}
					case vk::DescriptorType::eAccelerationStructureKHR: {
						//acceleration structure
					}
				}
			}
			_device->updateDescriptorSets(writes, copies);
			_pendingWrites.clear();
		}
	};

}

//template<> struct std::hash<vrg::DescriptorSet::SetBinding> {
//	inline size_t operator()(const vrg::DescriptorSet::SetBinding& d) const {
//		return vrg::hash_combine(d.binding, d.index);
//	}
//};
//
//static_assert(vrg::Hashable<vrg::DescriptorSet::SetBinding>);
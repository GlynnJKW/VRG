#pragma once

#include "Device.hpp"

namespace vrg {

	inline vk::AccessFlags GuessAccessMask(vk::ImageLayout layout) {
		switch (layout) {
		case vk::ImageLayout::eUndefined:
		case vk::ImageLayout::ePresentSrcKHR:
		case vk::ImageLayout::eColorAttachmentOptimal:
			return {};

		case vk::ImageLayout::eGeneral:
			return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;

		case vk::ImageLayout::eDepthAttachmentOptimal:
		case vk::ImageLayout::eStencilAttachmentOptimal:
		case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
		case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		case vk::ImageLayout::eDepthReadOnlyOptimal:
		case vk::ImageLayout::eStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
			return vk::AccessFlagBits::eDepthStencilAttachmentRead;

		case vk::ImageLayout::eShaderReadOnlyOptimal:
			return vk::AccessFlagBits::eShaderRead;
		case vk::ImageLayout::eTransferSrcOptimal:
			return vk::AccessFlagBits::eTransferRead;
		case vk::ImageLayout::eTransferDstOptimal:
			return vk::AccessFlagBits::eTransferWrite;
		}
		return vk::AccessFlagBits::eShaderRead;
	}
	inline vk::PipelineStageFlags GuessStage(vk::ImageLayout layout) {
		switch (layout) {
		case vk::ImageLayout::eGeneral:
			return vk::PipelineStageFlagBits::eComputeShader;

		case vk::ImageLayout::eColorAttachmentOptimal:
			return vk::PipelineStageFlagBits::eColorAttachmentOutput;

		case vk::ImageLayout::eShaderReadOnlyOptimal:
		case vk::ImageLayout::eDepthReadOnlyOptimal:
		case vk::ImageLayout::eStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
			return vk::PipelineStageFlagBits::eFragmentShader;

		case vk::ImageLayout::eTransferSrcOptimal:
		case vk::ImageLayout::eTransferDstOptimal:
			return vk::PipelineStageFlagBits::eTransfer;

		case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		case vk::ImageLayout::eStencilAttachmentOptimal:
		case vk::ImageLayout::eDepthAttachmentOptimal:
		case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
			return vk::PipelineStageFlagBits::eLateFragmentTests;

		case vk::ImageLayout::ePresentSrcKHR:
		case vk::ImageLayout::eSharedPresentKHR:
			return vk::PipelineStageFlagBits::eBottomOfPipe;

		default:
			return vk::PipelineStageFlagBits::eTopOfPipe;
		}
	}

	class Texture : public DeviceResource {
	public:
		static constexpr uint32_t MaxMips(const vk::Extent3D& extent) {
			return 32 - (uint32_t)std::countl_zero(std::max(std::max(extent.width, extent.height), extent.depth));
		}

		class View;

		enum ImageType {
			Auto = 3,
			Three = vk::ImageType::e3D,
			Two = vk::ImageType::e2D,
			One = vk::ImageType::e1D
		};

		Texture(Device& device, const std::string& name, const vk::Extent3D& extent, vk::Format format, ImageType type = ImageType::Auto, uint32_t arrayLayers = 1, uint32_t mipLevels = 0,
			vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1,
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
			vk::ImageCreateFlags createFlags = {}, vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

		//Create around already existing Image (useful for swapchain, etc)
		Texture(vk::Image image, Device& device, const std::string& name, const vk::Extent3D& extent, vk::Format format, uint32_t arrayLayers = 1, uint32_t mipLevels = 0,
			vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1,
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
			vk::ImageCreateFlags createFlags = {},
			vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

		inline Texture(Device& device, const std::string& name, const vk::Extent3D& extent, const vk::AttachmentDescription& description, vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
			vk::ImageCreateFlags createFlags = {}, vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageTiling tiling = vk::ImageTiling::eOptimal)
			: Texture(device, name, extent, description.format, ImageType::Auto, 1, 1, description.samples, usage, createFlags, memoryProperties, tiling) {}

		inline ~Texture() {
			for (auto& [k, v] : _views) _device->destroyImageView(v);
			if (_allocation) {
				_device.FreeImage(_image, _allocation);
			}
		}

		inline const vk::Image& operator*() { return _image; }
		inline const vk::Image* operator->() { return &_image; }
		inline operator bool() const { return _image; }

		inline vk::Extent3D Extent() const { return _extent; }
		inline vk::Format Format() const { return _format; }
		inline vk::ImageUsageFlags Usage() const { return _usage; }
		inline vk::SampleCountFlags SampleCount() const { return _sampleCount; }
		inline uint32_t MipLevels() const { return _mipLevels; }
		inline uint32_t ArrayLayers() const { return _arrayLayers; }
		inline vk::ImageAspectFlags AspectFlags() const { return _aspectFlags; }
		inline vk::ImageCreateFlags CreateFlags() const { return _createFlags; }

		void GenerateMipMaps(vrg::CommandBuffer& commandBuffer);

		void TransitionBarrier(vrg::CommandBuffer& commandBuffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
		inline void TransitionBarrier(vrg::CommandBuffer& commandBuffer, vk::ImageLayout newLayout) {
			TransitionBarrier(commandBuffer, _trackedStages, GuessStage(newLayout), _trackedLayout, newLayout);
		}

		class View {
		private:
			vk::ImageView _view;
			std::shared_ptr<vrg::Texture> _texture;
			vk::ImageAspectFlags _aspect;
			uint32_t _baseMip;
			uint32_t _mipCount;
			uint32_t _baseLayer;
			uint32_t _layerCount;

		public:
			View() = default;
			View(const View&) = default;
			View(View&&) = default;
			View(std::shared_ptr<Texture> texture, uint32_t baseMip = 0, uint32_t mipCount = 0, uint32_t baseLayer = 0, uint32_t layerCount = 0, vk::ImageAspectFlags aspect = (vk::ImageAspectFlags)0, vk::ComponentMapping components = {});
		
			View& operator=(const View&) = default;
			View& operator=(View&& v) = default;
			inline bool operator==(const View& rhs) const = default;
			inline operator bool() const { return _texture && _view; }

			inline const vk::ImageView& operator*() const { return _view; }
			inline const vk::ImageView* operator->() const { return &_view; }
			inline std::shared_ptr<Texture> TexturePtr() const { return _texture; }
			inline Texture& Texture() const { return *_texture; }
		};

	private:
		friend class Texture::View;
		friend class Window;
		friend class CommandBuffer;

		vk::Image _image;
		vk::Extent3D _extent;
		uint32_t _arrayLayers = 0;
		vk::Format _format;
		uint32_t _mipLevels = 0;
		vk::SampleCountFlagBits _sampleCount;
		vk::ImageUsageFlags _usage;
		vk::ImageCreateFlags _createFlags;
		vk::ImageAspectFlags _aspectFlags;
		vk::ImageTiling _tiling = vk::ImageTiling::eOptimal;
		ImageType _type;

		VmaAllocation _allocation = nullptr;

		std::unordered_map<size_t, vk::ImageView> _views;

		vk::ImageLayout _trackedLayout = vk::ImageLayout::eUndefined;
		vk::PipelineStageFlags _trackedStages = vk::PipelineStageFlagBits::eTopOfPipe;
		vk::AccessFlags _trackedAccessFlags = {};

		void Create();

	};
}
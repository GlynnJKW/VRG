#include "Texture.hpp"
#include "CommandBuffer.hpp"

using namespace vrg;

void Texture::Create() {
	vk::ImageCreateInfo imageInfo = {};
	if (_type == ImageType::Auto) {
		if (_extent.depth > 1) {
			imageInfo.imageType = vk::ImageType::e3D;
		}
		else if (_extent.height > 1 || (_extent.width == 1 & _extent.height == 1)) {
			imageInfo.imageType = vk::ImageType::e2D;
		}
		else {
			imageInfo.imageType = vk::ImageType::e1D;
		}
	}
	else {
		imageInfo.imageType = (vk::ImageType)_type;
	}

	imageInfo.extent.width = _extent.width;
	imageInfo.extent.height = _extent.height;
	imageInfo.extent.depth = _extent.depth;
	imageInfo.mipLevels = _mipLevels;
	imageInfo.arrayLayers = _arrayLayers;
	imageInfo.format = _format;
	imageInfo.tiling = _tiling;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = _usage;
	imageInfo.samples = _sampleCount;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.flags = _createFlags;
	_image = _device->createImage(imageInfo);

	switch (_format) {
	default:
		_aspectFlags = vk::ImageAspectFlagBits::eColor;
		break;
	case vk::Format::eD16Unorm:
	case vk::Format::eD32Sfloat:
		_aspectFlags = vk::ImageAspectFlagBits::eDepth;
		break;
	case vk::Format::eS8Uint:
		_aspectFlags = vk::ImageAspectFlagBits::eStencil;
		break;
	case vk::Format::eD16UnormS8Uint:
	case vk::Format::eD24UnormS8Uint:
	case vk::Format::eD32SfloatS8Uint:
	case vk::Format::eX8D24UnormPack32:
		_aspectFlags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		break;
	}
}

Texture::Texture(Device& device, const std::string& name, const vk::Extent3D& extent, vk::Format format, ImageType type, uint32_t arrayLayers, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount, vk::ImageUsageFlags usage, vk::ImageCreateFlags createFlags, vk::MemoryPropertyFlags memoryProperties, vk::ImageTiling tiling)
	: DeviceResource(device, name), _extent(extent), _format(format), _arrayLayers(arrayLayers), 
	_mipLevels(mipLevels ? mipLevels : (sampleCount > vk::SampleCountFlagBits::e1) ? 1 : MaxMips(extent)), _sampleCount(sampleCount), _usage(usage), _createFlags(createFlags), _tiling(tiling), _type(type) {
	Create();
	_allocation = _device.AllocateImage(_image, _device->getImageMemoryRequirements(_image), memoryProperties);
	//mMemory = mDevice.AllocateMemory(mDevice->getImageMemoryRequirements(mImage), properties);
	//mDevice->bindImageMemory(mImage, *mMemory->mMemory, mMemory->mOffset);
}

Texture::Texture(vk::Image image, Device& device, const std::string& name, const vk::Extent3D& extent, vk::Format format, uint32_t arrayLayers, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount, vk::ImageUsageFlags usage, vk::ImageCreateFlags createFlags, vk::ImageTiling tiling)
	: DeviceResource(device, name), _image(image), _extent(extent), _format(format), _arrayLayers(arrayLayers),
	_mipLevels(mipLevels ? mipLevels : (sampleCount > vk::SampleCountFlagBits::e1) ? 1 : MaxMips(extent)), _sampleCount(sampleCount), _usage(usage), _createFlags(createFlags), _tiling(tiling), _type(vrg::Texture::ImageType::Auto){
	switch (_format) {
	default:
		_aspectFlags = vk::ImageAspectFlagBits::eColor;
		break;
	case vk::Format::eD16Unorm:
	case vk::Format::eD32Sfloat:
		_aspectFlags = vk::ImageAspectFlagBits::eDepth;
		break;
	case vk::Format::eS8Uint:
		_aspectFlags = vk::ImageAspectFlagBits::eStencil;
		break;
	case vk::Format::eD16UnormS8Uint:
	case vk::Format::eD24UnormS8Uint:
	case vk::Format::eD32SfloatS8Uint:
	case vk::Format::eX8D24UnormPack32:
		_aspectFlags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		break;
	}
}

void Texture::TransitionBarrier(CommandBuffer& commandBuffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	if (oldLayout == newLayout) return;
	if (newLayout == vk::ImageLayout::eUndefined) {
		_trackedLayout = newLayout;
		_trackedStages = dstStage;
		_trackedAccessFlags = {};
		return;
	}

	vk::ImageMemoryBarrier barrier = {};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = _image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = MipLevels();
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = ArrayLayers();
	barrier.srcAccessMask = _trackedAccessFlags;
	barrier.dstAccessMask = GuessAccessMask(newLayout);
	barrier.subresourceRange.aspectMask = _aspectFlags;
	commandBuffer.Barrier(srcStage, dstStage, barrier);
	_trackedLayout = newLayout;
	_trackedStages = dstStage;
	_trackedAccessFlags = barrier.dstAccessMask;
}

void Texture::GenerateMipMaps(CommandBuffer& commandBuffer) {
	// TODO
}

Texture::View::View(std::shared_ptr<vrg::Texture> texture, uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount, vk::ImageAspectFlags aspect, vk::ComponentMapping components)
	: _texture(texture), _baseMip(baseMip), _mipCount(mipCount ? mipCount : texture->MipLevels()), _baseLayer(baseLayer), _layerCount(layerCount ? layerCount : texture->ArrayLayers()), _aspect(aspect) {

	if (_aspect == (vk::ImageAspectFlags)0) _aspect = texture->AspectFlags();

	size_t key = vrg::hash_combine(_baseMip, _mipCount, _baseLayer, _layerCount, _aspect, components);
	if (texture->_views.count(key)) {
		_view = texture->_views.at(key);
	}
	else {
		vk::ImageViewCreateInfo info = {};
		if (_texture->CreateFlags() & vk::ImageCreateFlagBits::eCubeCompatible) {
			info.viewType == vk::ImageViewType::eCube;
		}
		else if (_texture->_type == Texture::ImageType::Auto) {
			if (_texture->Extent().depth > 1) {
				info.viewType = vk::ImageViewType::e3D;
			}
			else if (_texture->Extent().height > 1) {
				info.viewType = vk::ImageViewType::e2D;
			}
			else {
				info.viewType = vk::ImageViewType::e1D;
			}
		}
		else {
			info.viewType = (vk::ImageViewType)_texture->_type;
		}

		info.image = **_texture;
		info.format = _texture->Format();
		info.subresourceRange.aspectMask = _aspect;
		info.subresourceRange.baseArrayLayer = _baseLayer;
		info.subresourceRange.layerCount = _layerCount;
		info.subresourceRange.baseMipLevel = _baseMip;
		info.subresourceRange.levelCount = _mipCount;
		info.components = components;
		_view = _texture->_device->createImageView(info);

		_texture->_views.emplace(key, _view);
	}
}
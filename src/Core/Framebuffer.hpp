#pragma once

#include "RenderPass.hpp"
#include "Texture.hpp"

namespace vrg {

	class Framebuffer : public DeviceResource {
	private:
		vk::Framebuffer _framebuffer;
		vk::Extent2D _extent = { 0, 0 };
		vrg::RenderPass& _renderPass;
		std::vector<vrg::Texture::View> _attachments;

	public:
		
		inline Framebuffer(const std::string& name, vrg::RenderPass& renderPass, const vk::ArrayProxy<const vrg::Texture::View>& attachments)
			: DeviceResource(renderPass._device, name), _renderPass(renderPass) {
			_attachments.resize(_renderPass.AttachmentDescriptions().size());
			std::vector<vk::ImageView> views(_attachments.size());
			for (const auto& view : attachments) {
				size_t idx = _renderPass.AttachmentIndex(view.Texture().Name());
				_attachments[idx] = view;
				views[idx] = *view;
				_extent = vk::Extent2D(std::max(_extent.width, view.Texture().Extent().width), std::max(_extent.height, view.Texture().Extent().height));
			}

			_framebuffer = _renderPass._device->createFramebuffer(vk::FramebufferCreateInfo({}, *_renderPass, views, _extent.width, _extent.height, 1));
		}

		template<std::convertible_to<Texture::View>... Args>
		inline Framebuffer(const std::string& name, vrg::RenderPass& renderPass, Args&&... attachments)
			: Framebuffer(name, renderPass, { std::forward<Args>(attachments)... }) {}

		inline ~Framebuffer() { _device->destroyFramebuffer(_framebuffer); }

		inline const vk::Framebuffer& operator*() const { return _framebuffer; };
		inline const vk::Framebuffer* operator->() const { return &_framebuffer; };

		inline vk::Extent2D Extent() const { return _extent; }
		inline vrg::RenderPass& RenderPass() const { return _renderPass; }

		inline size_t size() const { return _attachments.size(); }
		inline auto begin() const { return _attachments.begin(); }
		inline auto end() const { return _attachments.end(); }

		inline const Texture::View& operator[](size_t index) const { return _attachments[index]; }
		inline const Texture::View& operator[](const std::string& id) const { return _attachments[_renderPass.AttachmentIndex(id)]; }
	};
}
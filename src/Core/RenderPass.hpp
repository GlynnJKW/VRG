#pragma once

#include "Window.hpp"

namespace vrg {

	class RenderPass : public DeviceResource {
	public:
		enum AttachmentType {
			Input,
			Color,
			Resolve,
			DepthStencil,
			Preserve
		};

		struct SubpassDescription {
			std::string name;
			vk::PipelineBindPoint bindPoint;
			std::unordered_map<std::string, std::pair<vk::AttachmentDescription, std::pair<AttachmentType, vk::PipelineColorBlendAttachmentState>>> attachmentDescriptions;
			// names of attachments that this subpass depends on, and their accessflags for this subpass
			//unordered_map<string, vk::AccessFlags> mSubpassDependencies;
		};

		inline RenderPass(Device& device, const std::string& name, const std::vector<SubpassDescription> subpassDescriptions)
			: DeviceResource(device, name), _subpassDescriptions(subpassDescriptions), _hash(hash_combine(name, subpassDescriptions)){

			struct SubpassData {
				std::vector<vk::AttachmentReference> inputAttachments;
				std::vector<vk::AttachmentReference> colorAttachments;
				std::vector<vk::AttachmentReference> resolveAttachments;
				std::vector<uint32_t> preserveAttachments;
				vk::AttachmentReference depthAttachment;
			};

			std::vector<SubpassData> subpassData(subpassDescriptions.size());
			std::vector<vk::SubpassDescription> subpasses(subpassDescriptions.size());
			std::vector<vk::SubpassDependency> dependencies;
			std::vector<vk::AttachmentDescription> attachments;

			for (uint32_t i = 0; i < subpasses.size(); ++i) {
				auto& sp = subpasses[i];
				auto& spd = subpassData[i];

				for (const auto& [attachmentName, attachmentData] : subpassDescriptions[i].attachmentDescriptions) {

					const auto& vkdesc = attachmentData.first;
					if (_attachmentMap.count(attachmentName)) {
						auto& [desc, id] = _attachmentDescriptions[_attachmentMap.at(attachmentName)];
						desc.finalLayout = vkdesc.finalLayout;
						desc.storeOp = vkdesc.storeOp;
						desc.stencilStoreOp = vkdesc.stencilStoreOp;
					}
					else {
						_attachmentMap.emplace(attachmentName, _attachmentDescriptions.size());
						_attachmentDescriptions.push_back({ vkdesc, attachmentName });
						attachments.push_back(vkdesc);
					}

					switch (attachmentData.second.first) {
						case AttachmentType::Color:
							spd.colorAttachments.emplace_back((uint32_t)_attachmentMap.at(attachmentName), vk::ImageLayout::eColorAttachmentOptimal);
							break;
						case AttachmentType::DepthStencil:
							spd.depthAttachment = vk::AttachmentReference((uint32_t)_attachmentMap.at(attachmentName), vk::ImageLayout::eDepthStencilAttachmentOptimal);
							break;
						case AttachmentType::Resolve:
							spd.resolveAttachments.emplace_back((uint32_t)_attachmentMap.at(attachmentName), vk::ImageLayout::eColorAttachmentOptimal);
							break;
						case AttachmentType::Input:
							spd.inputAttachments.emplace_back((uint32_t)_attachmentMap.at(attachmentName), vk::ImageLayout::eShaderReadOnlyOptimal);
							break;
						case AttachmentType::Preserve:
							spd.preserveAttachments.emplace_back((uint32_t)_attachmentMap.at(attachmentName));
							break;
					}

					//look for which subpass index uses the attachment
					uint32_t srcSubpass = i;
					for (int32_t j = i - 1; j >= 0; --j) {
						if (_subpassDescriptions[j].attachmentDescriptions.count(attachmentName)) {
							auto t = _subpassDescriptions[j].attachmentDescriptions.at(attachmentName).second.first;
							if (t == AttachmentType::Color || t == AttachmentType::DepthStencil || t == AttachmentType::Resolve) {
								srcSubpass = j;
								break;
							}
						}
					}

					if (srcSubpass < i) {
						const auto& srcAttachment = _subpassDescriptions[srcSubpass].attachmentDescriptions.at(attachmentName);
						vk::SubpassDependency dependency(srcSubpass, i);
						dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;
						switch (srcAttachment.second.first) {
							case AttachmentType::Color:
								dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
								dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
								if (std::get<vk::AttachmentDescription>(srcAttachment).storeOp != vk::AttachmentStoreOp::eStore)
									throw std::invalid_argument("Color attachment " + attachmentName + " must use vk::AttachmentStoreOp::eStore");
								break;
							case AttachmentType::DepthStencil:
								dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
								dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
								if (std::get<vk::AttachmentDescription>(srcAttachment).storeOp != vk::AttachmentStoreOp::eStore)
									throw std::invalid_argument("DepthStencil attachment " + attachmentName + " must use vk::AttachmentStoreOp::eStore");
								break;
							case AttachmentType::Resolve:
								dependency.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
								dependency.srcStageMask = vk::PipelineStageFlagBits::eTransfer;
								if (std::get<vk::AttachmentDescription>(srcAttachment).storeOp != vk::AttachmentStoreOp::eStore)
									throw std::invalid_argument("Resolve attachment " + attachmentName + " must use vk::AttachmentStoreOp::eStore");
								break;
							default:
								throw std::invalid_argument("Attachment " + attachmentName + " cannot act as a dependency source");
						}

						switch (attachmentData.second.first) {
							case AttachmentType::Color:
								dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
								dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
								break;
							case AttachmentType::DepthStencil:
								dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
								dependency.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
								break;
							case AttachmentType::Resolve:
								dependency.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
								dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
								break;
							case AttachmentType::Input:
								dependency.dstAccessMask = vk::AccessFlagBits::eShaderRead;
								dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
								break;
							case AttachmentType::Preserve:
								dependency.dstAccessMask = vk::AccessFlagBits::eShaderRead;
								dependency.dstStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
								break;
						}

						dependencies.push_back(dependency);
					}
				}
				
				sp.pipelineBindPoint = _subpassDescriptions[i].bindPoint;
				sp.pDepthStencilAttachment = &spd.depthAttachment;
				sp.setInputAttachments(spd.inputAttachments);
				sp.setPreserveAttachments(spd.preserveAttachments);
				sp.setResolveAttachments(spd.resolveAttachments);
				sp.setColorAttachments(spd.colorAttachments);
			}

			auto renderPassInfo = vk::RenderPassCreateInfo({}, attachments, subpasses, dependencies);
			_renderPass = _device->createRenderPass(renderPassInfo);
		}

		inline ~RenderPass() {
			_device->destroyRenderPass(_renderPass);
		}

		inline const vk::RenderPass& operator*() const { return _renderPass; }
		inline const vk::RenderPass* operator->() const { return &_renderPass; }

		inline const auto& SubpassDescriptions() const { return _subpassDescriptions; }
		inline const std::vector<std::pair<vk::AttachmentDescription, std::string>>& AttachmentDescriptions() const { return _attachmentDescriptions; }
		inline size_t AttachmentIndex(const std::string& id) const { return _attachmentMap.at(id); }

	private:
		friend class CommandBuffer;
		friend struct std::hash<vrg::RenderPass>;
		
		vk::RenderPass _renderPass;
		std::vector<std::pair<vk::AttachmentDescription, std::string>> _attachmentDescriptions;
		std::unordered_map<std::string, size_t> _attachmentMap;

		std::vector<SubpassDescription> _subpassDescriptions;

		size_t _hash;
	};

}

template<> struct std::hash<vrg::RenderPass> {
	inline size_t operator()(const vrg::RenderPass& rp) { return rp._hash; }
};
template<> struct std::hash<vrg::RenderPass::SubpassDescription> {
	inline size_t operator()(const vrg::RenderPass::SubpassDescription& p) { return vrg::hash_combine(p.name, p.attachmentDescriptions); }
};

template<> struct std::hash<vk::PipelineColorBlendAttachmentState> {
	inline size_t operator()(const vk::PipelineColorBlendAttachmentState& p) {
		return vrg::hash_combine(p.alphaBlendOp, p.blendEnable, p.colorBlendOp, p.colorWriteMask, p.dstAlphaBlendFactor, p.dstColorBlendFactor, p.srcAlphaBlendFactor, p.srcColorBlendFactor);
	}
};
template<> struct std::hash<vk::AttachmentDescription> {
	inline size_t operator()(const vk::AttachmentDescription& p) {
		return vrg::hash_combine(p.finalLayout, p.flags, p.format, p.initialLayout, p.loadOp, p.samples, p.stencilLoadOp, p.stencilStoreOp, p.storeOp);
	}
};
template<typename A, typename B> struct std::hash<std::pair<A, B>> {
	inline size_t operator()(const std::pair<A, B>& p) {
		return vrg::hash_combine(p.first, p.second);
	}
};


static_assert(vrg::Hashable< std::unordered_map<std::string, std::pair<vk::AttachmentDescription, std::pair<vrg::RenderPass::AttachmentType, vk::PipelineColorBlendAttachmentState>>>>);

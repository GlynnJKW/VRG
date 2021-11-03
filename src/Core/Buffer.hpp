#pragma once

#include "Device.hpp"

namespace vrg {

	class CommandBuffer;

	class Buffer : public DeviceResource {
	private:
		vk::Buffer _buffer;
		VmaAllocation _allocation = nullptr;
		vk::DeviceSize _size;
		vk::BufferUsageFlags _usage;
		vk::SharingMode _sharingMode;


	public:
		inline Buffer(Device& device, const std::string& name, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN, vk::SharingMode sharingMode = vk::SharingMode::eExclusive, vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal)
			: DeviceResource(device, name), _size(size), _usage(usage), _sharingMode(sharingMode) {
			_buffer = _device->createBuffer(vk::BufferCreateInfo({}, _size, _usage, _sharingMode));
			_allocation = _device.AllocateBuffer(_buffer, _device->getBufferMemoryRequirements(_buffer), memoryProperties, memoryUsage);
		}

		inline Buffer(VmaAllocation allocation, Device& device, const std::string& name, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::SharingMode sharingMode = vk::SharingMode::eExclusive)
			: DeviceResource(device, name), _size(size), _usage(usage), _sharingMode(sharingMode) {
			_buffer = _device->createBuffer(vk::BufferCreateInfo({}, _size, _usage, _sharingMode));
			vmaBindBufferMemory(_device.Allocator(), allocation, _buffer);
		}

		inline ~Buffer() {
			_device.FreeBuffer(_buffer, _allocation);
			//_device->destroyBuffer(_buffer);
		}

		inline const vk::Buffer& operator*() const { return _buffer; }
		inline const vk::Buffer* operator->() const { return &_buffer; }
		inline operator bool() const { return _buffer; }

		inline vk::BufferUsageFlags Usage() const { return _usage; }
		inline vk::SharingMode SharingMode() const { return _sharingMode; }
		inline vk::DeviceSize Size() const { return _size; }
		inline std::byte* Data() const { return reinterpret_cast<std::byte*>(_device.AllocationInfo(_allocation).pMappedData); }

		template<typename T>
		class View {
		private:
			std::shared_ptr<Buffer> _buffer;
			vk::DeviceSize _offset;
			vk::DeviceSize _size;

		public:

			View() = default;
			View(const View&) = default;
			inline View(const std::shared_ptr<Buffer>& buffer, vk::DeviceSize offset = 0, vk::DeviceSize elementCount = VK_WHOLE_SIZE)
				: _buffer(buffer), _offset(offset), _size(elementCount == VK_WHOLE_SIZE ? (buffer->Size() - offset) / sizeof(T) : elementCount) {
				if (!buffer) throw std::invalid_argument("Buffer used in view cannot be null");
				if (_offset + _size > _buffer->Size()) throw std::out_of_range("View size is out of buffer bounds");
			}

			View& operator=(const View&) = default;
			View& operator=(View&&) = default;
			bool operator==(const View & rhs) const = default;

			inline operator bool() const { return !Empty(); }

			inline Buffer& Buffer() const { return *_buffer; }
			inline std::shared_ptr<vrg::Buffer> BufferPtr() const { return _buffer; }
			inline vk::DeviceSize Offset() const { return _offset; }
			inline bool Empty() const { return !_buffer || _size == 0; }
			inline vk::DeviceSize Size() const { return _size; }
			inline vk::DeviceSize ByteSize() const { return _size * sizeof(T); }
			inline T* Data() const { return reinterpret_cast<T*>(_buffer->Data() + Offset()); }

			inline T& at(vk::DeviceSize index) const { return Data()[index]; }
			inline T& operator[](vk::DeviceSize index) const { return at(index); }

			inline T& Front() { return at(0); }
			inline T& Back() { return at(_size - 1); }

			inline T* Begin() const { return Data(); }
			inline T* End() const { return Data() + _size; }
		};

		class StrideView : public View<std::byte> {
		private:
			vk::DeviceSize _stride;

		public:
			StrideView() = default;
			StrideView(const StrideView&) = default;
			StrideView(StrideView&&) = default;

			inline StrideView(const View<std::byte>& view, vk::DeviceSize stride) : View<std::byte>(view), _stride(stride) {}
			inline StrideView(const std::shared_ptr<vrg::Buffer>& buffer, vk::DeviceSize stride, vk::DeviceSize offset = 0, vk::DeviceSize elementCount = VK_WHOLE_SIZE)
				: View<std::byte>(buffer, offset, elementCount* stride), _stride(stride) {}

			template<typename T>
			inline StrideView(const View<T>& v) : View<std::byte>(v.BufferPtr(), v.Offset(), v.ByteSize()), _stride(sizeof(T)) {}

			StrideView& operator=(const StrideView&) = default;
			StrideView& operator=(StrideView&&) = default;
			bool operator==(const StrideView & rhs) const = default;

			inline vk::DeviceSize Stride() const { return _stride; }
		};

	};

	template<class T>
	class BufferVector {
	private:
		std::shared_ptr<Buffer> _buffer;
		vk::DeviceSize _size;
		vk::BufferUsageFlags _bufferUsage;
		VmaMemoryUsage _memoryUsage;
		vk::SharingMode _sharingMode;
	public:
		Device& _device;

		BufferVector() = delete;
		BufferVector(BufferVector<T>&&) = default;
		inline BufferVector(Device& device, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eTransferSrc, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode sharingMode = vk::SharingMode::eExclusive)
			: _device(device), _size(size), _bufferUsage(bufferUsage), _memoryUsage(memoryUsage), _sharingMode(sharingMode) {
			if (size) {
				vk::DeviceSize tmp = _size;
				_size = 0;
				Resize(tmp);
			}
		}

		inline BufferVector(const BufferVector<T>& v)
			: _device(v._device), _size(v._size), _bufferUsage(v._bufferUsage), _memoryUsage(v._memoryUsage), _sharingMode(v._sharingMode) {
			if (_size) {
				Reserve(_size);
				for (vk::DeviceSize i = 0; i < _size; ++i) {
					new (Data() + i) T(v[i]);
				}
			}
		}

		inline ~BufferVector() {
			for (vk::DeviceSize i = 0; i < _size; ++i) {
				At(i).~T();
			}
		}

		inline operator Buffer::View<std::byte>() const { return Buffer::View<std::byte>(_buffer, 0, ByteSize()); }
		inline operator Buffer::View<T>() const { return Buffer::View<T>(_buffer, 0, Size()); }

		inline bool Empty() const { return _size == 0; }
		inline vk::DeviceSize Size() const { return _size; }
		inline vk::DeviceSize ByteSize() const { return _size * sizeof(T); }
		inline vk::DeviceSize Capacity() const { return _buffer ? 0 : _buffer->Size() / sizeof(T); }
		
		inline vk::BufferUsageFlags BufferUsage() const { return _bufferUsage; }
		inline VmaMemoryUsage MemoryUsage() const { return _memoryUsage; }
		inline vk::SharingMode SharingMode() const { return _sharingMode; }

		inline T* Data() { return reinterpret_cast<T*>(_buffer->Data()); }

		inline void Reserve(vk::DeviceSize size) {
			if ((!_buffer && size > 0) || size > Capacity()) {
				vk::MemoryRequirements requirements;
				if (_buffer) {
					requirements = _device->getBufferMemoryRequirements(**_buffer);
					requirements.size = std::max(size * sizeof(T), _buffer->Size() + _buffer->Size() / 2);
				}
				else {
					vk::Buffer tmp = _device->createBuffer(vk::BufferCreateInfo({}, size * sizeof(T), _bufferUsage, _sharingMode));
					requirements = _device->getBufferMemoryRequirements(tmp);
					_device->destroyBuffer(tmp);
				}
				requirements.alignment = std::max(requirements.alignment, std::alignment_of_v<T>);
				auto buf = std::make_shared<Buffer>(_device, "BufferVector", requirements.size, _bufferUsage, _memoryUsage, _sharingMode);
				if (_buffer) memcpy(buf->Data(), _buffer->Data(), _buffer->Size());
				_buffer = buf;
			}
		}

		inline void Resize(vk::DeviceSize size) {
			if (size > _size) {
				Reserve(size);
				for (vk::DeviceSize i = _size; i < size; ++i) {
					new (Data() + i) T();
				}
				_size = size;
			}
			else if (size < _size) {
				for (vk::DeviceSize i = size; i < _size; ++i) {
					At(i).~T();
				}
				_size = size;
			}
		}

		inline T& Front() { return *Data(); }
		inline T& Back() { return *(Data() + (_size - 1)); }

		inline T* Begin() { return Data(); }
		inline T* End() { return Data() + _size; }

		inline T& At(vk::DeviceSize index) {
			if (index > _size) throw std::out_of_range("Index out of buffer bounds");
			return Data()[index];
		}

		inline T& operator[](vk::DeviceSize index) {
			return At(index);
		}

		template<typename... Args> requires(std::constructible_from<T, Args...>)
		inline T* Emplace_back(Args&&... args) {
			while (_size + 1 > Capacity()) Reserve(std::max(1, Capacity() * _size * 2)); //Exponentially increase array size to fit new element
			return *new (Data() + (_size++)) T(std::forward<Args>(args)...);
		}

		inline void Pop_back() {
			if (Empty()) throw std::out_of_range("Cannot pop empty Buffer");
			Back().~T();
			--_size;
		}

		inline T* Erase(T* pos) {
			pos->~T();
			while (pos != &Back()) {
				memcpy(pos, ++pos, sizeof(T));
			}
			--_size;
		}

	};

}
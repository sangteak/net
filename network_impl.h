#pragma once

#include <memory>
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include "net_interface.h"

#define _USE_UNION

namespace net
{
class WriteBufferImpl : public IWriteBuffer
{
public:
	static const int32_t DEFAULT_BUFFER_SIZE = 12;
	static const int32_t DEFAULT_TOTAL_LENGTH = 0;
	static const int32_t RESERVED_SPACE_SIZE = sizeof(DEFAULT_TOTAL_LENGTH);

	using _size_t = int32_t;

	enum class eBufferType
	{
		STATIC = 1,
		DYNAMIC
	};

	WriteBufferImpl()
	{
	}

	virtual ~WriteBufferImpl()
	{
		if (eBufferType::DYNAMIC == m_type)
		{
#ifndef _USE_UNION
			delete [] m_pointer;
			m_pointer = nullptr;
#else
			delete[] m_buffer.pointer;
			m_buffer.pointer = nullptr;
#endif
		}
	}

	virtual bool IsEmpty() override
	{
		return false;
	}

	virtual const uint8_t* GetData() 
	{
		return GetPointer();
	}

	virtual _size_t GetLength() override
	{
		return m_offset;
	}

	virtual std::size_t Put(const void* data, const _size_t& length) override
	{
		if (m_maxLength < (m_offset + length))
		{
			Grow(length);
		}

		memcpy(GetCurrentPointer(), data, length);

		m_offset += length;

		return 0;
	}

	virtual void Commit() override
	{
		int32_t dataLength = m_offset - RESERVED_SPACE_SIZE;
		memcpy(GetPointer(), &dataLength, sizeof(dataLength));
	}

	// 기본 POD도 가능하다!!!!!

private:
	uint8_t* GetPointer()
	{
#ifndef _USE_UNION
		uint8_t* pointer = m_buffer.data();
		if (eBufferType::DYNAMIC == m_type)
		{
			pointer = m_pointer;
		}
#else
		uint8_t* pointer = m_buffer.arr;
		if (eBufferType::DYNAMIC == m_type)
		{
			pointer = m_buffer.pointer;
		}
#endif
		return pointer;
	}

	uint8_t* GetCurrentPointer()
	{
		return GetPointer() + m_offset;
	}

	void Grow(const _size_t& n)
	{
#ifndef _USE_UNION
		if (eBufferType::STATIC == m_type)
		{
			if (DEFAULT_BUFFER_SIZE >= (m_offset + n))
			{
				return;
			}

			m_pointer = new uint8_t[m_maxLength * 2];
			
			memcpy(m_pointer, m_buffer.data(), m_offset);

			m_type = eBufferType::DYNAMIC;
			m_maxLength = m_maxLength * 2;
			return;
		}

		uint8_t* pointer = new uint8_t[m_maxLength * 2];
		
		memcpy(pointer, m_pointer, m_offset);
		if (nullptr != m_pointer)
		{
			delete[] m_pointer;
			m_pointer = nullptr;
		}

		m_pointer = pointer;
		m_maxLength = m_maxLength * 2;
#else 
		if (eBufferType::STATIC == m_type)
		{
			if (DEFAULT_BUFFER_SIZE >= (m_offset + n))
			{
				return;
			}

			uint8_t* tmp = new uint8_t[m_maxLength * 2];

			memcpy(tmp, m_buffer.arr, m_offset);

			m_type = eBufferType::DYNAMIC;
			m_maxLength = m_maxLength << 1;
			m_buffer.pointer = tmp;
			return;
		}

		uint8_t* tmp = new uint8_t[m_maxLength * 2];

		memcpy(tmp, m_buffer.pointer, m_offset);
		if (nullptr != m_buffer.pointer)
		{
			delete[] m_buffer.pointer;
			m_buffer.pointer = nullptr;
		}

		m_buffer.pointer = tmp;
		m_maxLength = m_maxLength << 1;
#endif
	}

	_size_t m_offset = RESERVED_SPACE_SIZE;
	eBufferType m_type = eBufferType::STATIC;

#ifndef _USE_UNION
	std::array<uint8_t, DEFAULT_BUFFER_SIZE> m_buffer;
	uint8_t* m_pointer{ nullptr };
#else
	union Buffer
	{
		uint8_t arr[DEFAULT_BUFFER_SIZE];
		uint8_t* pointer = nullptr;
	};

	Buffer m_buffer;
#endif
	_size_t m_maxLength{ DEFAULT_BUFFER_SIZE };
};

class Session;
class SessionManager;
class NetworkImpl : public IController, private boost::noncopyable
{
	using _io_context_t = boost::asio::io_context;
	using _acceptor_t = boost::asio::ip::tcp::acceptor;
	using _acceptor_ptr_t = std::unique_ptr<_acceptor_t>;
	using _thread_group_t = std::vector<std::thread>;
	using _signal_set_t = boost::asio::signal_set;
	using _session_manager_ptr_t = std::unique_ptr<SessionManager>;

public:
	explicit NetworkImpl(const int32_t numberOfThreads);

	virtual bool HasService() override
	{
		return (nullptr != m_service);
	}

	virtual void AttachService(IService* service) override;
	virtual void DetachService() override;

	virtual bool HasLogging() override
	{
		return (nullptr != m_logging);
	}
	virtual void AttachLogging(ILogging* logging) override;
	virtual void DetachLogging() override;

	virtual bool HasMonitor() override
	{
		return (nullptr != m_monitor);
	}
	virtual void AttachMonitor(IMonitor* monitor) override;
	virtual void DetachMonitor() override;

	virtual bool HasConfiguration() override
	{
		return (nullptr != m_configuration);
	}

	virtual void AttachConfiguration(IConfiguration* t_configuration) override;
	virtual void DetachConfiguratio() override;

	virtual void Stop() override;

	virtual bool IsState(const _sid_t& sid, const eState& state) override;

	virtual _sid_t Connect() override;
	virtual bool Accept() override;

	virtual bool Write(const _sid_t& sid, std::shared_ptr<uint8_t>& data, const std::size_t& len) override;
	virtual bool Write(const _sid_t& sid, std::shared_ptr<IWriteBuffer>& buffer) override;

private:
	void CreateWorkerThread();

	void AcceptInner();
	void HandleAccept(boost::shared_ptr<Session> session, const boost::system::error_code& t_errorCode);
	
	uint32_t m_numberOfThreads = 0;
	_io_context_t m_ioContext;
	_acceptor_ptr_t m_acceptor;

	_signal_set_t m_signalSet;

	_thread_group_t m_threadGroup;

	_session_manager_ptr_t m_sessionManager;

	IConfiguration* m_configuration = nullptr;
	IService* m_service = nullptr;
	ILogging* m_logging = nullptr;
	IMonitor* m_monitor = nullptr;
};
}
#pragma once

#include <map>
#include <shared_mutex>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/aligned_storage.hpp>
#include "error_code.h"
#include "net_interface.h"
#include "stream_buffer.h"

namespace boost
{
namespace asio
{
class HandlerMemory : private boost::noncopyable
{
public:
	HandlerMemory()
		: m_inUse(false)
	{
	}

	void* allocate(std::size_t t_size)
	{
		if (!m_inUse && t_size < m_storage.size)
		{
			m_inUse = true;
			return m_storage.address();
		}
		else
		{
			return ::operator new(t_size);
		}
	}

	void deallocate(void* t_pointer)
	{
		if (t_pointer == m_storage.address())
		{
			m_inUse = false;
		}
		else
		{
			::operator delete(t_pointer);
		}
	}

private:
	// Storage space used for handler-based custom memory allocation.
	boost::aligned_storage<1024> m_storage;

	// Whether the handler-based custom allocation storage has been used.
	bool m_inUse;
};

// The allocator to be associated with the handler objects. This allocator only
// needs to satisfy the C++11 minimal allocator requirements, plus rebind when
// targeting C++03.
template <typename T>
class HandlerAllocator
{
public:
	typedef T value_type;

	explicit HandlerAllocator(HandlerMemory& t_mem)
		: m_memory(t_mem)
	{
	}

	template <typename U>
	HandlerAllocator(const HandlerAllocator<U>& t_other)
		: m_memory(t_other.m_memory)
	{
	}

	template <typename U>
	struct rebind
	{
		typedef HandlerAllocator<U> other;
	};

	bool operator==(const HandlerAllocator& t_other) const
	{
		return &m_memory == &t_other._memory;
	}

	bool operator!=(const HandlerAllocator& t_other) const
	{
		return &m_memory != &t_other.m_memory;
	}

	T* allocate(std::size_t t_n) const
	{
		return static_cast<T*>(m_memory.allocate(sizeof(T) * t_n));
	}

	void deallocate(T* t_p, std::size_t /*n*/) const
	{
		return m_memory.deallocate(t_p);
	}

	//private:
		// The underlying memory.
	HandlerMemory& m_memory;
};

// Wrapper class template for handler objects to allow handler memory
// allocation to be customised. The allocator_type typedef and get_allocator()
// member function are used by the asynchronous operations to obtain the
// allocator. Calls to operator() are forwarded to the encapsulated handler.
template <typename Handler>
class CustomAllocHandler
{
public:
	typedef HandlerAllocator<Handler> allocator_type;

	CustomAllocHandler(HandlerMemory& t_m, Handler t_h)
		: m_memory(t_m)
		, m_handler(t_h)
	{
	}

	allocator_type get_allocator() const
	{
		return allocator_type(m_memory);
	}

	template <typename Arg1>
	void operator()(Arg1 t_arg1)
	{
		m_handler(t_arg1);
	}

	template <typename Arg1, typename Arg2>
	void operator()(Arg1 t_arg1, Arg2 t_arg2)
	{
		m_handler(t_arg1, t_arg2);
	}

private:
	HandlerMemory& m_memory;
	Handler m_handler;
};

// Helper function to wrap a handler object to add custom allocation.
template <typename Handler>
inline CustomAllocHandler<Handler> make_custom_alloc_handler(HandlerMemory& t_m, Handler t_h)
{
	return CustomAllocHandler<Handler>(t_m, t_h);
}
} // end namespace asio
} // end namespace boost

namespace net
{
enum class eWriteState
{
	IDEL = 1,
	WRITING = 2,
};

// async_write 호출 중 재 호출되지 않도록 처리해야함.
// 만일 async_write가 호출되고 있다면 대기 buffer에 메시지를 넣고 async_write가 완료되면 이어서 보내도록 한다.
class WriteQueue : boost::noncopyable
{
public:
	using _streambuf_t = boost::asio::streambuf;
	using _buffers_t = std::array<_streambuf_t, 2>;

	enum class eBufferType
	{
		NONE = 0,
		CURRENT = 1,
		WRITING = 2,
	};

	WriteQueue()
		: m_currentBuffer(&m_buffers[0])
		, m_writingBuffer(&m_buffers[1])
	{
	}

	~WriteQueue() = default;

	inline eBufferType GetTransmissibleBufferType()
	{
		if (0 < m_writingBuffer->size())
		{
			return eBufferType::WRITING;
		}
		else if (0 < m_currentBuffer->size())
		{
			return eBufferType::CURRENT;
		}

		return eBufferType::NONE;
	}

	// 데이터가 기록된 버퍼의 const_buffer_type으로 반환한다.
	const _streambuf_t& GetCurrentBuffer() const
	{
		return *m_currentBuffer;
	}

	const _streambuf_t& GetWritingBuffer() const
	{
		return *m_writingBuffer;
	}

	// 메시지를 대기 버퍼에 추가한다.
	void Put(const uint8_t* t_data, const std::size_t& t_length)
	{
		m_currentBuffer->sputn(reinterpret_cast<const char*>(t_data), t_length);
	}

	// sending buffer 보내진 만큼 삭제
	void Consume(const std::size_t& t_length)
	{
		m_writingBuffer->consume(t_length);
	}

	void Switch()
	{
		std::swap(m_currentBuffer, m_writingBuffer);
	}

private:
	_buffers_t m_buffers;
	_streambuf_t* m_currentBuffer = nullptr;
	_streambuf_t* m_writingBuffer = nullptr;
};

class Session : public boost::enable_shared_from_this<Session>, private boost::noncopyable
{
	using _io_context_t = boost::asio::io_context;
	using _resolver_t = boost::asio::ip::tcp::resolver;
	using _resolver_ptr_t = std::unique_ptr<_resolver_t>;
	using _socket_t = boost::asio::ip::tcp::socket;
	using _strand_t = boost::asio::strand<_io_context_t::executor_type>;
	//using _read_buffer_t = boost::asio::streambuf;
	using _read_buffer_t = std::array<uint8_t, 1024>;
	using _destroy_callback_t = std::function<void(const _sid_t& sid)>;

public:
	explicit Session(const _sid_t& sid, _io_context_t& t_ioContext, IService* t_service, ILogging* t_logging, IMonitor* t_monitor, _destroy_callback_t&& t_destroyCallback);

	inline _socket_t& getSocket() { return m_socket; }

	inline const _sid_t GetSID() { return m_sid; }

	inline bool IsState(const eState t_state)
	{
		return (m_state == t_state);
	}

	void Resolve(const std::string& t_address, const std::string& t_port);

	void Start();

	void PostClose(boost::system::error_code t_errorCode);

	void Post(std::shared_ptr<uint8_t> t_data, const std::size_t& t_len);
	void Post(std::shared_ptr<IWriteBuffer>& t_buffer);

private:
	inline void SetState(const eState t_state)
	{
		m_state = t_state;
	}

	void Read();
	void Write(const uint8_t* t_data, const std::size_t& t_len);
	void Close(boost::system::error_code t_errorCode);

	template <typename Callback>
	bool Post(Callback t_callback)
	{
		if (false == IsState(eState::CONNECTED))
		{
			OnError(boost::system::make_error_code(net::eErrorCode::NOT_CONNECTED));
			return false;
		}
		m_strand.post(t_callback, std::allocator<char>());
	
		return true;
	}

	void HandleRead(const boost::system::error_code& t_errorCode, std::size_t t_bytesTransferred);
	void HandleWrite(const boost::system::error_code& t_errorCode, std::size_t t_bytesTransferred);
	void HandleResolve(const boost::system::error_code& t_errorCode, const boost::asio::ip::tcp::resolver::results_type& t_endpoints);
	void HandleConnect(const boost::system::error_code& t_errorCode);

	inline bool HasService()
	{
		return (nullptr != m_service);
	}

	inline bool IsWriteState(const eWriteState& t_state)
	{
		return (t_state == m_writeState);
	}

	inline void SetWriteState(const eWriteState& t_state)
	{
		m_writeState = t_state;
	}

	void OnConnected();
	void OnClose(const boost::system::error_code& t_errorCode);
	void OnMessage(const uint8_t* t_data, const std::size_t& t_len);
	void OnError(const boost::system::error_code& t_errorCode);

	void Log(const eLogLevel& t_level, const char* function, const int32_t line, const boost::system::error_code& t_errorCode);
	void Log(const eLogLevel& t_level, const char* function, const int32_t line, const std::string_view t_message);

	eState m_state = eState::NONE;

	_sid_t m_sid = 0;
	
	_resolver_ptr_t m_resolver;

	_io_context_t& m_ioContext;
	_strand_t m_strand;
	_socket_t m_socket;

	// 리드 버퍼
	_read_buffer_t m_readBuffer;
	StreamBuffer<> m_messageBuffer;

	// 쓰기 버퍼, 이중으로 되어 있음.
	eWriteState m_writeState{ eWriteState::IDEL };
	WriteQueue m_writeQueue;

	// 서비스
	//_service_ptr_t m_service;
	IService* m_service;
	ILogging* m_logging;
	IMonitor* m_monitor;

	// asio 핸들러 커스텀 메모리 할당자
	boost::asio::HandlerMemory m_handlerMemory;

	// 연결 종료 시 정리
	_destroy_callback_t m_destroyCallback;
};
using _session_ptr_t = boost::shared_ptr<Session>;

class SessionManager : boost::noncopyable
{
public:
	using _io_context_t = boost::asio::io_context;
	using _session_map_t = std::map<_sid_t, _session_ptr_t>;
	
	bool Create(_io_context_t& t_ioContext, IService* t_service, ILogging* t_logging, IMonitor* t_monitor, _session_ptr_t& session);

	bool Lookup(const _sid_t& t_sid, _session_ptr_t& t_session);

	bool Add(const _sid_t& t_sid, const _session_ptr_t& t_session);

	void Remove(const _sid_t& t_sid);

private:
	inline _sid_t GeneratedSID()
	{
		return ++m_currentSID;
	}

	mutable std::shared_mutex m_rwMutex;

	std::atomic<_sid_t> m_currentSID{ 0 };

	_session_map_t m_sessionMap;
};

using _session_manager_ptr_t = std::unique_ptr<SessionManager>;
}
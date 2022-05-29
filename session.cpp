#include <iostream>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include "session.h"

namespace net
{
#pragma region R_SESSION
Session::Session(const _sid_t& t_sid, _io_context_t& t_ioContext, IService* t_service, ILogging* t_logging, IMonitor* t_monitor, _destroy_callback_t&& t_destroyCallback)
	: m_sid(t_sid)
	, m_ioContext(t_ioContext)
	, m_strand(boost::asio::make_strand(t_ioContext))
	, m_socket(m_strand)
	, m_service(t_service)
	, m_logging(t_logging)
	, m_monitor(t_monitor)
	, m_destroyCallback(std::forward<_destroy_callback_t>(t_destroyCallback))
{
}

void Session::Resolve(const std::string& t_address, const std::string& t_port)
{
	if (nullptr == m_resolver)
	{
		m_resolver.reset(new _resolver_t(m_ioContext));
	}

	// 연결 중
	SetState(eState::CONNECTING);

	boost::asio::ip::tcp::resolver::query query(t_address, t_port);
	m_resolver->async_resolve(query,
		boost::bind(&Session::HandleResolve, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::results));
}

void Session::Start()
{
	SetState(eState::CONNECTED);

	Post([this] {
			Read();

			OnConnected();
		}
	);
}

void Session::Close(boost::system::error_code t_errorCode)
{
	if (!t_errorCode)
	{
		boost::system::error_code ignoredErrorCode;
		m_socket.shutdown(_socket_t::shutdown_both, ignoredErrorCode); // 우아한 종료. 
	}

	if (t_errorCode != boost::asio::error::operation_aborted)
	{
		boost::system::error_code closeErrorCode;
		m_socket.close(closeErrorCode);
	}

	OnClose(t_errorCode);

	m_destroyCallback(m_sid);

	m_sid = 0;

	// 연결 종료 상태
	SetState(eState::CLOSED);
}

void Session::PostClose(boost::system::error_code t_errorCode)
{
	Post([this, t_errorCode]() {
		Close(t_errorCode);
		}
	);
}

void Session::Post(std::shared_ptr<uint8_t> t_data, const std::size_t& t_len)
{
	Post([this, t_data, t_len]() {
		const uint8_t* pointer = reinterpret_cast<const uint8_t*>(t_data.get());
		Write(pointer, t_len);
		}
	);
}

void Session::Post(std::shared_ptr<IWriteBuffer>& t_buffer)
{
	Post([this, t_buffer]() {
		Write(t_buffer->GetData(), t_buffer->GetLength());
		}
	);
}

void Session::Write(const uint8_t* t_data, const std::size_t& t_len)
{
	m_writeQueue.Put(t_data, t_len);
	
	// 아무것도 하지 않는 경우 쓰기 처리를 한다.
	if (true == IsWriteState(eWriteState::IDEL))
	{
		m_writeQueue.Switch();

		WriteQueue::_streambuf_t::const_buffers_type buffer = m_writeQueue.GetWritingBuffer().data();
		boost::asio::async_write(
			m_socket, buffer,
			boost::asio::make_custom_alloc_handler(m_handlerMemory,
				boost::bind(&Session::HandleWrite, shared_from_this(), 
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred)
			)
		);

		// 쓰기 상태로 변경한다.
		SetWriteState(eWriteState::WRITING);
	}
}

void Session::Read()
{
	m_socket.async_read_some(boost::asio::buffer(m_readBuffer),
		boost::asio::make_custom_alloc_handler(m_handlerMemory,
			boost::bind(&Session::HandleRead, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		)
	);
}

void Session::HandleRead(const boost::system::error_code& t_errorCode, std::size_t t_bytesTransferred)
{
	if (t_errorCode)
	{
		Close(t_errorCode);
		return;
	}

	m_messageBuffer.Write(m_readBuffer.data(), t_bytesTransferred);
	
	uint8_t header[4] = { 0, };
	std::size_t headerSize = sizeof(header);

	m_messageBuffer.Read(header, headerSize);
	auto dataLength = *((int32_t*)header);

	Log(eLogLevel::DEBUG, __FUNCTION__, __LINE__, "");

	while ((headerSize + dataLength) <= m_messageBuffer.GetLength())
	{
		// 2. 완료된 패킷을 콜백에 올려준다.
		std::shared_ptr<uint8_t> message(
			new uint8_t[dataLength], 
			[](uint8_t* pointer) {
				delete[] pointer;
			}
		);

		// Header Size 
		m_messageBuffer.Consume(headerSize);
		m_messageBuffer.ReadAndConsume(message.get(), dataLength);
		
		OnMessage(message.get(), dataLength);

		if (headerSize >= m_messageBuffer.GetLength())
		{ 
			break;
		}

		memset(header, 0x00, headerSize);
		m_messageBuffer.Read(header, headerSize);
		dataLength = *((int32_t*)header);
	}

	Read();
}

void Session::HandleWrite(const boost::system::error_code& t_errorCode, std::size_t t_bytesTransferred)
{
	m_writeQueue.Consume(t_bytesTransferred);

	if (t_errorCode)
	{
		std::cout << __FUNCTION__ << " - failed. message=" << t_errorCode.message() << std::endl;
		Close(t_errorCode);
		return;
	}

	SetWriteState(eWriteState::IDEL);
	
	auto transmissibleBufferType = m_writeQueue.GetTransmissibleBufferType();
	if (WriteQueue::eBufferType::NONE == transmissibleBufferType)
	{
		return;
	}

	if (WriteQueue::eBufferType::CURRENT == transmissibleBufferType)
	{
		m_writeQueue.Switch();
	}

	SetWriteState(eWriteState::WRITING);

	WriteQueue::_streambuf_t::const_buffers_type buffer = m_writeQueue.GetWritingBuffer().data();
	// 현재 버퍼로는 안되는데?? ㅋㅋ WriingBuffer로 다시한번!!!!!
	boost::asio::async_write(
		m_socket, buffer,
		boost::asio::make_custom_alloc_handler(m_handlerMemory,
			boost::bind(&Session::HandleWrite, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		)
	);

	std::cout << __FUNCTION__ << " - success." << std::endl;
}

void Session::HandleResolve(const boost::system::error_code& t_errorCode, const boost::asio::ip::tcp::resolver::results_type& t_endpoints)
{
	if (t_errorCode)
	{
		return;
	}

	// Attempt a connection to each endpoint in the list until we
	// successfully establish a connection.
	boost::asio::async_connect(m_socket, t_endpoints,
		boost::bind(&Session::HandleConnect, shared_from_this(),
			boost::asio::placeholders::error));
}

void Session::HandleConnect(const boost::system::error_code& t_errorCode)
{
	if (t_errorCode)
	{
		std::cout << __FUNCTION__ << "- message=" << t_errorCode.message() << std::endl;
		// TODO : 에러 노티 필요함. 실패했다!!!
		OnError(t_errorCode);
		return;
	}

	// 시작합시다!!!!!
	Start();
}

void Session::OnConnected()
{
	if (false == HasService())
	{
		return;
	}

	m_service->OnConnected(m_sid);
}

void Session::OnClose(const boost::system::error_code& t_errorCode)
{
	if (false == HasService())
	{
		return;
	}

	m_service->OnClose(m_sid, t_errorCode);
}

void Session::OnMessage(const uint8_t* t_data, const std::size_t& t_len)
{
	if (false == HasService())
	{
		return;
	}

	m_service->OnMessage(m_sid, t_data, t_len);
}

void Session::OnError(const boost::system::error_code& t_errorCode)
{
	if (false == HasService())
	{
		return;
	}

	m_service->OnError(m_sid, t_errorCode);
}

void Session::Log(const eLogLevel& t_level, const char* function, const int32_t line, const boost::system::error_code& t_errorCode)
{
	Log(t_level, function, line, t_errorCode.to_string());
}

void Session::Log(const eLogLevel& t_level, const char* function, const int32_t line, const std::string_view t_message)
{
	if (nullptr == m_logging)
	{
		return;
	}

	std::stringstream ss;
	ss << "[" << function << "(" << line << ")] " << t_message;

	switch (t_level)
	{
	case eLogLevel::TRACE:
		m_logging->Trace(ss.str());
		break;
	case eLogLevel::DEBUG:
		m_logging->Debug(ss.str());
		break;

	case eLogLevel::INFO:
		m_logging->Info(ss.str());
		break;

	case eLogLevel::WARN:
		m_logging->Warnning(ss.str());
		break;

	case eLogLevel::ERR:
		m_logging->Error(ss.str());
		break;

	default:
		break;
	}
}

#pragma endregion R_SESSION

#pragma region R_SESSION_MANAGER
bool SessionManager::Create(_io_context_t& t_ioContext, IService* t_service, ILogging* t_logging, IMonitor* t_monitor, _session_ptr_t& t_session)
{
	_sid_t sid = GeneratedSID();

	t_session = boost::make_shared<Session>(sid, t_ioContext, t_service, t_logging, t_monitor,
		[this](const auto& sid) {
			Remove(sid);
		}
	);

	if (nullptr == t_session)
	{
		return false;
	}

	Add(sid, t_session);

	return true;
}

bool SessionManager::Lookup(const _sid_t& t_sid, _session_ptr_t& t_session)
{
	std::shared_lock lock(m_rwMutex);

	auto itr = m_sessionMap.find(t_sid);
	if (itr == m_sessionMap.end())
	{
		return false;
	}

	t_session = itr->second;

	return true;
}

bool SessionManager::Add(const _sid_t& t_sid, const _session_ptr_t& t_session)
{
	std::unique_lock lock(m_rwMutex);
	
	return m_sessionMap.emplace(t_sid, t_session).second;
}

void SessionManager::Remove(const _sid_t& t_sid)
{
	std::unique_lock lock(m_rwMutex);
	m_sessionMap.erase(t_sid);
}
#pragma endregion R_SESSION_MANAGER
}

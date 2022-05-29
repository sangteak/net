#pragma once

#include "net_define.h"

namespace net
{
class IConfiguration
{
public:
	using _address_t = std::pair<std::string, std::string>;

	using _lingeropt_t = std::optional<std::pair<bool, int32_t>>;
	using _boolopt_t = std::optional<bool>;
	using _sizeopt_t = std::optional<int32_t>;

	// 접속 정보
	virtual _address_t GetAddress() = 0;
	
	// Socket 옵션(optional을 사용하여 nullopt가 아닐 경우에만 해당 옵션을 적용하도록 한다.)
	virtual _boolopt_t Reuse() = 0;
	virtual _sizeopt_t MMS() = 0;
	virtual _lingeropt_t Linger() = 0;
	virtual _boolopt_t Nagle() = 0;
	virtual _boolopt_t Keepalive() = 0;

	//
};

class IWriteBuffer
{
public:
	virtual bool IsEmpty() = 0;
	virtual const uint8_t* GetData() = 0;
	virtual int32_t GetLength() = 0;
	virtual std::size_t Put(const void* data, const int32_t& length) = 0;
	virtual void Commit() = 0;
};
using _write_buffer_ptr_t = std::shared_ptr<IWriteBuffer>;

// 서비스에 필요한 콜백을 정의합니다.
class IService
{
public:
	virtual void OnConnected(const _sid_t& sid) = 0;
	virtual void OnClose(const _sid_t& sid, const boost::system::error_code& t_errorCode) = 0;
	virtual void OnMessage(const _sid_t& sid, const uint8_t* data, const std::size_t& len) = 0;
	virtual void OnError(const _sid_t& sid, const boost::system::error_code& t_errorCode) = 0;
};
using _service_ptr_t = std::shared_ptr<IService>;

enum class eLogLevel
{
	TRACE = 0,
	DEBUG = 1,
	INFO = 2,
	WARN = 3,
	ERR = 4,
};

class ILogging
{
public:
	virtual void Trace(const std::string_view message) = 0;
	virtual void Debug(const std::string_view message) = 0;
	virtual void Info(const std::string_view message) = 0;
	virtual void Warnning(const std::string_view message) = 0;
	virtual void Error(const std::string_view message) = 0;
};
using _logging_ptr_t = std::shared_ptr<ILogging>;

class IMonitor
{
	// 송신 요청
	virtual void OnSend() = 0;

	// 송신 완료
	virtual void OnSent() = 0;

	// 수신
	virtual void OnReceive() = 0;
};

// 네트워크 기능을 이용하기 위한 기본 기능을 제공합니다.
// 사용자에게 네트워크 설정 및 기능을 추가로 오픈해야할 경우 해당 인터페이스를 확장해야합니다.
enum class eState
{
	NONE = 0,
	CONNECTING,
	CONNECTED,
	CLOSED
};

class IController
{
public:
	virtual bool HasService() = 0;
	virtual void AttachService(IService* service) = 0;
	virtual void DetachService() = 0;

	virtual bool HasLogging() = 0;
	virtual void AttachLogging(ILogging* logging) = 0;
	virtual void DetachLogging() = 0;

	virtual bool HasMonitor() = 0;
	virtual void AttachMonitor(IMonitor* monitor) = 0;
	virtual void DetachMonitor() = 0;

	virtual bool HasConfiguration() = 0;
	virtual void AttachConfiguration(IConfiguration* configuration) = 0;
	virtual void DetachConfiguratio() = 0;

	virtual void Stop() = 0;

	virtual _sid_t Connect() = 0;
	virtual bool Accept() = 0;

	virtual bool IsState(const _sid_t& sid, const eState& state) = 0;

	virtual bool Write(const _sid_t& sid, std::shared_ptr<uint8_t>& data, const std::size_t& len) = 0;
	virtual bool Write(const _sid_t& sid, std::shared_ptr<IWriteBuffer>& buffer) = 0;
};
}
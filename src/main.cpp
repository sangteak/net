#include <iostream>
#include <memory>
#include <conio.h>
#include "network_impl.h"
#include "stream_buffer.h"
#include "session.h"

namespace tcp
{
// 내부 상태 로깅을 위해 net::ILogging 인터페이스 구현
class NetworkLogging : public net::ILogging
{
public:
	virtual void Trace(const std::string_view message) 
	{
		std::cout << "[TRACE] " << message << std::endl;
	}

	virtual void Debug(const std::string_view message)
	{
		std::cout << "[DEBUG] " << message << std::endl;
	}

	virtual void Info(const std::string_view message)
	{
		std::cout << "[INFO] " << message << std::endl;
	}

	virtual void Warnning(const std::string_view message)
	{
		std::cout << "[WARN] " << message << std::endl;
	}

	virtual void Error(const std::string_view message)
	{
		std::cout << "[ERROR] " << message << std::endl;
	}
};

// 클라이언트 기능 구현
class GameClient : public net::IListener, private boost::noncopyable
{
	using _controller_t = std::unique_ptr<net::IController>;

	// 클라이언트 설정 정보
	class Configuration : public net::IConfiguration
	{
	public:
		using _super_t = net::IConfiguration;

		// 접속 정보
		virtual _address_t GetAddress() override
		{
			return std::make_pair("127.0.0.1", "20195");
		}

		// Socket 옵션(optional을 사용하여 nullopt가 아닐 경우에만 해당 옵션을 적용하도록 한다.)
		virtual _boolopt_t Reuse() override
		{
			return std::nullopt;
		}

		virtual _sizeopt_t MMS() override
		{
			return std::nullopt;
		}

		virtual _lingeropt_t Linger() override
		{
			return std::nullopt;
		}

		virtual _boolopt_t Nagle() override
		{
			return std::nullopt;
		}

		virtual _boolopt_t Keepalive() override
		{
			return std::nullopt;
		}
	};

public:
	GameClient()
		: m_controller(new net::NetworkImpl(1))
	{
		m_controller->AttachService(this);
		m_controller->AttachConfiguration(&m_configuration);
	}

	inline _controller_t& GetConroller()
	{
		return m_controller;
	}

	virtual void OnConnected(const net::_sid_t& sid) override
	{
		std::cout << __FUNCTION__ << std::endl;

		std::shared_ptr<uint8_t> buf(
			new uint8_t[100],
			[](uint8_t* pointer) {
				delete[] pointer;
			}
		);

		int32_t dataLength = 5;
		std::string data = "Hello";
		
		memcpy(buf.get(), &dataLength, sizeof(dataLength));
		memcpy(buf.get() + sizeof(dataLength), data.data(), dataLength);

		m_controller->Write(sid, buf, sizeof(dataLength) + data.length());
	}

	virtual void OnClose(const net::_sid_t& sid, const boost::system::error_code&/*t_errorCode*/) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

	virtual void OnMessage(const net::_sid_t& sid, const uint8_t* data, const std::size_t& len) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << ", message=" << std::string(reinterpret_cast<const char*>(data), len) << std::endl;
	}

	virtual void OnError(const net::_sid_t& sid, const boost::system::error_code& /*t_errorCode*/) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

private:
	_controller_t m_controller;
	Configuration m_configuration;
};

// TCP 서버 기능 구현
class GameServer : public net::IListener, private boost::noncopyable
{
	// 설정 정보 
	class Configuration : public net::IConfiguration
	{
	public:
		using _super_t = net::IConfiguration;

		// 접속 정보
		virtual _address_t GetAddress() override
		{
			return std::make_pair("127.0.0.1", "20195");
		}

		// Socket 옵션(optional을 사용하여 nullopt가 아닐 경우에만 해당 옵션을 적용하도록 한다.)
		virtual _boolopt_t Reuse() override 
		{
			return _boolopt_t(true);
		}

		virtual _sizeopt_t MMS() override
		{
			return _sizeopt_t(102400);
		}

		virtual _lingeropt_t Linger() override
		{
			return std::nullopt;
		}

		virtual _boolopt_t Nagle() override
		{
			return _boolopt_t(false);
		}

		virtual _boolopt_t Keepalive() override
		{
			return _boolopt_t(false);
		}
	};

public:
	using _controller_t = std::unique_ptr<net::IController>;

	// 워커 쓰레드 2개로 서버 생성
	GameServer()
		: m_controller(new net::NetworkImpl(2))
	{
		m_controller->AttachService(this);
		m_controller->AttachConfiguration(&m_configuration);
	}

	inline _controller_t& GetConroller() 
	{
		return m_controller;
	}

	virtual void OnConnected(const net::_sid_t& sid) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

	virtual void OnClose(const net::_sid_t& sid, const boost::system::error_code& /*t_errorCode*/) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

	virtual void OnMessage(const net::_sid_t& sid, const uint8_t* data, const std::size_t& len) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << ", message=" << std::string(reinterpret_cast<const char*>(data), len) << std::endl;

		std::shared_ptr<net::IWriteBuffer> wbuffer(new net::WriteBufferImpl);
		wbuffer->Put(data, len);
		wbuffer->Commit();

		m_controller->Write(sid, wbuffer);
	}

	virtual void OnError(const net::_sid_t& sid, const boost::system::error_code& /*t_errorCode*/) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

private:

	NetworkLogging m_logging;
	_controller_t m_controller;
	Configuration m_configuration;
};
}

/*
// configuration.json
{
	"address": {
		"address":"127.0.0.1",
		"port": 20195
	},
	"socket option": {
		"reuse": false,
		"mms": 1024,
		"nagle": false,
		"keepalive": false,
		"linger": {
			"onoff": false,
			"sec": 0,
		}
	}
}
*/

int main(int argc, char* argv[])
{
	std::thread serverThread(
		[]() {
			tcp::GameServer gameserver;
			gameserver.GetConroller()->Accept();

			while (true)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
	);
	
	std::this_thread::sleep_for(std::chrono::seconds(3));
	
	std::cout << "client start" << std::endl;

	tcp::GameClient gameclient;
	auto sid = gameclient.GetConroller()->Connect();
	if (0 >= sid)
	{
		std::cout << "failed to connect to server." << std::endl;
		return 0;
	}

	while (false == gameclient.GetConroller()->IsState(sid, net::eState::CONNECTED))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}


	while (1)
	{
		std::cout << "\n\nPlease press 'Q' key for quit..." << std::endl;

		auto ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			break; // 종료
		}
	}

	return 0;
}
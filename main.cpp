#include <iostream>
#include <memory>
#include "network_impl.h"
#include "stream_buffer.h"

namespace tcp
{
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

class GameClient : public net::IService, private boost::noncopyable
{
	using _controller_t = std::unique_ptr<net::IController>;

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

		//auto buf = std::make_shared<uint8_t>(new uint8_t[100]);
		// 2. 완료된 패킷을 콜백에 올려준다.
		std::shared_ptr<uint8_t> buf(
			new uint8_t[100],
			[](uint8_t* pointer) {
				delete[] pointer;
			}
		);

		int32_t dataLength = 5;
		std::string data = "Hello";
		auto p = data.data();
		auto p2 = reinterpret_cast<const uint8_t*>(data.c_str());
		auto len = data.length();
		memcpy(buf.get(), &dataLength, sizeof(dataLength));
		memcpy(buf.get() + sizeof(dataLength), data.data(), dataLength);

		m_controller->Write(sid, buf, sizeof(dataLength) + data.length());
	}

	virtual void OnClose(const net::_sid_t& sid, const boost::system::error_code& t_errorCode) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

	virtual void OnMessage(const net::_sid_t& sid, const uint8_t* data, const std::size_t& len) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << ", message=" << std::string(reinterpret_cast<const char*>(data), len) << std::endl;
	}

	virtual void OnError(const net::_sid_t& sid, const boost::system::error_code& t_errorCode) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

private:
	_controller_t m_controller;
	Configuration m_configuration;
};

class GameServer : public net::IService, private boost::noncopyable
{
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
	using _controller_t = std::unique_ptr<net::IController>;

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
		std::cout << __FUNCTION__ << std::endl;
	}

	virtual void OnClose(const net::_sid_t& sid, const boost::system::error_code& t_errorCode) override
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

	virtual void OnError(const net::_sid_t& sid, const boost::system::error_code& t_errorCode) override
	{
		std::cout << __FUNCTION__ << " - sid:" << sid << std::endl;
	}

private:

	NetworkLogging m_logging;
	_controller_t m_controller;
	Configuration m_configuration;
};
}

void heapify(int32_t* arr, int32_t maxLength, int32_t index)
{
	// Root 노드
	int32_t largest = index;
	
	// Root 노드의 왼쪽 자식 노드
	int32_t left = 2 * index + 1;

	// Root 노드의 오른쪽 자식 노드
	int32_t right = 2 * index + 2;

	// 왼쪽 자식 노드가 존재한다면 값을 비교 후 충족된다면 largest 변경
	if (left < maxLength && arr[left] > arr[largest])
	{
		largest = left;
	}

	if (right < maxLength && arr[right] > arr[largest])
	{
		largest = right;
	}

	// index의 값이 변경됐다면... 
	if (largest != index)
	{
		std::swap(arr[index], arr[largest]);

		heapify(arr, maxLength, largest);
	}
}

void heapSort(int32_t* arr, int32_t maxLength)
{
	// 값을 정렬한다.
	for (int32_t i = maxLength / 2 - 1; i >= 0; --i)
	{
		heapify(arr, maxLength, i);
	}

	std::vector<int32_t> sorted;

	// 정렬한다.
	for (int32_t i = maxLength - 1; i >= 0; --i)
	{
		std::swap(arr[0], arr[i]);

		heapify(arr, i, 0); // root 를 변경(인덱스 0이 루트다)
	}
}

//#define _CLIENT
#include "session.h"

/*
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


	getchar();

#ifdef _CLIENT
	std::unique_ptr<net::IController> controller(new net::NetworkImpl(1));
	auto service = std::make_shared<tcp::NetworkService>();
	service->m_controller = controller.get();

	controller->AttachService(std::static_pointer_cast<net::IService>(service));
	
	controller->Connect("127.0.0.1", "20195");
	{
		std::cout << "connecting..." << std::endl;
	}

	getchar();
#else
	//tcp::GameServer gameserver;
	//gameserver.GetConroller()->Accept("127.0.0.1", "20195");

    getchar();
#endif
	

	return 0;
}
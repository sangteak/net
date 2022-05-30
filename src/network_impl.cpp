#include <iostream>
#include <vector>
#include <boost/bind.hpp>
#include "session.h"
#include "network_impl.h"

namespace net
{
NetworkImpl::NetworkImpl(const int32_t numberOfThreads)
	: m_signalSet(m_ioContext, SIGINT, SIGTERM)
	, m_numberOfThreads(numberOfThreads)
	, m_sessionManager(new SessionManager)
	, m_service(nullptr)
	, m_logging(nullptr)
	, m_monitor(nullptr)
{
	m_signalSet.async_wait([this](auto, auto) {
		Stop();
		});
}

void NetworkImpl::AttachService(IListener* service)
{
	if (nullptr == service)
	{
		return;
	}

	if (true == HasService())
	{
		DetachService();
	}

	m_service = service;
}

void NetworkImpl::DetachService()
{
	//m_service.reset();
	m_service = nullptr;
}

void NetworkImpl::AttachLogging(ILogging* logging)
{
	if (nullptr == logging)
	{
		return;
	}

	if (true == HasLogging())
	{
		DetachLogging();
	}

	m_logging = logging;
}

void NetworkImpl::DetachLogging()
{
	m_logging = nullptr;
}

void NetworkImpl::AttachMonitor(IMonitor* monitor)
{
	if (nullptr == monitor)
	{
		return;
	}

	if (true == HasMonitor())
	{
		DetachMonitor();
	}

	m_monitor = monitor;
}

void NetworkImpl::DetachMonitor()
{
	m_monitor = nullptr;
}

void NetworkImpl::AttachConfiguration(IConfiguration* t_configuration)
{
	if (nullptr == t_configuration)
	{
		return;
	}

	if (true == HasConfiguration())
	{
		DetachConfiguratio();
	}

	m_configuration = t_configuration;
}

void NetworkImpl::DetachConfiguratio()
{
	m_configuration = nullptr;
}

void NetworkImpl::Stop()
{
	m_ioContext.stop();

	for (auto& t : m_threadGroup)
	{
		if (true == t.joinable())
		{
			t.join();
		}
	}
}

bool NetworkImpl::IsState(const _sid_t& sid, const eState& state)
{
	_session_ptr_t session;
	if (false == m_sessionManager->Lookup(sid, session))
	{
		return false;
	}

	return session->IsState(state);
}

_sid_t NetworkImpl::Connect()
{
	if (false == HasConfiguration())
	{
		return 0;
	}

	_session_ptr_t session;
	if (false == m_sessionManager->Create(m_ioContext, m_service, m_logging, m_monitor, session))
	{
		// TODO : 노티해줘야함.
		return 0;
	}

	session->Resolve(m_configuration->GetAddress().first, m_configuration->GetAddress().second);

	CreateWorkerThread();

	return session->GetSID();
}

bool NetworkImpl::Accept()
{
	if (false == HasConfiguration())
	{
		return false;
	}

	if (nullptr == m_acceptor) 
	{
		m_acceptor.reset(new _acceptor_t(m_ioContext));
		boost::asio::ip::tcp::resolver resolver(m_ioContext);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(m_configuration->GetAddress().first, m_configuration->GetAddress().second).begin();
		m_acceptor->open(endpoint.protocol());
		m_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		m_acceptor->bind(endpoint);
		m_acceptor->listen();

		// 워커쓰레드 생성
		CreateWorkerThread();
	}

	AcceptInner();

	return true;
}

bool NetworkImpl::Write(const _sid_t& sid, std::shared_ptr<uint8_t>& data, const std::size_t& len)
{
	_session_ptr_t session;
	if (false == m_sessionManager->Lookup(sid, session))
	{
		return false;
	}
	
	// TODO : Session을 이용해 Write 처리
	//session->Write(data, len);
	session->Post(data, len);

	return true;
}

bool NetworkImpl::Write(const _sid_t& sid, std::shared_ptr<IWriteBuffer>& buffer)
{
	_session_ptr_t session;
	if (false == m_sessionManager->Lookup(sid, session))
	{
		return false;
	}

	// TODO : Session을 이용해 Write 처리
	//session->Write(data, len);
	session->Post(buffer);

	return true;
}

void NetworkImpl::CreateWorkerThread() 
{
	if (false == m_threadGroup.empty())
	{
		return;
	}

	for (uint32_t i = 0; i < m_numberOfThreads; ++i)
	{
		m_threadGroup.emplace_back([this]() {
			m_ioContext.run();

			std::cout << "ioContext terminated" << std::endl;
			});
	}
}

void NetworkImpl::AcceptInner()
{
	_session_ptr_t session;
	if (false == m_sessionManager->Create(m_ioContext, m_service, m_logging, m_monitor, session))
	{
		// TODO : 노티해줘야함.
		return;
	}

	m_acceptor->async_accept(session->getSocket(),
		boost::bind(&NetworkImpl::HandleAccept, this, session, boost::asio::placeholders::error)
	);
}

void NetworkImpl::HandleAccept(_session_ptr_t session, const boost::system::error_code& t_errorCode)
{
	if (!t_errorCode)
	{
		session->Start();
	}
	else
	{
		std::cout << "error : " << t_errorCode.message() << std::endl;
	}

	AcceptInner();
}
}
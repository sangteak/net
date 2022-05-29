#pragma once

#include <boost/system/error_code.hpp>

// 내부에서 사용할 boost::system::error_code 재정의
namespace net
{
enum class eErrorCode
{
	SUCCESS = 0,
	NOT_CONNECTED = 1,
};
}

namespace boost
{
namespace system
{
template<> struct is_error_code_enum<net::eErrorCode> : std::true_type
{
};

BOOST_SYSTEM_CONSTEXPR inline boost::system::error_code make_error_code(net::eErrorCode e) BOOST_NOEXCEPT;
BOOST_SYSTEM_CONSTEXPR inline boost::system::error_condition make_error_condition(net::eErrorCode e) BOOST_NOEXCEPT;

namespace detail
{
class excpetion_category : public boost::system::error_category
{
public:
	using __super_t = boost::system::error_category;

	explicit excpetion_category(const char* message)
		: __super_t()
		, m_message(message)
	{
	}

	// Return a short descriptive name for the category
	virtual const char* name() const noexcept override final
	{
		return "exception";
	}
	// Return what each enum means in text
	virtual std::string message(int c) const override final
	{
		return m_message;
	}

	// OPTIONAL: Allow generic error conditions to be compared to me
	virtual boost::system::error_condition default_error_condition(int c) const noexcept override final
	{
		// I have no mapping for this code
		return boost::system::error_condition(c, *this);
	}

private:
	std::string m_message;
};

class custom_error_category : public boost::system::error_category
{
public:
	// Return a short descriptive name for the category
	virtual const char* name() const noexcept override final
	{
		return "custom";
	}
	// Return what each enum means in text
	virtual std::string message(int c) const override final
	{
		switch (static_cast<net::eErrorCode>(c))
		{
		case net::eErrorCode::SUCCESS:
			return "no_error";
		case net::eErrorCode::NOT_CONNECTED:
			return "not_connected";
		default:
			return "unknown";
		}
	}
	// OPTIONAL: Allow generic error conditions to be compared to me
	virtual boost::system::error_condition default_error_condition(int c) const noexcept override final
	{
		auto err = static_cast<net::eErrorCode>(c);
		switch (err)
		{
		case net::eErrorCode::NOT_CONNECTED:
			return boost::system::make_error_condition(err);
		default:
			return boost::system::error_condition(c, *this);
		}
	}
};

template<class T> struct BOOST_SYMBOL_VISIBLE custom_cat_holder
{
	static constexpr custom_error_category instance{};
};

template<class T> constexpr custom_error_category custom_cat_holder<T>::instance;

constexpr boost::system::error_category const& custom_category() BOOST_NOEXCEPT
{
	return boost::system::detail::custom_cat_holder<void>::instance;
}
} // end namepsace detail

// exception에 대한 category는 호출 시 마다 생성한다.
BOOST_SYSTEM_CONSTEXPR inline boost::system::error_code make_error_code(net::eErrorCode e, const char* message) BOOST_NOEXCEPT
{
	return boost::system::error_code(static_cast<int32_t>(e), boost::system::detail::excpetion_category(message));
}

BOOST_SYSTEM_CONSTEXPR inline boost::system::error_code make_error_code(net::eErrorCode e) BOOST_NOEXCEPT
{
	return boost::system::error_code(static_cast<int32_t>(e), boost::system::detail::custom_category());
}

BOOST_SYSTEM_CONSTEXPR inline boost::system::error_condition make_error_condition(net::eErrorCode e) BOOST_NOEXCEPT
{
	return boost::system::error_condition(static_cast<int32_t>(e), boost::system::detail::custom_category());
}
} // end namespace system
} // end namespace boost
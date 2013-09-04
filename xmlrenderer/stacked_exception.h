#ifndef STACKED_EXCEPTION_H
#define STACKED_EXCEPTION_H

#include <string>
#include <stdexcept>
#include <list>
#include <tuple>
#include <boost/current_function.hpp>

namespace webpp {
	class stacked_exception : public std::runtime_error
	{
		typedef std::list<std::tuple<std::string, size_t, std::string, std::string>> messages_t;
		messages_t messages_;
	public:
		stacked_exception(const std::string& msg);
		stacked_exception(const std::runtime_error& base);
		void pushmsg(const std::string& filename, size_t line, const std::string& function, const std::string& msg);

		inline const messages_t& messages() const { return messages_; }
		std::string format() const;
	};
}

#define STACKED_EXCEPTIONS_ENTER() try {
#define STACKED_EXCEPTIONS_LEAVE(msg) } \
	catch(webpp::stacked_exception& e) { e.pushmsg(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION, msg); throw; } \
	catch(const std::runtime_error& e) { webpp::stacked_exception se(e); se.pushmsg(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION, msg); throw se; }


#endif // STACKED_EXCEPTION_H

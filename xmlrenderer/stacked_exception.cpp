#include "stacked_exception.h"
#include <sstream>
namespace webpp {
	stacked_exception::stacked_exception(const std::string& msg)
		: std::runtime_error(msg) {}

	stacked_exception::stacked_exception(const std::runtime_error& base)
		: std::runtime_error(base) {}


	void stacked_exception::pushmsg(const std::string& filename, size_t line, const std::string& function, const std::string& msg) {
		messages_.emplace_back(filename, line, function, msg);
	}

	std::string stacked_exception::format() const {
		std::ostringstream oss;
		unsigned cnt = 1;
		oss << "Exception: " << what() << std::endl;
		for(auto i : messages_) {
			oss << "\t" << cnt << ". " << std::get<0>(i) << ":" << std::get<1>(i) << " - " << std::get<2>(i) << " - " << std::get<3>(i) << std::endl;
			++cnt;
		}
		return oss.str();
	}
}

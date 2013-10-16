#include "xmllib.hpp"
#include "taglib.hpp"
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/date_time.hpp>

void parse_render_values(const std::map<std::string, std::string>& lines, webpp::xml::render::tree_element& rnd) {
	typedef std::map <std::string, std::string> array_element_lines;
	typedef std::map< int, array_element_lines> array_elements;
	typedef std::map< std::string, array_elements > arrays;
	arrays found_arrays;
	for(auto i = lines.cbegin(); i != lines.cend(); ++i) {
		const std::string name = i->first;
		const std::string value = i->second;

		const std::size_t beg = name.find('[');
		const std::size_t end = name.find(']');
		if(beg == std::string::npos && end == std::string::npos) {
			//std::cerr << "Loaded " << name << " = " << value << std::endl;
			if(value == "true")
				rnd.find(name).create_value(true);
			else if(value == "false")
				rnd.find(name).create_value(false);
			else
				rnd.find(name).create_value(value);
		} else if(beg == std::string::npos || end == std::string::npos || end < beg) {
			throw std::runtime_error("invalid render line: " + i->first + " = " + i->second);
		} else {
			const std::string array_name = name.substr(0, beg);
			const std::string array_index = name.substr(beg+1, end-beg-1);
			const std::string array_rest = ( name[end+1] == '.' ? name.substr(end+2) : name.substr(end+1) );
			int index;
			try {
				 index = boost::lexical_cast<int>(array_index);
			} catch(boost::bad_lexical_cast&) {
				throw std::runtime_error("bad cast '" + array_index + "' to int, invalid render line: " + i->first + " = " + i->second);
			}
			found_arrays[array_name][index][array_rest] = value;
		}
	}
	for(const auto& array_element : found_arrays) {
		auto& arr = rnd.find(array_element.first).create_array();
		//std::cerr << "Created array " << array_element.first << std::endl;
		for(const auto& array_element_line : array_element.second) {
			//std::cerr << "Created array element in " << array_element.first << std::endl;
			parse_render_values(array_element_line.second, arr.add());
		}
	}
}

int main(int argc, char **argv) {
	bool bench = false;
	if(argc == 4 && !strcmp(argv[3], "bench")) {
		bench = true;
		--argc;
	}

	if(argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <render values file> <xml template file>\n";
		return 1;
	}
	webpp::xml::context ctx(".");
	ctx.load_taglib<webpp::xml::taglib::basic>();
	std::ifstream xmlfile(argv[2]);
	assert(xmlfile);
	std::ostringstream oss;
	oss << xmlfile.rdbuf();
	xmlfile.close();
	ctx.put("testfile", oss.str());

	std::ifstream file(argv[1]);
	assert(file);
	std::map<std::string, std::string> lines;
	std::string line;
	while(std::getline(file, line)) {
		std::size_t p = line.find(' ');
		if(p == std::string::npos)
			throw std::runtime_error("invalid render line: " + line);
		lines.emplace(line.substr(0, p), line.substr(p+1));
	}
	if(bench) {
		int n = 1e2, i = n;
		webpp::xml::render::context rnd;
		parse_render_values(lines, rnd.get(""));
		auto now = boost::posix_time::microsec_clock::universal_time();
		while(i--) {
			std::string result = ctx.get("testfile").render(rnd).to_string();
		}
		auto later = boost::posix_time::microsec_clock::universal_time();
		auto process_time = (later-now).ticks();
		double perreq = (double)process_time/n/1000000;
		std::cout << n << " rendered documents, total time " << (double)process_time/1000000 << " seconds, " << perreq << " per request\n";
	} else {
		try {
			webpp::xml::render::context rnd;
			parse_render_values(lines, rnd.get(""));
			std::cout << ctx.get("testfile").render(rnd).to_string() << std::endl;
		} catch(const webpp::stacked_exception& e) {
			std::cerr << e.format();
			throw;
		}
	}
}

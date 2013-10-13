#include "test_parser.hpp"
#include <string>
#include <iostream>

int main() {
	std::string line;
	while(std::getline(std::cin, line)) {
		webpp::xml::expressions::print_expression_ast(line);
	}
	return 0;
}

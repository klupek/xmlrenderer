#ifndef WEBPP_XML_TEST_PARSER_HPP
#define WEBPP_XML_TEST_PARSER_HPP

#include "xmllib.hpp"
#include <webpp-common/stacked_exception.hpp>
#include <memory>
#include <boost/spirit/include/qi.hpp>

namespace webpp { namespace xml {
namespace expressions {
	namespace qi = boost::spirit::qi;

	class error : public webpp::stacked_exception {
		std::shared_ptr<error> reason_;
		std::string token_, value_, msg_;
	public:
		error(const std::string& token, const std::string& value, const std::string& msg);
		error(const std::string& token, const std::string& value, const error& reason);
		error(const std::string& token, const std::string& value, const std::runtime_error& reason);

		static std::string build_error_msg(const error* e, const std::string& token, const std::string& value, const std::string& msg);
	};

	class base {
	public:
		struct value_t {
			enum class type_t { integer, real, string, unknown } type;
			boost::any value_;
			static std::string type_name(type_t);
		};

		enum class operand {
				EQ, // 'expr = expr', L/R must be the same type (string, int, ...)
				NE, // 'expr != expr', as above

				LT, // 'expr < expr', L must be integer/double
				LE, // 'expr <= expr', as above
				GT, // 'expr > expr', as above
				GE, // 'expr >= expr', as above

				IS_TRUE, // 'expr is true', expr must be bool
				IS_NOT_TRUE,
				IS_EMPTY, // 'expr is empty', expr can be anything
				IS_NOT_EMPTY,
				IS_NULL, IS_NOT_NULL
		};
		virtual ~base() {}

		static std::string operand_name(const operand op);

		virtual bool evaluate(render::context&) const = 0;
		virtual render::tree_element& get_tree_element(render::context&) const = 0;
		virtual std::string to_string() const = 0;
		virtual value_t get_value(render::context&) const = 0;
	};

	typedef std::shared_ptr<base> expression_ptr;

	template <typename T>
	struct strict_real_policies : qi::real_policies<T>
	{
		static bool const expect_dot = true;
	};

	struct unescaped_string
	  : qi::grammar<std::string::const_iterator, std::string(char const*)>
	{
		unescaped_string();
		qi::rule<std::string::const_iterator, std::string(char const*)> unesc_str;
		qi::symbols<char const, char const> unesc_char;
	};

	struct expression_grammar : qi::grammar<std::string::const_iterator, expression_ptr()> {
			expression_grammar();

			unescaped_string p;
			qi::real_parser< double, strict_real_policies<double> > qi_real;
			qi::rule<std::string::const_iterator, expression_ptr()> and_rule, or_rule, expr, atom,result;
			qi::rule<std::string::const_iterator, expression_ptr()> and_suffix, or_suffix, function_rule;
			qi::rule<std::string::const_iterator, std::string()> variable_rule, function_name_rule;
			qi::rule<std::string::const_iterator, std::string()> literal_rule;
			qi::symbols<char, base::operand> operands1, operands2;
	};

	bool evaluate_test_expression(const std::string& expression, render::context& rnd);
}}}

#endif // WEBPP_XML_TEST_PARSER_HPP

#include "test_parser.hpp"

#include <boost/fusion/adapted.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/nview.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/lexical_cast.hpp>

namespace webpp { namespace xml { namespace expressions {
	struct literal_expression : public base {
			std::string literal_;
			literal_expression(const std::string& v);

			virtual bool evaluate(render::context&) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual std::string to_string() const;
			virtual value_t get_value(render::context&) const;
	};

	struct variable_expression : public base {
			std::string variable_;
			variable_expression(const std::string& v);

			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context& rnd) const;

			virtual std::string to_string() const;
			virtual value_t get_value(render::context& rnd) const;
	};

	struct function_expression : public base {
			std::string function_, variable_;
			function_expression(const std::string& name);

			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual std::string to_string() const;
			virtual value_t get_value(render::context & rnd) const;
	};


	struct integer_expression : public base {
			int integer_;
			integer_expression(const int v);
			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual std::string to_string() const;
			virtual value_t get_value(render::context &) const;
	};

	struct real_expression : public base {
			double real_;
			real_expression(const double v);

			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual std::string to_string() const;
			virtual value_t get_value(render::context &) const;
	};

	struct oneop_expression : public base {
		expression_ptr lhs_;
		base::operand op_;
		oneop_expression(expression_ptr lhs, base::operand op);
		virtual bool evaluate(render::context& rnd) const;
		virtual render::tree_element& get_tree_element(render::context&) const;
		virtual value_t get_value(render::context &) const;
		virtual std::string to_string() const;
	};

	struct twoop_expression : public base {
		expression_ptr lhs_, rhs_;
		base::operand op_;
		twoop_expression(expression_ptr lhs, base::operand op, expression_ptr rhs);
		virtual bool evaluate(render::context& rnd) const;
		virtual render::tree_element& get_tree_element(render::context&) const;
		virtual value_t get_value(render::context &) const;
		virtual std::string to_string() const;
	};

	struct and_expression : public base {
		expression_ptr lhs_, rhs_;
		and_expression(expression_ptr lhs, expression_ptr rhs);

		virtual bool evaluate(render::context& rnd) const;
		virtual render::tree_element& get_tree_element(render::context&) const;
		virtual value_t get_value(render::context &) const;
		virtual std::string to_string() const;
	};

	struct or_expression : public base {
			expression_ptr lhs_, rhs_;
			or_expression(expression_ptr lhs, expression_ptr rhs);

			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual value_t get_value(render::context &) const;
			virtual std::string to_string() const;
	};

	struct not_expression : public base {
			expression_ptr rhs_;
			not_expression(expression_ptr rhs);

			virtual bool evaluate(render::context& rnd) const;
			virtual render::tree_element& get_tree_element(render::context&) const;
			virtual value_t get_value(render::context &) const;
			virtual std::string to_string() const;
	};

	unescaped_string::unescaped_string()
	  : unescaped_string::base_type(unesc_str)
	{
		unesc_char.add("\\a", '\a')("\\b", '\b')("\\f", '\f')("\\n", '\n')
					  ("\\r", '\r')("\\t", '\t')("\\v", '\v')
					  ("\\\\", '\\')("\\\'", '\'')("\\\"", '\"')
		;

		unesc_str = qi::lit(qi::_r1)
			>> *(unesc_char | qi::alnum | qi::ascii::space | "\\x" >> qi::hex)
			>>  qi::lit(qi::_r1)
		;
	}

	bool evaluate_test_expression(const std::string& expression, render::context& rnd) {
		STACKED_EXCEPTIONS_ENTER()
		using qi::phrase_parse;
		using qi::ascii::space;
		std::string::const_iterator saved = expression.begin(), begin = expression.begin(), end = expression.end();

		expression_ptr ex;
		bool r = phrase_parse(begin, end, expression_grammar(), space, ex);
		if(begin != end || !r) {
			throw std::runtime_error("Parse failed, stopped at character "
									 + boost::lexical_cast<std::string>(begin-saved)
									 + ": " + std::string(begin, end));
		} else {
			return ex->evaluate(rnd);
		}
		STACKED_EXCEPTIONS_LEAVE("evaluate test expression: " + expression);
	}

	void print_expression_ast(const std::string& expression) {
		STACKED_EXCEPTIONS_ENTER()
		using qi::phrase_parse;
		using qi::ascii::space;
		std::string::const_iterator saved = expression.begin(), begin = expression.begin(), end = expression.end();

		expression_ptr ex;
		bool r = phrase_parse(begin, end, expression_grammar(), space, ex);
		if(begin != end || !r) {
			throw std::runtime_error("Parse failed, stopped at character "
									 + boost::lexical_cast<std::string>(begin-saved)
									 + ": " + std::string(begin, end));
		} else {
			std::cout << ex->to_string();
		}
		STACKED_EXCEPTIONS_LEAVE("evaluate test expression: " + expression);
	}

	expression_grammar::expression_grammar()
			: expression_grammar::base_type(result) {
			using qi::lit;
			using qi::lexeme;
			using qi::double_;
			using qi::int_;
			using qi::phrase_parse;
			using qi::ascii::space;
			using qi::_1;
			using qi::_2;
			using qi::_3;
			using qi::_4;
			using qi::_5;
			using boost::phoenix::push_back;
			using boost::phoenix::construct;
			using boost::phoenix::new_;
			using qi::char_;
			using boost::phoenix::val;
			using qi::no_skip;
			using qi::on_error;
			using qi::fail;
			using namespace qi::labels;

			char const* quote = "'";
			operands2.add("=",base::operand::EQ)("!=",base::operand::NE)("<=",base::operand::LE)("<",base::operand::LT)(">=",base::operand::GE)(">",base::operand::GT);
			operands1.add("is true", base::operand::IS_TRUE)("is empty", base::operand::IS_EMPTY)
					("is not true", base::operand::IS_NOT_TRUE)("is false", base::operand::IS_NOT_TRUE)
					("is not empty", base::operand::IS_NOT_EMPTY)
					("is null",base::operand::IS_NULL)("is not null", base::operand::IS_NOT_NULL);

			literal_rule = ( p(quote) );
			variable_rule = ( ( qi::lower | qi::upper | char_('_') ) >> *( char_('.') | qi::alnum  | char_('_') | char_('-')) );

			result %= expr;

			function_name_rule %= ( qi::lower | qi::upper );

			function_rule =
					( variable_rule >> lit("()") ) [ _val = construct<expression_ptr>(new_<function_expression>(_1)) ];

			atom =
					function_rule [ _val = _1 ]
					| variable_rule [ _val = construct<expression_ptr>(new_<variable_expression>(_1)) ]
					| literal_rule [ _val = construct<expression_ptr>(new_<literal_expression>(_1)) ]
					| qi_real [ _val = construct<expression_ptr>(new_<real_expression>(_1)) ]
					| int_ [ _val = construct<expression_ptr>(new_<integer_expression>(_1)) ];

			expr =
					( lit("not") >> *space >> lit('(') >> or_rule >> *space >> lit(")") ) [ _val = construct<expression_ptr>(new_<not_expression>(_2)) ]
					| ( lit('(') >> *space >> or_rule >> *space >> lit(')') ) [ _val = _2 ]
					| ( atom >> +space >> operands1 ) [ _val = construct<expression_ptr>(new_<oneop_expression>(_1, _3)) ]
					| ( atom >> *space >> operands2 >> *space >> atom ) [ _val = construct<expression_ptr>(new_<twoop_expression>(_1, _3, _5)) ];

			and_suffix = ( +space >> "and" >> +space >> expr ) [ _val = _3 ];
			and_rule =
					( expr ) [ _val = _1 ]
					 >>  *( and_suffix [ _val = construct<expression_ptr>(new_<and_expression>(_val, _1)) ] );

			or_suffix = ( +space >> "or" >> +space >> and_rule ) [ _val = _3 ];
			or_rule =
					( and_rule ) [ _val = _1 ]
					>> *( or_suffix [ _val = construct<expression_ptr>(new_<or_expression>(_val, _1)) ]) ;
			result = or_rule [ _val = _1 ];

			on_error<fail>
					(
					 result
					 , std::cerr
					 << val("Error! Expecting ")
					 << _4                               // what failed?
					 << val(" here: \"")
					 << construct<std::string>(_3, _2)   // iterators to error-pos, end
					 << val("\"")
					 << std::endl
					);
	}

	std::string error::build_error_msg(const error* e, const std::string& token, const std::string& value, const std::string& msg) {
		std::ostringstream result;
		result << "Expression error: " << msg << "\n";
		const error* c = e;
		int i = 2;
		result << "1. At token " << token << "(value = " << value << ")\n";
		while(c != nullptr) {
			result << i << ". At token " << c->token_ << "(value = " << c->value_ << ")\n";
			++i;
			c = c->reason_.get();
		}
		return result.str();
	}

	error::error(const std::string& token, const std::string& value, const std::string& msg)
			: webpp::stacked_exception(build_error_msg(nullptr, token, value, msg)), token_(token), value_(value),msg_(msg) {}

	error::error(const std::string& token, const std::string& value, const error& reason)
		: webpp::stacked_exception(build_error_msg(&reason, token, value, reason.msg_)), reason_(new error(reason)), token_(token), value_(value), msg_(reason.msg_) {}

	error::error(const std::string& token, const std::string& value, const std::runtime_error& reason)
		: webpp::stacked_exception(build_error_msg(nullptr, token, value, reason.what())), token_(token), value_(value), msg_(reason.what()) {}

	std::string base::value_t::type_name(type_t t) {
		switch(t) {
			case type_t::integer: return "integer";
			case type_t::real: return "real";
			case type_t::string: return "string";
			case type_t::unknown: return "unknown";
		}
	}

	std::string base::operand_name(const operand op) {
			switch(op) {
			case operand::EQ: return "eq";
			case operand::NE: return "ne";
			case operand::LT: return "lt";
			case operand::LE: return "le";
			case operand::GT: return "gt";
			case operand::GE: return "ge";
			case operand::IS_TRUE: return "is_true";
			case operand::IS_NOT_TRUE: return "is_not_true";
			case operand::IS_EMPTY: return "is_empty";
			case operand::IS_NOT_EMPTY: return "is_not_empty";
			case operand::IS_NULL: return "is_null";
			case operand::IS_NOT_NULL: return "is_not_null";

			}
	}

	literal_expression::literal_expression(const std::string& v) : literal_(v) {}
	bool literal_expression::evaluate(render::context&) const {
		throw error("string", literal_, "String can not be evaluated as boolean expression");
	}
	render::tree_element& literal_expression::get_tree_element(render::context&) const {
		throw error("string", literal_, "Expected variable");
	}
	std::string literal_expression::to_string() const {
		return "string(" + literal_ + ")";
	}

	base::value_t literal_expression::get_value(render::context&) const {
		return value_t { value_t::type_t::string, literal_ };
	}

	variable_expression::variable_expression(const std::string& v) : variable_(v) {}

	bool variable_expression::evaluate(render::context&) const {
		throw error("variable", variable_, "Variable can not be evaluated as boolean expression, use 'foo is true' instead");
	}
	render::tree_element& variable_expression::get_tree_element(render::context& rnd) const {
		return rnd.get(variable_);
	}

	std::string variable_expression::to_string() const {
		return "variable(" + variable_ + ")";
	}

	base::value_t variable_expression::get_value(render::context& rnd) const {
		auto& v = rnd.get(variable_);
		if(v.empty())
			throw std::runtime_error("Variable is null: " + variable_);
		return value_t { value_t::type_t::unknown, std::string(v.get_value().output()) };
	}

	function_expression::function_expression(const std::string& name) {
			std::size_t lastdot = name.find_last_of('.');
			if(lastdot == std::string::npos) {
					variable_ = std::string();
					function_ = name;
			} else {
					variable_ = name.substr(0, lastdot);
					function_ = name.substr(lastdot+1);
			}
	}

	bool function_expression::evaluate(render::context&) const {
		throw error("function", variable_ + "." + function_ + "()", "Function can not be evaluated as boolean expression, use 'foo.bar() is true' instead");
	}
	render::tree_element& function_expression::get_tree_element(render::context&) const {
		throw error("function", variable_ + "." + function_ + "()", "Expected variable");
	}
	std::string function_expression::to_string() const {
		return "function(" + variable_ + "." + function_ + "())";
	}
	base::value_t function_expression::get_value(render::context & rnd) const {
		if(function_ == "size") {
			auto& v = rnd.get(variable_);
			if(!v.is_array())
				throw std::runtime_error("size(): variable is not array: " + variable_);
			return value_t { value_t::type_t::integer, static_cast<int>(v.get_array().size()) };

		} else {
			throw std::runtime_error("Unknown function " + function_ + ": " + variable_ + "." + function_ + "()");
		}
	}

	integer_expression::integer_expression(const int v) : integer_(v) {}

	bool integer_expression::evaluate(render::context&) const {
		throw error("integer", boost::lexical_cast<std::string>(integer_), "Integer can not be evaluated as boolean expression");
	}
	render::tree_element& integer_expression::get_tree_element(render::context&) const {
		throw error("integer", boost::lexical_cast<std::string>(integer_), "Expected variable");
	}
	std::string integer_expression::to_string() const {
		return "integer(" + boost::lexical_cast<std::string>(integer_) + ")";
	}

	base::value_t integer_expression::get_value(render::context &) const {
		return value_t { value_t::type_t::integer, integer_ };
	}

	real_expression::real_expression(const double v) : real_(v) {}
	bool real_expression::evaluate(render::context&) const {
		throw error("real", boost::lexical_cast<std::string>(real_), "Real can not be evaluated as boolean expression");
	}
	render::tree_element& real_expression::get_tree_element(render::context&) const {
		throw error("real", boost::lexical_cast<std::string>(real_), "Expected variable");
	}
	std::string real_expression::to_string() const {
		return "real(" + boost::lexical_cast<std::string>(real_) + ")";
	}

	base::value_t real_expression::get_value(render::context &) const {
		return value_t { value_t::type_t::real, real_ };
	}

	oneop_expression::oneop_expression(expression_ptr lhs, base::operand op)
			: lhs_(lhs), op_(op) {}
	bool oneop_expression::evaluate(render::context& rnd) const {
		try {
			render::tree_element& t = lhs_->get_tree_element(rnd);
			switch(op_) {
				case base::operand::IS_NULL:
					return t.empty();

				case base::operand::IS_NOT_NULL:
					return t.is_array() || t.is_value();

				case base::operand::IS_NOT_EMPTY:
					return t.is_array() && !t.get_array().empty();

				case base::operand::IS_EMPTY:
					return !t.is_array() || t.get_array().empty();

				case base::operand::IS_TRUE:
					if(!t.is_value())
						throw std::runtime_error("Expected boolean value");
					return t.get_value().is_true();
				case base::operand::IS_NOT_TRUE:
					if(!t.is_value())
						throw std::runtime_error("Expected boolean value");
					return !t.get_value().is_true();
				default:
					throw std::logic_error("oneop does not support " + base::operand_name(op_));
			}
		} catch(const error& e) {
			throw error(base::operand_name(op_), lhs_->to_string(), e);
		} catch(const std::runtime_error& e) {
			throw error(base::operand_name(op_), lhs_->to_string(), e);
		}
	}

	render::tree_element& oneop_expression::get_tree_element(render::context&) const {
		throw error(base::operand_name(op_), lhs_->to_string(), "Expected variable");
	}

	base::value_t oneop_expression::get_value(render::context &) const {
		throw error(base::operand_name(op_), lhs_->to_string(), "Expected atom");
	}

	std::string oneop_expression::to_string() const {
		return base::operand_name(op_) + "(" + lhs_->to_string() + ")";
	}

	template<typename T>
	bool cast_and_compare_impl(const base::operand op, const base::value_t& lhs, const base::value_t& rhs, const std::string& stringrep) {
		try {
			const T l = boost::any_cast<T>(lhs.value_);
			const T r = boost::any_cast<T>(rhs.value_);
			//std::cout << "op " << base::operand_name(op) << ": " << l << ", " << r << std::endl;
			switch(op) {
				case base::operand::EQ: return l == r;
				case base::operand::NE: return l != r;
				case base::operand::GE: return l >= r;
				case base::operand::GT: return l > r;
				case base::operand::LE: return l <= r;
				case base::operand::LT: return l < r;
				default: throw std::logic_error("cast_and_compare does not support " + base::operand_name(op));

			}
		} catch(const boost::bad_any_cast& e) {
			throw error("cast_and_compare_impl", stringrep, e.what());
		}
	}

	bool cast_and_compare(const base::operand op, const base::value_t::type_t type, const base::value_t& lhs, const base::value_t& rhs, const std::string& stringrep) {
		switch(type) {
			case base::value_t::type_t::integer: return cast_and_compare_impl<int>(op, lhs, rhs, stringrep);
			case base::value_t::type_t::real: return cast_and_compare_impl<double>(op, lhs, rhs, stringrep);
			case base::value_t::type_t::string: return cast_and_compare_impl<std::string>(op, lhs, rhs, stringrep);
			default: throw std::logic_error("cast_and_compare should not be called with type=unknown");
		}
	}

	twoop_expression::twoop_expression(expression_ptr lhs, base::operand op, expression_ptr rhs)
			: lhs_(lhs), rhs_(rhs), op_(op)  {}
	bool twoop_expression::evaluate(render::context& rnd) const {
		try {
				const value_t rhs = rhs_->get_value(rnd);
				const value_t lhs = lhs_->get_value(rnd);
				// one of the expression sides is unknown type
				if(rhs.type == value_t::type_t::unknown || lhs.type == value_t::type_t::unknown) {
					// two variables, no other way then compare string representation of them
					// TODO: virtual equal() for render::value?
					if(rhs.type == lhs.type) {
						return cast_and_compare_impl<std::string>(op_, lhs, rhs, to_string());
					} else if(rhs.type == value_t::type_t::unknown) {
						// left is known, right is unknown, cast right to left's type
						return cast_and_compare(op_, lhs.type, lhs, rhs, to_string());
					} else {
						// right is known, left is unknonw, cast left to right's type
						return cast_and_compare(op_, rhs.type, lhs, rhs, to_string());
					}
				} else if(rhs.type == lhs.type) {
					// known type on both sides
					return cast_and_compare(op_, rhs.type, lhs, rhs, to_string());
				} else {
					// different types on right and left side
					throw std::runtime_error("Could not use operator " + base::operand_name(op_) + " on different types: "
											 + value_t::type_name(lhs.type) + "(" + lhs_->to_string() + ") and "
											 + value_t::type_name(rhs.type) + "(" + rhs_->to_string() + ")");
				}
		} catch(const error& e) {
			throw error(base::operand_name(op_), lhs_->to_string() + "," + rhs_->to_string(), e);
		} catch(const std::runtime_error& e) {
			throw error(base::operand_name(op_), lhs_->to_string() + "," + rhs_->to_string(), e);
		}
	}

	render::tree_element& twoop_expression::get_tree_element(render::context&) const {
		throw error(base::operand_name(op_), lhs_->to_string() + "," + rhs_->to_string(), "Expected variable");
	}

	base::value_t twoop_expression::get_value(render::context &) const {
		throw error(base::operand_name(op_), lhs_->to_string(), "Expected atom");
	}

	std::string twoop_expression::to_string() const { return operand_name(op_) + "(" + lhs_->to_string() + "," + rhs_->to_string() + ")"; }

	and_expression::and_expression(expression_ptr lhs, expression_ptr rhs)
			: lhs_(lhs), rhs_(rhs) {}

	bool and_expression::evaluate(render::context& rnd) const {
		return lhs_->evaluate(rnd) && rhs_->evaluate(rnd);
	}

	render::tree_element& and_expression::get_tree_element(render::context&) const {
		throw error("and", lhs_->to_string() + "," + rhs_->to_string(), "Expected variable");
	}

	base::value_t and_expression::get_value(render::context &) const {
		throw error("and", lhs_->to_string() + "," + rhs_->to_string(), "Expected atom");
	}
	std::string and_expression::to_string() const {
		return "and(" + lhs_->to_string() + "," + rhs_->to_string() + ")";
	}

	or_expression::or_expression(expression_ptr lhs, expression_ptr rhs)
			: lhs_(lhs), rhs_(rhs) {}

	bool or_expression::evaluate(render::context& rnd) const {
		return lhs_->evaluate(rnd) || rhs_->evaluate(rnd);
	}
	render::tree_element& or_expression::get_tree_element(render::context&) const {
		throw error("or", lhs_->to_string() + "," + rhs_->to_string(), "Expected variable");
	}
	base::value_t or_expression::get_value(render::context &) const {
		throw error("or", lhs_->to_string() + "," + rhs_->to_string(), "Expected atom");
	}
	std::string or_expression::to_string() const {
		return "or(" + lhs_->to_string() + "," + rhs_->to_string() + ")";
	}

	not_expression::not_expression(expression_ptr rhs) : rhs_(rhs) {}

	bool not_expression::evaluate(render::context& rnd) const {
		return !rhs_->evaluate(rnd);
	}
	render::tree_element& not_expression::get_tree_element(render::context&) const {
		throw error("not", rhs_->to_string(), "Expected variable");
	}
	base::value_t not_expression::get_value(render::context &) const {
		throw error("not", rhs_->to_string(), "Expected atom");

	}
	std::string not_expression::to_string() const {
		return "not(" + rhs_->to_string() + ")";
	}


}}}


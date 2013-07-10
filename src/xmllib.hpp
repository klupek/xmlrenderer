#ifndef WEBPP_XMLLIB
#define WEBPP_XMLLIB

#include <libxml++-2.6/libxml++/libxml++.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/unordered_map.hpp>
#include <boost/type_traits.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <type_traits>
#include <cstdarg>
#include <iomanip>
#include <fstream>
#include <functional>
#include <memory>
#include <list>
#include <cstring>
#include <cassert>
#include "stacked_exception.h"
namespace boost {
	// forward hash<Glib::ustring> to std::string's hash
	template<>
	struct hash<Glib::ustring> {
		size_t operator()(const Glib::ustring& v) const {
			return hash<std::string>()(v);
		}
	};
}

namespace webpp { namespace xml {

	/// abstract interface for values in render context
	/// supports output() - lexical cast to string 
	/// and format(fmt), where fmt is argument for boost::format(fmt) % value
	class render_value_base : public boost::noncopyable {
	public:
		virtual Glib::ustring format(const Glib::ustring& fmt) const = 0;
		virtual Glib::ustring output() const = 0;
		virtual bool is_true() const = 0;
	};

	/// default implementations of render_value interface
	template<typename T>
	class render_value : public render_value_base {
		const T value_;
	public:
		render_value(const T& value) 
			: value_(value) {}
		render_value(const render_value&) = delete;
		render_value& operator=(const render_value&) = delete;

		virtual Glib::ustring format(const Glib::ustring& fmt) const {
			return (boost::format(fmt) % value_).str();
		}

		virtual Glib::ustring output() const {
			return boost::lexical_cast<Glib::ustring>(value_);
		}

		virtual bool is_true() const {
			STACKED_EXCEPTIONS_ENTER();
			throw std::runtime_error("render_value<" + Glib::ustring(typeid(T).name()) + ">::is_true(): '" + output() + "' is not a boolean");
			STACKED_EXCEPTIONS_LEAVE("");
		}
	};
	
	// char literals are stored as Glib::ustring
	template< std::size_t N >
	class render_value<char[N]> : public render_value<Glib::ustring> {
	public:
		render_value(const char value[N]) 
			: render_value<Glib::ustring>(value) {}		
	};

	//! \brief Lazy evaluated function/lambda/bind/any callable. Will execute once requested from renderer and then value will be cached.
	template <typename T>
	class render_function : public render_value_base {
		T lambda_;
		typedef decltype(lambda_()) return_type;
		mutable std::unique_ptr<render_value<return_type>> value_;

		render_value<return_type>& eval() const {
			if(!value_)
				value_.reset(new render_value<return_type>(lambda_()));
			return *value_;
		}

	public:
		render_function(T&& value)
			: lambda_(std::forward<T>(value)) {}

		render_function(const render_function&) = delete;
		render_function& operator=(const render_function&) = delete;

		virtual Glib::ustring format(const Glib::ustring& fmt) const {
			return eval().format(fmt);
		}

		virtual Glib::ustring output() const {
			return eval().output();
		}

		virtual bool is_true() const {
			return eval().is_true();
		}
	};

	template<>
	bool render_value<bool>::is_true() const;

	/// \brief Key-[Array|Value] tree, each node may be one of: empty (transitive node), value (of any type), array of subtrees
	class render_context_tree_element {
		typedef boost::unordered_map<Glib::ustring, render_context_tree_element> children_t;
		mutable children_t children_;
		typedef std::list<render_context_tree_element> array_t;
		array_t array_;
		std::shared_ptr<render_value_base> value_;

	public:
		//! \brief Find tree element stored under key in this subtree. Every key exists in tree, but only some of them have associated variables or arrays
		render_context_tree_element& find(const Glib::ustring& key) {
			if(key.empty()) 
				return *this;
			else {
				const auto position = key.find('.');
				if(position == Glib::ustring::npos)
					return children_[key.substr(0, position)];
				else
					return children_[key.substr(0, position)].find(key.substr(position+1));
			}
		}
	
		//! \brief Get value stored under this tree element.
		const render_value_base& value() const {
			return *value_.get();
		}

		//! \brief Start iterating through array stored in this tree element
		array_t::const_iterator begin() const {
			return array_.begin();
		}
		
		//! \brief Next-to-last element in array stored in this tree element
		array_t::const_iterator end() const {
			return array_.end();
		}

		//! \brief Store value of any type in this tree element.
		template<typename T>
		void put_value(const T& v) {
			value_.reset(new render_value<T>(v));
		}

		//! \brief Store lazy evaluated callable in this tree element.
		template<typename T>
		void put_lambda(T&& v) {
			value_.reset(new render_function<T>(std::forward<T>(v)));
		}

		//! \brief Add one element to array and return reference to it.
		render_context_tree_element& add_to_array() {
			array_.emplace_back();
			return array_.back();
		}

		//! \brief Check if there is no value, no lambda and no array elements.
		bool empty() const { 
			return !value_ && array_.empty();
		}

		//! \brief Check if value or lambda is set.
		bool has_value() const {
			return !!value_;
		}

		void debug(int tab = 0) const {
			if(has_value())
				std::cout << std::setw(tab*5) << "" <<  ".: " << value_->output() << std::endl;
			else
				std::cout << std::setw(tab*5) << "" << ".: (null)" << std::endl;
			if(begin() != end()) {
				std::cout << std::setw(tab*5) << "" << "array elements\n";
				for(auto e : *this) {
					e.debug(tab+1);
				}
			}

			if(!children_.empty()) {
				std::cout << std::setw(tab*5) << "" << "children\n";
				for(auto child : children_) {
					std::cout << std::setw((tab+1)*5) << ""<< "child " << child.first << ":\n";
					child.second.debug(tab+2);
				}
			}
		}
	};



	/// class which stores values for rendering fragment(s)
	class render_context {
		render_context_tree_element root_;
	public:
		/// store value (copied) under key
		template<typename T>
		render_context& val(const Glib::ustring& key, const T& value) {
			root_.find(key).put_value(value);
			return *this;
		}

		/// store reference under key, reference must be valid during rendering
		template<typename T>
		render_context& ref(const Glib::ustring& key, const T& value) {
			root_.find(key).put_value(value);
			return *this;
		}

		/// store lazy evaulated lambda under key, any lambda reference captures must be valid during rendering
		template<typename T>
		render_context& lambda(const Glib::ustring& key, T&& value) {
			root_.find(key).put_lambda(std::forward<T>(value));
			return *this;
		}

		/// copy subtree to new key
		void put(const Glib::ustring& key, const render_context_tree_element& subtree) {
		//	std::cout << "subtree = \n";
			//subtree.debug();
			//std::cout << "root before = \n";
			//root_.debug();
			root_.find(key) = subtree;
			//std::cout << "root after = \n";
			//root_.debug();

		}
		
		/// add to array
		render_context_tree_element& add_to_array(const Glib::ustring& key) {
			return root_.find(key).add_to_array();
		}

		/// find by name, descend from root_
		const render_context_tree_element& get(const Glib::ustring& key);
	};



	class context;
// short macro for checking existance of variable in rendering context
#define ctx_variable_check(tag, attribute, variablename, rndvalue) if(rndvalue.empty()) throw std::runtime_error((boost::format("variable '%s' required from <%s> at line %d, attribute %s, is missing") % variablename % tag->get_name() % tag->get_line() % attribute).str())

	/// \brief Piece of html5/xml, which was rendered from fragment. Can be modified and then converted to ustring.
	class fragment_output {
		const Glib::ustring name_; // for exception decorating
		std::unique_ptr<xmlpp::Document> output_; // mutable, because to_string() is obviously const, and libxml++ thinks different.
	public:
		/// \brief Construct empty document
		fragment_output(const Glib::ustring& name);
		fragment_output(fragment_output&&);

		/// \brief Find all nodes matching to XPath expression
		//xmlpp::NodeSet find_by_xpath(const Glib::ustring& query) const;

		//! \brief Convert XML tree to string
		Glib::ustring to_string() const;
	private:
		inline xmlpp::Document& document() { return *output_; }

		friend class fragment;
	};

	/// \brief Piece of html5/xml, which is stored and then rendered using render_context and its values
	class fragment : public boost::noncopyable {
		const Glib::ustring name_;
		context& context_;
		xmlpp::DomParser reader_;
	public:
		/// \brief Load fragment from file 'filename'
		fragment(const Glib::ustring& filename, context& ctx);
		
		/// \brief Load fragment from string 'buffer'
		fragment(const Glib::ustring& name, const Glib::ustring& buffer, context& ctx);
		
		fragment(const fragment&) = delete;
		fragment& operator=(const fragment&) = delete;
				
		/// \brief render this fragment, return XML in string
		fragment_output render(render_context& rnd);
	private:
		// list of modifiers for single attribute
		typedef std::map<Glib::ustring, Glib::ustring> modifier_list_t;
		// list of attribute with modifiers
		typedef std::map<Glib::ustring, modifier_list_t> modifier_map_t;

		/// \brief Process node 'src' and its children, put generated output into 'dst'
		void process_node(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd, bool already_processing_outer_repeat = false);
		/// \brief Process children of 'src' and put generated output as children of 'dst
		void process_children(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd); 
	};
	
	
	/*! \brief Handler for custom XML tags
	 * 	\example <f:input name="user.name" />
	 */
	class tag {
	public:
		/// \brief render as TEXT node to 'dst', using 'src' for attribute source and 'ctx' to value source		
		virtual void render(xmlpp::Element* dst, const xmlpp::Element* src, render_context& ctx) const = 0;
	};

	/*! \brief Handle all attributes and tags in namespace
	 *  \example <a f:href="/users/#{user.name}" f:title="user #[user.name} - abuse level #{user.abuse|%.2f]">
	 */
	class xmlns {
	public:
		/// Process tag 'src' and place result as child element in 'dst'
		virtual void tag(xmlpp::Element* dst, const xmlpp::Element* src, render_context& ctx) const = 0;
		/// Process attribute 'src' and place results (attributes) inside element 'dst'
		virtual void attribute(xmlpp::Element* dst, const xmlpp::Attribute* src, render_context& ctx) const = 0;
	};

	/*! \class context
	 *  \brief Container for XML fragments and support XML tag and subattribute objects
	 */
	class context {
		const boost::filesystem::path library_directory_;
		boost::unordered_map<Glib::ustring, std::shared_ptr<fragment> > fragments_;
		/// <wpp:foobar ... />
		boost::unordered_map<std::pair<Glib::ustring,Glib::ustring>, std::unique_ptr<tag> > tags_;
		/// <a href.value="variable-name" href.format="%.3lf">
		boost::unordered_map<Glib::ustring, std::unique_ptr<xmlns>> xmlnses_;
		
	public:		
		/*! \brief Construct context
		 * 	\param library_directory directory with fragment files
		 */
		context(const std::string& library_directory);

		/*! \brief Load fragment from library
		 * 	\param name fragment path in library
		 */
		void load(const std::string& name);
		/*! \brief Load fragment from in-memory string
		 * 	\param name fragment name
		 * 	\param data string containing XML data
		 */					
		void put(const Glib::ustring& name, const Glib::ustring& data);

		/// \brief Load tag library _Tgt
		template<typename _Tgt>
		void load_taglib() {
			_Tgt::process(tags_, xmlnses_);
		}

		/// \brief Find or load fragment named 'name', throw exception if not found
		fragment&  get(const Glib::ustring& name);

		/*! \brief Find tag handler named 'name' in 'ns' namespace, returns nullptr if not found
		 * 	\param ns namespace URI
		 * 	\param name tag name
		 */		
		const tag* find_tag(const Glib::ustring& ns, const Glib::ustring& name);
		
		/// \brief Find xmlns handler for uri 'ns', returns nullptr if not found
		const xmlns* find_xmlns(const Glib::ustring& ns);
	};
}}

#endif // WEBPP_XMLLIB

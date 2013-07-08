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
#include <fstream>
#include <functional>
#include <memory>
#include <list>
#include <cstring>
#include <cassert>

/* zalozenia:
	- nazwy tagów i atrybutów sa czystym ascii, bez udziwnien i ogonków
	- zawartosc nodow i atrybutow jest parsowana jako unicode
*/
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
			throw std::runtime_error("webpp::xml::render_value<" + std::string(typeid(T).name()) + ">::is_true(): '" + output() + "' is not a boolean");
		}
	};
	
	template< std::size_t N >
	class render_value<char[N]> : public render_value<Glib::ustring> {
	public:
		render_value(const char value[N]) 
			: render_value<Glib::ustring>(value) {}		
	};

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
		typedef boost::unordered_map<std::string, render_context_tree_element> children_t;
		mutable children_t children_;
		typedef std::list<render_context_tree_element> array_t;
		array_t array_;
		std::shared_ptr<render_value_base> value_;

	public:
		render_context_tree_element& find(const std::string& key) {
			if(key.empty()) 
				return *this;
			else {
				const auto position = key.find('.');
				if(position == std::string::npos)
					return children_[key.substr(0, position)];
				else
					return children_[key.substr(0, position)].find(key.substr(position+1));
			}
		}
	
		const render_value_base& value() const {
			return *value_.get();
		}

		array_t::const_iterator begin() const {
			return array_.begin();
		}
		
		array_t::const_iterator end() const {
			return array_.end();
		}

		template<typename T>
		void put_value(const T& v) {
			value_.reset(new render_value<T>(v));
		}

		template<typename T>
		void put_lambda(T&& v) {
			value_.reset(new render_function<T>(std::forward<T>(v)));
		}

		render_context_tree_element& add_to_array() {
			array_.emplace_back();
			return array_.back();
		}

		bool empty() const { 
			return !value_ && array_.empty();
		}

		bool has_value() {
			return !!value_;
		}
	};



	/// class which stores values for rendering fragment(s)
	class render_context {
		render_context_tree_element root_;
	public:
		/// store value (copied) under key
		template<typename T>
		render_context& val(const std::string& key, const T& value) {
			root_.find(key).put_value(value);
			return *this;
		}

		/// store reference under key, reference must be valid during rendering
		template<typename T>
		render_context& ref(const std::string& key, const T& value) {
			root_.find(key).put_value(value);
			return *this;
		}

		/// store lazy evaulated lambda under key, any lambda reference captures must be valid during rendering
		template<typename T>
		render_context& lambda(const std::string& key, T&& value) {
			root_.find(key).put_lambda(std::forward<T>(value));
			return *this;
		}

		/// copy subtree to new key
		void put(const std::string& key, const render_context_tree_element& subtree) {
			root_.find(key) = subtree;
		}
		
		/// add to array
		template<typename T>
		render_context& add_to_array(const std::string& key, const T& value) {
			root_.find(key).add_to_array().put_value(value);
			return *this;
		}

		/// find by name, descend from root_
		const render_context_tree_element& get(const std::string& key);
	};



	class context;

#define ctx_variable_check(tag, attribute, variablename, rndvalue) if(rndvalue.empty()) throw std::runtime_error((boost::format("webpp::xml::render_context(): variable '%s' required from <%s> at line %d, attribute %s, is missing") % variablename % tag->get_name() % tag->get_line() % attribute).str())

	/// \brief Piece of html5/xml, which was rendered from fragment. Can be modified and then converted to ustring.
	class fragment_output {
		std::unique_ptr<xmlpp::Document> output_; // mutable, because to_string() is obviously const, and libxml++ thins different.
	public:
		/// \brief Construct empty document
		fragment_output();
		fragment_output(fragment_output&&);

		/// \brief Find all nodes matching to XPath expression
		//xmlpp::NodeSet find_by_xpath(const std::string& query) const;

		Glib::ustring to_string() const;

		inline xmlpp::Document& document() { return *output_; } // FIXME!
	private:


		friend class fragment;
	};

	/// \brief Piece of html5/xml, which is stored and then rendered using render_context and its values
	class fragment : public boost::noncopyable {
		context& context_;
		xmlpp::DomParser reader_;
	public:
		/// \brief Load fragment from file 'filename'
		fragment(const std::string& filename, context& ctx);
		
		/// \brief Load fragment from string 'buffer'
		fragment(const std::string& name, const Glib::ustring& buffer, context& ctx);
		
		fragment(const fragment&) = delete;
		fragment& operator=(const fragment&) = delete;
				
		/// \brief render this fragment, return XML in string
		fragment_output render(render_context& rnd);
	private:
		// list of modifiers for single attribute
		typedef std::map<std::string, std::string> modifier_list_t;
		// list of attribute with modifiers
		typedef std::map<std::string, modifier_list_t> modifier_map_t;

		/// \brief Process node 'src' and its children, put generated output into 'dst'
		void process_node(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd);
		/// \brief Process subattributes in 'src' into destination node 'dst'
		void process_subattributes(modifier_map_t& src, xmlpp::Element* dst, render_context& rnd);
		/// \brief Process children of 'src' and put generated output as children of 'dst
		void process_children(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd); 
	};
	
	
	/*! \brief Handler for custom XML tags
	 * 	\example <f:input f:name="user.name" />
	 */
	class tag {
	public:
		/// \brief render as TEXT node to 'dst', using 'src' for attribute source and 'ctx' to value source		
		virtual void render(xmlpp::Element* dst, const xmlpp::Element* src, render_context& ctx) const = 0;
	};

	/*! \brief Handler for subattributes (see example)
	 *  \example <a href.render="user.nickname" href.format="/users/%s"> (render is subattribute, format is consumed by render)
	 */
	class subattribute {
	public:
		/*! \brief process subattribute, consume one (subattribute name) or more (optional) subattributes, return rendered value for subattributed attribute
		 * 	\param attributes subattribute name -> subattribute value map
		 * 	\param ctx rendering context
		 */
		virtual Glib::ustring render(std::map<std::string, std::string>& attributes, const render_context& ctx) const = 0;
	};

	/*! \class context
	 *  \brief Container for XML fragments and support XML tag and subattribute objects
	 */
	class context {
		const boost::filesystem::path library_directory_;
		boost::unordered_map<std::string, std::shared_ptr<fragment> > fragments_;
		/// <wpp:foobar ... />
		boost::unordered_map<std::pair<std::string,std::string>, std::unique_ptr<tag> > tags_;
		/// <a href.value="variable-name" href.format="%.3lf">
		boost::unordered_map<std::string, std::unique_ptr<subattribute>> subattributes_;
		
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
		void put(const std::string& name, const std::string& data);

		/// \brief Load tag library _Tgt
		template<typename _Tgt>
		void load_taglib() {
			_Tgt::process(tags_, subattributes_);
		}

		/// \brief Find or load fragment named 'name', throw exception if not found
		fragment&  get(const std::string& name);

		/*! \brief Find tag object named 'name' in 'ns' namespace, returns nullptr if not found
		 * 	\param ns namespace URI
		 * 	\param name tag name
		 */		
		const tag* find_tag(const std::string& ns, const std::string& name);
		
		/// \brief Find subattribute named 'name', returns nullptr if not found
		const subattribute* find_subattribute(const std::string& name);
	};
}}

#endif // WEBPP_XMLLIB
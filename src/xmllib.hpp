#ifndef WEBPP_XMLLIB
#define WEBPP_XMLLIB

#include <libxml++-2.6/libxml++/libxml++.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/unordered_map.hpp>
#include <boost/type_traits.hpp>
#include <boost/ptr_container/ptr_list.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/noncopyable.hpp>
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
	namespace render {
		/// abstract interface for values in render context
		/// supports output() - lexical cast to string
		/// and format(fmt), where fmt is argument for boost::format(fmt) % value
		class value_base : public boost::noncopyable {
		public:
			virtual Glib::ustring format(const Glib::ustring& fmt) const = 0;
			virtual Glib::ustring output() const = 0;
			virtual bool is_true() const = 0;
		};

		/// default implementations of render_value interface
		template<typename T>
		class value : public value_base {
			const T value_;
		public:
			value(const T& value)
				: value_(value) {}

			virtual Glib::ustring format(const Glib::ustring& fmt) const {
				return (boost::format(fmt) % value_).str();
			}

			virtual Glib::ustring output() const {
				return boost::lexical_cast<Glib::ustring>(value_);
			}

			virtual bool is_true() const {
				STACKED_EXCEPTIONS_ENTER();
				throw std::runtime_error("render::value<" + Glib::ustring(typeid(T).name()) + ">::is_true(): '" + output() + "' is not a boolean");
				STACKED_EXCEPTIONS_LEAVE("");
			}
		};
	
		// char literals are stored as Glib::ustring
		template< std::size_t N >
		class value<char[N]> : public value<Glib::ustring> {
		public:
			value(const char v[N])
				: value<Glib::ustring>(v) {}
		};

		//! \brief Lazy evaluated function/lambda/bind/any callable. Will execute once requested from renderer and then value will be cached.
		template <typename T>
		class function : public value_base {
			T lambda_;
			typedef decltype(lambda_()) return_type;
			mutable std::unique_ptr<value<return_type>> value_;

			value<return_type>& eval() const {
				if(!value_)
					value_.reset(new value<return_type>(lambda_()));
				return *value_;
			}

		public:
			function(T&& value)
				: lambda_(std::forward<T>(value)) {}

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
		bool value<bool>::is_true() const;

		class tree_element;

		//! \brief Array interface
		class array_base {
		public:
			virtual tree_element& next() = 0;
			virtual bool has_next() const = 0;
			virtual bool empty() const = 0;
			virtual void reset() = 0;
		};

		//! \brief Store zero or more sub storages (aka subtrees)
		class array : public array_base {
			typedef std::list<std::shared_ptr<tree_element>> elements_t;
			elements_t elements_;
			elements_t::iterator it_;
		public:
			array() : it_(elements_.end()) {}

			template<typename TreeElementT = tree_element, typename... TreeElementParamsT>
			TreeElementT& add(TreeElementParamsT&&... params) {
				elements_.emplace_back(new TreeElementT(std::forward<TreeElementParamsT>(params)...));
				return *dynamic_cast<TreeElementT*>(elements_.back().get());
			}

            virtual tree_element& next();
            virtual bool has_next() const;
            virtual bool empty() const;
            virtual void reset();
		};

		//! \brief Storage node for values used for rendering XML fragment(s)
		class tree_element : public std::enable_shared_from_this<tree_element>, boost::noncopyable {
			std::unique_ptr<value_base> value_;
			std::unique_ptr<array_base> array_;
			typedef boost::unordered_map<Glib::ustring, std::shared_ptr<tree_element>> children_t;
			children_t children_;
			std::shared_ptr<tree_element> link_;

			inline tree_element& self() { return !link_ ? *this : *link_; }
			inline const tree_element& self() const { return !link_ ? *this : *link_; }
		public:

			//! \brief Remove link from this node (used with imported and lazy tree nodes)
            void remove_link();

			//! \brief Create link from this node (used with imported and lazy tree nodes)
            void create_link(std::shared_ptr<tree_element> e);

			//! \brief Find tree element stored under key in this subtree. Every key exists in tree, but only some of them have associated variables or arrays
            virtual tree_element& find(const Glib::ustring& key);

			//! \brief Get value stored under this tree element. Throw exception if there is no value here.
            virtual const value_base& get_value() const;
			//! \brief Get array stored under this tree element. Throw exception if there is no array here.
            virtual array_base& get_array() const;

            inline bool is_value() const {
				return !!self().value_;
			}

            inline bool is_array() const {
				return !!self().array_;
			}

            inline bool empty() const {
				return !is_value() && !is_array();
			}

			//! \brief Put value of any type in this tree element. Also, reset previous value or array stored here.
			template<typename T, typename StorageT = T>
			void create_value(const T& v) {
				self().value_.reset(new value<StorageT>(v));
				self().array_.reset();
			}

			//! \brief Put lambda returing value in this tree element. Also, reset previous value or array stored here.
			template<typename F>
			void create_lambda(F&& f) {
				self().value_.reset(new function<F>(std::forward<F>(f)));
				self().array_.reset();
			}

			//! \brief Put array here. Also, reset previous value or array stored here. Returns array to fill contents.
			template<typename ArrayT = array, typename... ArrayParams>
			ArrayT& create_array(ArrayParams&&... ap) {
				self().value_.reset();
				self().array_.reset(new ArrayT(std::forward<ArrayParams>(ap)...));
				return *dynamic_cast<ArrayT*>(self().array_.get());
			}

            virtual void debug(int tab = 0) const;
        };


		//! \brief Frontend for storage tree
		class context {
			mutable std::shared_ptr<tree_element> root_; // mutable, because 'read only' operations also create paths
		public:
			context() : root_(std::make_shared<tree_element>()) {}
			//! \brief Get mutable tree element found under key
            inline tree_element& get(const Glib::ustring &name) {
				return root_->find(name);
			}

			//! \brief Get const tree element found under key
            inline const tree_element& get(const Glib::ustring &name) const {
				return root_->find(name);
			}

			//! \brief Store value (copied) under key
			template<typename T>
			void create_value(const Glib::ustring& key, const T& value) {
				get(key).create_value(value);
			}

			//! \brief Store reference under key. Referenced variable must be valid during rendering.
			template<typename T>
			void create_reference(const Glib::ustring& key, const T& value) {
				get(key).create_value<T,T&>(value);
			}

			//! \brief Store lazy evaluated value from lambda under key.
			template<typename F>
			void create_lambda(const Glib::ustring& key, F&& function) {
				get(key).create_lambda(std::forward<F>(function));
			}

			template<typename ArrayT = array, typename... ArgsT>
			auto create_array(const Glib::ustring& key, ArgsT&&... args) -> decltype(get(key).create_array<ArrayT, ArgsT...>(std::forward<ArgsT>(args)...)) {
				return get(key).create_array<ArrayT, ArgsT...>(std::forward<ArgsT>(args)...);
			}

			//! \brief Import subtree to key. Subtree ownership remains as before this call.
            void import_subtree(const Glib::ustring& key, tree_element& orig);

			//! \brief Link newly allocated dynamic subtree to key
			template<typename T, typename... Args>
			void link_dynamic_subtree(const Glib::ustring& key, Args&&... args) {
				root_->find(key).remove_link();
				root_->find(key).create_link(std::make_shared<T>(std::forward<Args>(args)...));
			}
		};
	}


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

	/// \brief Piece of html5/xml, which is stored and then rendered using render::context and its values
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
		fragment_output render(render::context& rnd);
	private:
		// list of modifiers for single attribute
		typedef std::map<Glib::ustring, Glib::ustring> modifier_list_t;
		// list of attribute with modifiers
		typedef std::map<Glib::ustring, modifier_list_t> modifier_map_t;

		/// \brief Process node 'src' and its children, put generated output into 'dst'
		void process_node(const xmlpp::Element* src, xmlpp::Element* dst, render::context& rnd, bool already_processing_outer_repeat = false);
		/// \brief Process children of 'src' and put generated output as children of 'dst
		void process_children(const xmlpp::Element* src, xmlpp::Element* dst, render::context& rnd);
	};
	
	
	/*! \brief Handler for custom XML tags
	 * 	\example <f:input name="user.name" />
	 */
	class tag {
	public:
		/// \brief render as TEXT node to 'dst', using 'src' for attribute source and 'ctx' to value source		
		virtual void render(xmlpp::Element* dst, const xmlpp::Element* src, render::context& ctx) const = 0;
	};

	/*! \brief Handle all attributes and tags in namespace
	 *  \example <a f:href="/users/#{user.name}" f:title="user #[user.name} - abuse level #{user.abuse|%.2f]">
	 */
	class xmlns {
	public:
		/// Process tag 'src' and place result as child element in 'dst'
		virtual void tag(xmlpp::Element* dst, const xmlpp::Element* src, render::context& ctx) const = 0;
		/// Process attribute 'src' and place results (attributes) inside element 'dst'
		virtual void attribute(xmlpp::Element* dst, const xmlpp::Attribute* src, render::context& ctx) const = 0;
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

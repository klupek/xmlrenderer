#ifndef WEBPP_XMLRENDERER_XMLLIB_HPP
#define WEBPP_XMLRENDERER_XMLLIB_HPP

#include <libxml++-2.6/libxml++/libxml++.h>

extern "C" {
	struct _xsltStylesheet;
	typedef struct _xsltStylesheet xsltStylesheet;
}

#include <boost/format.hpp>
#include <boost/container/flat_map.hpp>
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

#include <webpp-common/stacked_exception.hpp>
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
            virtual ~value_base() {}
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

		template<>
		inline Glib::ustring value<Glib::ustring>::output() const {
			return value_;
		}
	
		// char literals are stored as Glib::ustring
		template< std::size_t N >
		class value<char[N]> : public value<Glib::ustring> {
		public:
			value(const char v[N])
				: value<Glib::ustring>(v) {}
		};

		template<>
		class value<std::string> : public value<Glib::ustring> {
		public:
			inline value(const std::string& v)
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
			virtual size_t size() const = 0;
            virtual ~array_base() {}
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
			virtual size_t size() const;
		};

		//! \brief Storage node for values used for rendering XML fragment(s)
		class tree_element : public std::enable_shared_from_this<tree_element>, boost::noncopyable {
			std::unique_ptr<value_base> value_;
			std::unique_ptr<array_base> array_;
			typedef boost::unordered_map<Glib::ustring, std::shared_ptr<tree_element>> children_t;
			children_t children_;
            std::weak_ptr<tree_element> link_;
            std::shared_ptr<tree_element> permalink_;

            inline std::shared_ptr<tree_element> self() { return link_.expired() ? shared_from_this() : link_.lock(); }
            inline std::shared_ptr<const tree_element> self() const { return link_.expired() ? shared_from_this() : link_.lock(); }
		public:

			//! \brief Remove link from this node (used with imported and lazy tree nodes)
            void remove_link();

            //! \brief Create link from this node (used with imported tree nodes)
            void create_link(std::shared_ptr<tree_element> e);

            //! \brief Create permanent link (used with lazy tree nodes)
            void create_permanent_link(std::shared_ptr<tree_element> e);

			//! \brief Find tree element stored under key in this subtree. Every key exists in tree, but only some of them have associated variables or arrays
            virtual tree_element& find(const Glib::ustring& key);

			//! \brief Get value stored under this tree element. Throw exception if there is no value here.
            virtual const value_base& get_value() const;
			//! \brief Get array stored under this tree element. Throw exception if there is no array here.
            virtual array_base& get_array() const;

            inline bool is_value() const {
                return !!self()->value_;
			}

            inline bool is_array() const {
                return !!self()->array_;
			}

            inline bool empty() const {
				return !is_value() && !is_array();
			}

			//! \brief Put value of any type in this tree element. Also, reset previous value or array stored here.
			template<typename T, typename StorageT = T>
			void create_value(const T& v) {
                self()->value_.reset(new value<StorageT>(v));
                self()->array_.reset();
			}

			//! \brief Put lambda returing value in this tree element. Also, reset previous value or array stored here.
			template<typename F>
			void create_lambda(F&& f) {
                self()->value_.reset(new function<F>(std::forward<F>(f)));
                self()->array_.reset();
			}

			//! \brief Put array here. Also, reset previous value or array stored here. Returns array to fill contents.
			template<typename ArrayT = array, typename... ArrayParams>
			ArrayT& create_array(ArrayParams&&... ap) {
                self()->value_.reset();
                self()->array_.reset(new ArrayT(std::forward<ArrayParams>(ap)...));
                return *dynamic_cast<ArrayT*>(self()->array_.get());
			}

			virtual void debug(const std::string& prefix = "/", int tab = 0) const;
        };


		//! \brief Frontend for storage tree
		class context {
			mutable std::shared_ptr<tree_element> root_; // mutable, because 'read only' operations also create paths
            std::deque<Glib::ustring> prefixes_;
            Glib::ustring current_prefix_;
		public:
			context() : root_(std::make_shared<tree_element>()) {}
			//! \brief Get mutable tree element found under key
            inline tree_element& get(const Glib::ustring &name) {
                return root_->find(current_prefix_ + name);
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

            //! \brief All searches after this call will add this (and previous) prefixes joined by "."
            inline void push_prefix(const Glib::ustring& prefix) {
                prefixes_.push_back(prefix);
                if(!prefix.empty()) {
                    current_prefix_ += prefix + ".";
                }
            }

            //! \brief Pop last added prefix
            inline void pop_prefix() {
                prefixes_.pop_back();
                current_prefix_ = "";
                for(const auto &i : prefixes_)
                    if(!i.empty())
                        current_prefix_ += i + ".";

            }
		};
	}

	class node_iterator {
		xmlpp::Node* node_;
	public:
		node_iterator(xmlpp::Node* node);
		xmlpp::Node& operator*();
		xmlpp::Node* operator->();
		node_iterator& operator++();
		node_iterator operator++(int);
		inline bool operator==(const node_iterator& rhs) { return node_ == rhs.node_; }
		inline bool operator!=(const node_iterator& rhs) { return node_ != rhs.node_; }
		inline xmlpp::Element* element() { return dynamic_cast<xmlpp::Element*>(node_); }

	private:
		void increment();
	};

	class context;
// short macro for checking existance of variable in rendering context
#define ctx_variable_check(tag, attribute, variablename, rndvalue) if(rndvalue.empty()) throw std::runtime_error((boost::format("variable '%s' required from <%s> at line %d, attribute %s, is missing") % variablename % tag->get_name() % tag->get_line() % attribute).str())

	/// \brief Piece of html5/xml, which was rendered from fragment. Can be modified and then converted to ustring.
	class fragment_output {
		const Glib::ustring name_; // for exception decorating
		std::unique_ptr<xmlpp::Document> output_; // mutable, because to_string() is obviously const, and libxml++ thinks different.
        bool remove_xml_declaration_; // usefull for broken browsers		
	public:
		/// \brief Construct empty document
		fragment_output(const Glib::ustring& name);
		fragment_output(fragment_output&&);

		/// \brief Find all nodes matching to XPath expression
		//xmlpp::NodeSet find_by_xpath(const Glib::ustring& query) const;

        //! \brief Convert output to valid XML (currently: nothing to do)
        fragment_output& xml();

        enum xhtml5_encoding {
            DOCTYPE = 1, // add xhtml5 doctype
            REMOVE_XML_DECLARATION = 2, // remove <?xml ...
            REMOVE_COMMENTS = 4 // remove all comments
        };

        //! \brief Convert XML tree to valid HTML5 and add fixes (conditional <html> etc.)
        fragment_output& xhtml5(int html5_encoding);
        inline xmlpp::Document& document() { return *output_; }
        Glib::ustring to_string() const;

		inline node_iterator begin() { return node_iterator(output_.get()->get_root_node()); }
		inline node_iterator end() { return node_iterator(nullptr); }
	private:
        void remove_comments(xmlpp::Element*);
    };

	/// \brief Piece of html5/xml, which is stored and then rendered using render::context and its values
	class fragment : public boost::noncopyable {
		const Glib::ustring name_;
		context& context_;
		xmlpp::DomParser reader_;
		std::unique_ptr<xmlpp::Document> processed_document_;
	public:
		/// \brief Load fragment from file 'filename'
		fragment(const Glib::ustring& filename, context& ctx);
		
		/// \brief Load fragment from string 'buffer'
		fragment(const Glib::ustring& name, const Glib::ustring& buffer, context& ctx);						

        inline const Glib::ustring& name() const { return name_; }
		inline xmlpp::Document& get_document() { return processed_document_ ? *processed_document_ : *reader_.get_document(); }
		inline const xmlpp::Document& get_document() const { return processed_document_ ? *processed_document_ : *reader_.get_document(); }
	private:
		void apply_stylesheets();
    };

    /// \brief Prepared fragment
    class prepared_fragment {
        const fragment& fragment_;
        context& context_;
        struct view_insertion {
            Glib::ustring view_name, value_prefix;
        };

        typedef boost::container::flat_map<Glib::ustring, view_insertion> view_insertions_t;
        view_insertions_t view_insertions_;

    public:
        prepared_fragment(const fragment& fragment, context& ctx) : fragment_(fragment), context_(ctx) {}

        /// \brief render this fragment, return XML in string
        fragment_output render(render::context& rnd);

        /// \brief Add view 'view_name' to node with id='id'
        inline prepared_fragment& insert(const Glib::ustring& id, const Glib::ustring& view_name, const Glib::ustring& value_prefix) {
            view_insertions_[id] = view_insertion { view_name, value_prefix };
            return *this;
        }

        inline const fragment& get_fragment() const { return fragment_; }

    private:
        /// \brief Process node 'src' and its children, put generated output into 'dst'
        void process_node(const xmlpp::Element* src, xmlpp::Document& output, xmlpp::Element* dst, render::context& rnd, bool already_processing_outer_repeat = false);
        /// \brief Process children of 'src' and put generated output as children of 'dst
        void process_children(const xmlpp::Element* src, xmlpp::Document& output, xmlpp::Element* dst, render::context& rnd);
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
        /// Process tag 'src' and place result as element 'dst'
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
		typedef std::list<std::shared_ptr<xsltStylesheet>> stylesheets_t;
		stylesheets_t stylesheets_;
	public:		
		/*! \brief Construct context
		 * 	\param library_directory directory with fragment files
		 */
		context(const std::string& library_directory);

		/*! \brief Attach XSLT stylesheet for newly loaded documents. Only future loaded fragments will be affected.
		 *  \param name XSLT stylesheet path in library
		 */
		void attach_xslt(const std::string& name);

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
        prepared_fragment  get(const Glib::ustring& name);

		/*! \brief Find tag handler named 'name' in 'ns' namespace, returns nullptr if not found
		 * 	\param ns namespace URI
		 * 	\param name tag name
		 */		
		const tag* find_tag(const Glib::ustring& ns, const Glib::ustring& name);
		
		/// \brief Find xmlns handler for uri 'ns', returns nullptr if not found
		const xmlns* find_xmlns(const Glib::ustring& ns);

		inline const stylesheets_t& get_stylesheets() { return stylesheets_; }
	};
}}

#endif // WEBPP_XMLRENDERER_XMLLIB_HPP

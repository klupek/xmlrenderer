// xmllib.cpp : Defines the entry point for the console application.
//

#include "xmllib.hpp"

#include <iostream>
#include <libxml/xpath.h>

namespace webpp { namespace xml { 
	fragment_output::fragment_output(const Glib::ustring& name)
        : name_(name), output_(new xmlpp::Document), remove_xml_declaration_(false) {

	}

    fragment_output::fragment_output(fragment_output&& orig) : name_(orig.name_), remove_xml_declaration_(orig.remove_xml_declaration_) {
		std::swap(output_, orig.output_);
	}

    Glib::ustring fragment_output::to_string() const {
		STACKED_EXCEPTIONS_ENTER();
        const Glib::ustring xml_declaration("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        Glib::ustring result =  output_->write_to_string();
        if(remove_xml_declaration_)
            return result.substr(xml_declaration.length()); // or should it use .replace (slower) or other magic?
        else
            return result;
		STACKED_EXCEPTIONS_LEAVE("");
	}

    fragment_output& fragment_output::xml() {
        return *this;
    }

    fragment_output& fragment_output::xhtml5(int xhtml5_encoding) {
        if(xhtml5_encoding & DOCTYPE) {
            xmlpp::Document *d = output_.get();
            d->set_internal_subset("html",Glib::ustring(), Glib::ustring());
        }

        if(xhtml5_encoding & REMOVE_XML_DECLARATION) {
            remove_xml_declaration_ = true;
        }

        if(xhtml5_encoding & REMOVE_COMMENTS) {
            // first process (pre|post)-root comments
            for(xmlNode* i = output_->cobj()->children; i != nullptr;) {
                if(i->type == XML_COMMENT_NODE) {
                    xmlNode* tmp = i;
                    i = i->next;
                    // libxml++ private empty object
                    xmlpp::CommentNode* tmp2 = static_cast<xmlpp::CommentNode*>(tmp->_private);
                    delete tmp2;
                    xmlUnlinkNode(tmp);
                    xmlFreeNode(tmp);

                } else
                    i = i->next;
            }
            remove_comments(output_->get_root_node());
        }
        return *this;
    }

    void fragment_output::remove_comments(xmlpp::Element* element) {
        for(xmlpp::Node* i : element->get_children()) {
            xmlpp::CommentNode* cn = dynamic_cast<xmlpp::CommentNode*>(i);
            xmlpp::Element* e = dynamic_cast<xmlpp::Element*>(i);
            if(cn != nullptr) {
                element->remove_child(cn);
            } else if(e != nullptr) {
                remove_comments(e);
            }
        }
    }


	/// Load fragment from file 'filename', fragment name is filename
	fragment::fragment(const Glib::ustring& filename, context& ctx)
		: name_(filename), context_(ctx) {
		STACKED_EXCEPTIONS_ENTER();
		reader_.set_substitute_entities(false);
		reader_.set_validate(false);
		reader_.parse_file(filename);
		STACKED_EXCEPTIONS_LEAVE("parsing file '" + filename + "'");
	}

	/// Load fragment name 'name' from in-memory 'buffer'
	fragment::fragment(const Glib::ustring& name, const Glib::ustring& buffer, context& ctx)
		: name_(name), context_(ctx) {
		STACKED_EXCEPTIONS_ENTER();
		reader_.set_substitute_entities(false);
		reader_.set_validate(false);
		reader_.parse_memory(buffer);
		STACKED_EXCEPTIONS_LEAVE("parsing memory buffer named '" + name + "':<<XML\n" + buffer + "\nXML\n");
	}

	/// Return all nodes in fragment, matching given XPath expression
/*	xmlpp::NodeSet fragment::find_by_xpath(const Glib::ustring& query) {
		return reader_.get_document()->get_root_node()->find(query);
	}
*/

    fragment_output prepared_fragment::render(render::context& rnd) {
		STACKED_EXCEPTIONS_ENTER();
        fragment_output result(fragment_.name());
		xmlpp::Document& output = result.document();
        xmlpp::Element* src = fragment_.reader().get_document()->get_root_node();

        // copy children prev and next to root element, without processing (comments...)
        for(xmlNode* i = fragment_.reader().get_document()->cobj()->children; i != src->cobj() && i != nullptr; i = i->next) {
            if(i->type == XML_COMMENT_NODE) {
                xmlChar* comment = xmlNodeGetContent(i);
                output.add_comment(Glib::ustring(reinterpret_cast<const char*>(comment)));
                xmlFree(comment);
            }
        }

        output.create_root_node(src->get_name());
        xmlpp::Element* dst = output.get_root_node();

        for(xmlNode* i = src->cobj(); i != nullptr; i = i->next) {
            if(i->type == XML_COMMENT_NODE) {
                xmlChar* comment = xmlNodeGetContent(i);
                output.add_comment(Glib::ustring(reinterpret_cast<const char*>(comment)));
                xmlFree(comment);
            }
        }

        process_node(src, output, dst, rnd);
		return result;
        STACKED_EXCEPTIONS_LEAVE("fragment '" + fragment_.name() + "'");
	}		

	/// Construct context; library_directory is directory root for fragment XML files
	context::context(const std::string& library_directory)
		: library_directory_(library_directory) {
	}

	/// Load fragment 'name' from file in library
	void context::load(const std::string& name) {
		STACKED_EXCEPTIONS_ENTER();
        fragments_.emplace(name, std::make_shared<fragment>( (library_directory_ / name).string() + ".xml", *this));
		STACKED_EXCEPTIONS_LEAVE("loading file " + name);
	}

	/// Load fragment 'name' from in-memory buffer 'data'
	void context::put(const Glib::ustring& name, const Glib::ustring& data) {
		STACKED_EXCEPTIONS_ENTER();
		fragments_[name] = std::make_shared<fragment>( name, data, *this);
		STACKED_EXCEPTIONS_LEAVE("loading memory buffer " + name);
	}

	/// find fragment by 'name', load it from library if not loaded yet.
    prepared_fragment context::get(const Glib::ustring& name) {
		STACKED_EXCEPTIONS_ENTER();
		auto i = fragments_.find(name);
		if(i == fragments_.end()) {
			load(name);
			i = fragments_.find(name);
		}

		if(i == fragments_.end())
			throw std::runtime_error("webpp::xml::context::get(): required fragment '" + name + "' not found");
		else
            return prepared_fragment(*i->second, *this);
		STACKED_EXCEPTIONS_LEAVE("fragment name " + name);
	}

	const tag* context::find_tag(const Glib::ustring& ns, const Glib::ustring& name) {
		auto i = tags_.find(std::make_pair(ns, name));
		if(i == tags_.end())
			return nullptr;
		else
			return i->second.get();
	}

	const xmlns* context::find_xmlns(const Glib::ustring& ns) {
		auto i = xmlnses_.find(ns);
		if(i == xmlnses_.end())
			return nullptr;
		else
			return i->second.get();
	}

	template<>
	bool render::value<bool>::is_true() const {
		return value_;
	}


    void prepared_fragment::process_node(const xmlpp::Element* src, xmlpp::Document& output, xmlpp::Element* dst, render::context& rnd, bool already_processing_outer_repeat) {
		STACKED_EXCEPTIONS_ENTER();

		Glib::ustring repeat_variable, repeat_array;
		enum { inner, outer,none } repeat_type = none;
		bool visible = true;
        bool nochildren = false;

		// process control attributes first
		for(auto attribute : src->get_attributes()) {
			const auto ns = attribute->get_namespace_uri();
			const auto name = attribute->get_name();
			const auto value = attribute->get_value();

            if(ns == "webpp://control") {
				// control statements, loops and conditions
				// foreach loops, outer repeats whole tag and children, inner repeats children only
				if(name == "repeat") {
					if(value == "inner")
						repeat_type = inner;
					else if(value == "outer")
						repeat_type = outer;
					else
						throw std::runtime_error(
							(boost::format("repeat must be one of (inner,outer), not '%s' in line '%s', tag '%s'")
								% value % src->get_line() % src->get_name()).str());
				} else if(name == "repeat-array") {
					repeat_array = value;
				} else if(name == "repeat-variable") {
					repeat_variable = value;
				// element visibility
				} else if(name == "if-exists") {
					const auto& val = rnd.get(value);
					visible &= (!val.empty());
				} else if(name == "if-not-exists") {
					const auto& val = rnd.get(value);
					visible &= val.empty();
				} else if(name == "if-true") {
					const auto& val = rnd.get(value);
					ctx_variable_check(src, name, value, val);
					visible &= val.get_value().is_true();
				} else if(name == "if-not-true") {
					const auto& val = rnd.get(value);
					ctx_variable_check(src, name, value, val);
					visible &= !val.get_value().is_true();
				} else
					throw std::runtime_error("webpp://control atribute " + name + " is not implemented");
			}
		} // foreach attribute

		if(already_processing_outer_repeat && repeat_type == outer)
			repeat_type = none;

		if(!visible) {
			auto parent = dst->get_parent();
			if(parent == nullptr)
				throw std::runtime_error("response resulted in empty document");
			else
				parent->remove_child(dst);
		} else if(repeat_type != outer) {
			// element is visible AND it is not outer repeat
            xmlpp::Attribute *id_attribute = src->get_attribute("id");
            view_insertions_t::const_iterator view_insertion_iterator = view_insertions_.end();
            if(id_attribute != nullptr)
                view_insertion_iterator = view_insertions_.find(id_attribute->get_value());

            if(view_insertion_iterator == view_insertions_.end() &&
                    (src->get_namespace_uri() == "webpp://html5" || src->get_namespace_uri() == "webpp://xml" || src->get_namespace_uri().find("webpp://") == Glib::ustring::npos) ) {
                if(src->get_namespace_uri() == "webpp://html5")
                    output.get_root_node()->set_namespace_declaration("http://www.w3.org/1999/xhtml");
                else if(src->get_namespace_uri() != "webpp://xml") {
                    output.get_root_node()->set_namespace_declaration(src->get_namespace_uri(), src->get_namespace_prefix());
                    dst->set_namespace(src->get_namespace_prefix());
                }
				// normal tag, process attributes                
				// and every other attribute then
				for(auto attribute : src->get_attributes()) {
					const auto ns = attribute->get_namespace_uri();
					const auto name = attribute->get_name();
					const auto value = attribute->get_value();

					if(ns == "") /* default namespace, attributes in XML do NOT HAVE default namespace set via xmlns= in root node */ {
						// normal attribute
						dst->set_attribute(name, value);
					} else if(ns != "webpp://control"){ // if ns == webpp://control
						const xmlns* nshandler = context_.find_xmlns(ns);
						if(nshandler == nullptr)
							throw std::runtime_error("unknown attribute namespace  " + ns);
						nshandler->attribute(dst, attribute, rnd);
					}
				}
			} else {
				nochildren = true; // custom tags handle their children				
                if(src->get_namespace_uri() == "webpp://control" || view_insertion_iterator != view_insertions_.end()) {
                    // handle all c: internally
                    if(src->get_name() == "insert") {
                        if(src->get_attribute("name") == nullptr)
                            throw std::runtime_error("webpp://control:insert requires attribute name (inserted view name)");
                        if(src->get_attribute("value-prefix") == nullptr)
                            throw std::runtime_error("webpp://control:insert requires attribute value-prefix (prefix for render context variables)");
                        rnd.push_prefix(src->get_attribute("value-prefix")->get_value());
                        auto subdoc = context_.get(src->get_attribute("name")->get_value());
                        subdoc.process_node(subdoc.get_fragment().reader().get_document()->get_root_node(), output, dst, rnd);
                        rnd.pop_prefix();
                    } else if(view_insertion_iterator != view_insertions_.end()) {
                        rnd.push_prefix(view_insertion_iterator->second.value_prefix);
                        auto subdoc = context_.get(view_insertion_iterator->second.view_name);
                        subdoc.process_node(subdoc.get_fragment().reader().get_document()->get_root_node(), output, dst, rnd);
                        rnd.pop_prefix();
                    } else {
                        throw std::runtime_error("unknown webpp://control tag: " + src->get_name());
                    }
                } else {
                    // look for pair(tagns,tagname) handler
                    auto tag = context_.find_tag(src->get_namespace_uri(), src->get_name());
                    if(tag == nullptr) {
                        const xmlns* nshandler = context_.find_xmlns(src->get_namespace_uri());
                        if(!nshandler)
                            throw std::runtime_error( (boost::format("required custom tag %s in ns %s (or namespace handler) not found") % src->get_name() % src->get_namespace_uri()).str());

                        nshandler->tag(dst, src, rnd);
                    } else
                        tag->render(dst, src, rnd);
                }
			}

			if(repeat_type == none) {
				if(!nochildren)
                    process_children(src, output, dst, rnd);
			} else /* if(repeat_type == inner) */ {
				// repeat_variable, repeat_array;
				if(repeat_variable.empty() || repeat_array.empty())
					throw std::runtime_error("repeat attribute set, but repeat_variable or repeat_array is not set");

				auto& array = rnd.get(repeat_array).get_array();
				array.reset();
				while(array.has_next()) {
					rnd.import_subtree(repeat_variable, array.next());
                    process_children(src, output, dst, rnd);
				}
			}
		} else { // repeat_type == outer
			if(src->get_parent() == nullptr)
				throw std::runtime_error("outer repeat on root element is not possible");
			// repeat_variable, repeat_array;
			if(repeat_variable.empty() || repeat_array.empty())
				throw std::runtime_error("repeat attribute set, but repeat_variable or repeat_array is not set");
			// we need to repeat whole xml element
			auto& array = rnd.get(repeat_array).get_array();
			if(array.empty())
				dst->get_parent()->remove_child(dst);
			else {
				xmlpp::Element* currentdst = dst;
				array.reset();
				while(array.has_next()) {
					// first setup context variable
					rnd.import_subtree(repeat_variable, array.next());
                    process_node(src, output, currentdst, rnd, true);
					// move to next source array element, if it is not end, then add next sibling
					if(array.has_next())
						currentdst = currentdst->get_parent()->add_child(src->get_name());
				}
			}
        } // if repeat_type
        STACKED_EXCEPTIONS_LEAVE("node " + src->get_namespace_uri() + ":" + src->get_name() + " at line " + boost::lexical_cast<std::string>(src->get_line()));
	}

    void prepared_fragment::process_children(const xmlpp::Element* src, xmlpp::Document& output, xmlpp::Element* dst, render::context& rnd) {
		STACKED_EXCEPTIONS_ENTER();
		for(auto child : src->get_children()) {
			const xmlpp::Element* childelement = dynamic_cast<xmlpp::Element*>(child);
			if(childelement != nullptr) {
				xmlpp::Element* e = dst->add_child(child->get_name());				
                process_node(childelement, output, e, rnd);
			} else {
				dst->import_node(child);
			} // if childelement != nullptr
		}
		STACKED_EXCEPTIONS_LEAVE("");
	}


    render::tree_element& render::array::next() {
        return **it_++;
    }

    bool render::array::has_next() const {
        return it_ != elements_.end();
    }

    bool render::array::empty() const {
        return elements_.empty();
    }

    void render::array::reset() {
        it_ = elements_.begin();
    }

    //! \brief Remove link from this node (used with imported and lazy tree nodes)
    void render::tree_element::remove_link() {
        link_.reset();
    }

    //! \brief Create link from this node (used with imported and lazy tree nodes)
    void render::tree_element::create_link(std::shared_ptr<tree_element> e) {
        link_ = e;
    }

    void render::tree_element::create_permanent_link(std::shared_ptr<tree_element> e) {
        std::swap(permalink_,e);
        link_ = e;
    }


    //! \brief Find tree element stored under key in this subtree. Every key exists in tree, but only some of them have associated variables or arrays
    render::tree_element& render::tree_element::find(const Glib::ustring& key) {
        if(key.empty())
            return *this;
        else {
            const auto position = key.find('.');

            if(position == Glib::ustring::npos) {
                auto& result = self()->children_[key.substr(0, position)];
                if(!result)
                    result.reset(new tree_element);
                return *result;

            } else {
                auto& result = self()->children_[key.substr(0, position)];
                if(!result)
                    result.reset(new tree_element);
                return result->find(key.substr(position+1));
            }
        }
    }


    const render::value_base& render::tree_element::get_value() const {
        if(!self()->value_)
            throw std::runtime_error("no value in this node");
        return *self()->value_;
    }

    render::array_base& render::tree_element::get_array() const {
        if(!self()->array_)
            throw std::runtime_error("no array in this node");
        return *self()->array_;
    }

    void render::tree_element::debug(int tab) const {
        if(is_value())
            std::cout << std::setw(tab*5) << "" << "value: " << value_->output() << std::endl;
        if(is_array()) {
            std::cout << std::setw(tab*5) << "" << "array elements:\n";
            auto& array = *self()->array_;
            array.reset();
            while(array.has_next())
                array.next().debug(tab+1);
        }
        if(!self()->children_.empty()) {
            for(auto& child : self()->children_) {
                std::cout << std::setw((tab+1)*5) << "" << "child " << child.first << std::endl;
                child.second->debug(tab+2);
            }
        }
    }

    void render::context::import_subtree(const Glib::ustring& key, tree_element& orig) {
        root_->find(key).remove_link();
        root_->find(key).create_link(orig.shared_from_this());
    }

}}



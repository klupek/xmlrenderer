// xmllib.cpp : Defines the entry point for the console application.
//

#include "xmllib.hpp"

#include <iostream>
#include <libxml/xpath.h>
namespace webpp { namespace xml { 
	fragment_output::fragment_output()
		: output_(new xmlpp::Document) {

	}

	fragment_output::fragment_output(fragment_output&& orig) {
		std::swap(output_, orig.output_);
	}

	Glib::ustring fragment_output::to_string() const {
		return output_->write_to_string();
	}


	/// Load fragment from file 'filename', fragment name is filename
	fragment::fragment(const Glib::ustring& filename, context& ctx)
		: context_(ctx) {
		reader_.set_substitute_entities(false);
		reader_.set_validate(false);
		reader_.parse_file(filename);
	}

	/// Load fragment name 'name' from in-memory 'buffer'
	fragment::fragment(const Glib::ustring& name, const Glib::ustring& buffer, context& ctx)
		: context_(ctx) {
		reader_.set_substitute_entities(false);
		reader_.set_validate(false);
		reader_.parse_memory(buffer);
	}

	/// Return all nodes in fragment, matching given XPath expression
/*	xmlpp::NodeSet fragment::find_by_xpath(const Glib::ustring& query) {
		return reader_.get_document()->get_root_node()->find(query);
	}
*/

	fragment_output fragment::render(render_context& rnd) {
		fragment_output result;
		xmlpp::Document& output = result.document();
		xmlpp::Element* src = reader_.get_document()->get_root_node();
		output.create_root_node(src->get_name());
		xmlpp::Element* dst = output.get_root_node();
		process_node(src, dst, rnd);
		return result;
	}		

	/// Construct context; library_directory is directory root for fragment XML files
	context::context(const std::string& library_directory)
		: library_directory_(library_directory) {
	}

	/// Load fragment 'name' from file in library
	void context::load(const std::string& name) {
		fragments_.emplace(name, std::make_shared<fragment>( (library_directory_ / name).string(), *this));
	}

	/// Load fragment 'name' from in-memory buffer 'data'
	void context::put(const Glib::ustring& name, const Glib::ustring& data) {
		fragments_[name] = std::make_shared<fragment>( name, data, *this);
	}

	/// find fragment by 'name', load it from library if not loaded yet.
	fragment& context::get(const Glib::ustring& name) {
		auto i = fragments_.find(name);
		if(i == fragments_.end()) {
			load(name);
			i = fragments_.find(name);
		}

		if(i == fragments_.end())
			throw std::runtime_error("webpp::xml::context::get(): required fragment '" + name + "' not found");
		else
			return *i->second;
	}

	/// get render context variable
	const render_context_tree_element& render_context::get(const Glib::ustring& key) {
		return root_.find(key);
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
	bool render_value<bool>::is_true() const {
		return value_;
	}


	void fragment::process_node(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd) {
		// first, check tag type(namespace)
		if(src->get_namespace_uri() == "webpp://html5" || src->get_namespace_uri() == "webpp://xml") {
			// normal tag, process attributes
			Glib::ustring repeat_variable, repeat_array;
			enum { inner, outer,none } repeat_type = none;
			bool visible = true;

			for(auto attribute : src->get_attributes()) {
				const auto ns = attribute->get_namespace_uri();
				const auto name = attribute->get_name();
				const auto value = attribute->get_value();
				
				if(ns == "") /* default namespace, attributes in XML do NOT HAVE default namespace set via xmlns= in root node */ {
					// normal attribute
					dst->set_attribute(name, value);
				} else if(ns == "webpp://control") {
					// control statements, loops and conditions
					// foreach loops, outer repeats whole tag and children, inner repeats children only
					if(name == "repeat") {
						if(value == "inner") 
							repeat_type = inner;
						else if(value == "outer")
							repeat_type = outer;
						else
							throw std::runtime_error(
								(boost::format("webpp::xml::fragment::process_node(): repeat must be one of (inner,outer), not '%s' in line '%s', tag '%s'") 
									% value % src->get_line() % src->get_name()).str());
					} else if(name == "repeat-array") {
						repeat_array = value;
					} else if(name == "repeat-variable") {
						repeat_variable = value;
					// element visibility
					} else if(name == "if-exists") {
						const auto val = rnd.get(value);
						visible &= (!val.empty());
					} else if(name == "if-not-exists") {
						const auto val = rnd.get(value);
						visible &= val.empty();
					} else if(name == "if-true") {
						const auto val = rnd.get(value);
						ctx_variable_check(src, name, value, val);
						visible &= val.value().is_true();
					} else if(name == "if-not-true") {
						const auto val = rnd.get(value);
						ctx_variable_check(src, name, value, val);
						visible &= !val.value().is_true();
					} else
						throw std::runtime_error("webpp::xml::fragment::process_node(): webpp://control atribute " + name + " is not implemented");
				} else { // if ns == webpp://control
					const xmlns* nshandler = context_.find_xmlns(ns);
					if(nshandler == nullptr)
						throw std::runtime_error("webpp::xml::fragment::process_node(): unknown attribute namespace  " + ns);

					nshandler->attribute(dst, attribute, rnd);
				}
			} // foreach attribute

			// check for visibility, if not visible, no further actions about this src tag should be taken
			if(!visible) {
				auto parent = dst->get_parent();
				if(parent == nullptr)
					throw std::runtime_error("webpp::xml::fragment::process_node(): response resulted in empty document");
				else
					parent->remove_child(dst);
			} else { // this is visible, lets process children
				if(repeat_type == none) {
					process_children(src, dst, rnd);
				} else if(repeat_type == inner) {
					// repeat_variable, repeat_array;						
					if(repeat_variable.empty() || repeat_array.empty())
						throw std::runtime_error("repeat attribute set, but repeat_variable or repeat_array is not set");

					const auto array = rnd.get(repeat_array);
					for(auto i : array) {
						rnd.put(repeat_variable, i);
						process_children(src, dst, rnd);
					}
				} else { // repeat_type == outer 
					// repeat_variable, repeat_array;						
					if(repeat_variable.empty() || repeat_array.empty())
						throw std::runtime_error("repeat attribute set, but repeat_variable or repeat_array is not set");
					// we need to repeat whole xml element
					xmlpp::Element* currentdst = dst;
					const auto array = rnd.get(repeat_array);
					for(auto i = array.begin(); i != array.end();) {
						// first setup context variable
						rnd.put(repeat_variable, *i);
						// move to next source array element, if it is not end, then add next sibling
						++i;
						if(i != array.end())
							currentdst = currentdst->get_parent()->add_child(src->get_name());
					}
				} // if repeat_type 
			} // if visible
		} else { // if tagns == webpp://html5 or webpp://xml
			// look for pair(tagns,tagname) handler

			auto parent = dst->get_parent();
			parent->remove_child(dst);

			auto tag = context_.find_tag(src->get_namespace_uri(), src->get_name());
			if(tag == nullptr) {
				const xmlns* nshandler = context_.find_xmlns(src->get_namespace_uri());
				if(!nshandler)
					throw std::runtime_error( (boost::format("webpp::xml::fragmet::process_node(): required custom tag %s in ns %s (or namespace handler) not found") % src->get_name() % src->get_namespace_uri()).str());

				nshandler->tag(parent, src, rnd);
			}
			tag->render(parent, src, rnd);
		} // // if tagns == webpp://html5 or webpp://xml
	}

	void fragment::process_children(const xmlpp::Element* src, xmlpp::Element* dst, render_context& rnd) {
		for(auto child : src->get_children()) {
			const xmlpp::Element* childelement = dynamic_cast<xmlpp::Element*>(child);
			if(childelement != nullptr) {
				xmlpp::Element* e = dst->add_child(child->get_name());				
				process_node(childelement, e, rnd);
			} else {
				dst->import_node(src);
			} // if childelement != nullptr
		}
	}


}}




#ifndef WEBPP_TAGLIB_HPP
#define WEBPP_TAGLIB_HPP

#include "xmllib.hpp"
#include "stacked_exception.h"
namespace webpp { namespace xml { namespace taglib {
	/*! \brief XMLNS handler for formatting attributes
	 *  \\example <a f:href="/users/#{user.name}" f:title="user #[user.name} - abuse level #{user.abuse|%.2f]">
	 */
	class format_xmlns : public xmlns {
		Glib::ustring format(const Glib::ustring& source, render::context& ctx) const {
			std::size_t last = 0, start;
			std::ostringstream result;
			while(start = source.find("#{", last), start != Glib::ustring::npos) {
				if(start != last)
					result << source.substr(last, start-last);

				auto pipe = source.find('|', start+1), end = source.find('}', start+1);
				if(end == Glib::ustring::npos) {
					throw std::runtime_error("#{ not terminated by }");
				}
				if(pipe != Glib::ustring::npos && pipe < end) {
					auto variable = source.substr(start+2, pipe - start-2);
					auto format = source.substr(pipe+1, end-pipe-1);
					if(format.empty()) {
						throw std::runtime_error("empty format string");
					}
					auto& var = ctx.get(variable);
					if(!var.is_value())
						throw std::runtime_error("format: required variable '" + variable + "' not found in render context");
					result << var.get_value().format(format);
				} else {
					auto variable = source.substr(start+2, end-start-2);
					auto& var = ctx.get(variable);
					if(!var.is_value())
						throw std::runtime_error("output: required variable '" + variable + "' not found in render context");
					result << var.get_value().output();
				}
				last = end+1;
			}
			if(last != source.length())
				result << source.substr(last);
			return result.str();
		}

	public:
		virtual void tag(xmlpp::Element* dst, const xmlpp::Element* src , render::context& ctx) const {
			STACKED_EXCEPTIONS_ENTER();
			xmlpp::Element *target;
			if(src->get_name() == "text") {
                target = dst->get_parent();
                if(target == nullptr)
                    throw std::runtime_error("format: text node cannot be root node");
                target->remove_child(dst);
			} else {
                target = dst;
                target->set_name(src->get_name());
				for(const xmlpp::Attribute* i : src->get_attributes()) {
					if(i->get_namespace_uri() == "webpp://xml" || i->get_namespace_uri() == "webpp://html5") {
						target->set_attribute(i->get_name(), i->get_value());
					} else if(i->get_namespace_uri() == "webpp://format") {
						attribute(target, i, ctx);
					} else if(i->get_namespace_uri() == "webpp://control") {
						// ignore control attributes, core handles it
					} else {
						throw std::runtime_error("webpp://format tags support only XML/HTML5/webpp://format attributes, not " + i->get_namespace_uri());
					}
				}
			}
			for(xmlpp::Node* i : src->get_children()) {
				xmlpp::TextNode* ti = dynamic_cast<xmlpp::TextNode*>(i);
				xmlpp::CommentNode* ci = dynamic_cast<xmlpp::CommentNode*>(i);
				xmlpp::CdataNode *cdi = dynamic_cast<xmlpp::CdataNode*>(i);
				if(ti != nullptr)
					target->add_child_text(format(ti->get_content(), ctx));
				else if(ci != nullptr)
					target->add_child_comment(format(ci->get_content(), ctx));
				else if(cdi != nullptr)
					target->add_child_cdata(format(cdi->get_content(), ctx));
				else
					throw std::runtime_error("webpp://format rendered tag can contain only text, comment or cdata nodes");

			}
			STACKED_EXCEPTIONS_LEAVE("tag " + src->get_namespace_uri() + ":" + src->get_name());
		}

		virtual void attribute(xmlpp::Element* dst, const xmlpp::Attribute* src, render::context& ctx) const {
			STACKED_EXCEPTIONS_ENTER();
			dst->set_attribute(src->get_name(), format(src->get_value(), ctx));
			STACKED_EXCEPTIONS_LEAVE("attribute " + src->get_namespace_uri() + ":" + src->get_name());
		}			
	};

	struct basic {
		template<typename TagsT, typename XmlnsesT>
		static void process(TagsT&, XmlnsesT& xmlnses) {
			xmlnses["webpp://format"].reset(new format_xmlns);
		}
	};
}}}
#endif // WEBPP_TAGLIB_HPP

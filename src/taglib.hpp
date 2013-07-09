#ifndef WEBPP_TAGLIB_HPP
#define WEBPP_TAGLIB_HPP

#include "xmllib.hpp"

namespace webpp { namespace xml { namespace taglib {
	/// <render value="..."(required) [default="text to insert, if value not found in render ctx", optional, defaults to ""] [required="false", if "false" and no value in ctx, use default value, optional, defaults to true] />
	class tag_render : public tag {
	public:
		virtual void render(xmlpp::Element* dst, const xmlpp::Element* src, render_context& ctx) const {
			auto value = src->get_attribute("value");
			if(!value)
				throw std::runtime_error((boost::format("webpp::xml::taglib::tag_render: <render> at line %d requires value attribute") % src->get_line()).str());
			auto strvalue = value->get_value();
			auto fmt = src->get_attribute("format");
			auto req = src->get_attribute("required");
			auto dfl = src->get_attribute("default");
			bool required = true;
			Glib::ustring defval;
			
			if(req) {
				if(req->get_value() == "false")
					required = false;
				else if(req->get_value() != "true")
					throw std::runtime_error((boost::format("webpp::xml::taglib::tag_render: <render> at line %d: required=\"true|false\", not '%s'") % src->get_line() % req->get_value()).str());
			}

			if(dfl)
				defval = dfl->get_value();

			auto val = ctx.get(strvalue);

			if(val.has_value()) {
				if(fmt)
					dst->add_child_text( ctx.get(strvalue).value().format(fmt->get_value()));
				else
					dst->add_child_text( ctx.get(strvalue).value().output());
			} else if(!defval.empty()) 
				dst->add_child_text(defval);
			else if(required)
				throw std::runtime_error((boost::format("webpp::xml::taglib::tag_render: <render> at line %d requires value '%s' in render context") % src->get_line() % strvalue).str());
		}
	};

	/*! \brief XMLNS handler for formatting attributes
	 *  \\example <a f:href="/users/#{user.name}" f:title="user #[user.name} - abuse level #{user.abuse|%.2f]">
	 */
	class format_xmlns : public xmlns {
	public:
		virtual void tag(xmlpp::Element* /*dst*/, const xmlpp::Element* /* src */, render_context& /* ctx */ ) const {
			throw std::runtime_error("tag() is not implemented in this xmlns");
		}

		virtual void attribute(xmlpp::Element* dst, const xmlpp::Attribute* src, render_context& ctx) const {
			Glib::ustring source = src->get_value();
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
					auto var = ctx.get(variable);
					if(!var.has_value())
						throw std::runtime_error("format: required variable '" + variable + "' not found in render context");
					result << var.value().format(format);
				} else {
					auto variable = source.substr(start+2, end-start-2);
					auto var = ctx.get(variable);
					if(!var.has_value())
						throw std::runtime_error("output: required variable '" + variable + "' not found in render context");
					result << var.value().output();
				}
				last = end+1;
			}
			if(last != source.length())
				result << source.substr(last);
			dst->set_attribute(src->get_name(), result.str());
		}
	};

	struct basic {
		template<typename TagsT, typename XmlnsesT>
		static void process(TagsT& tags, XmlnsesT& xmlnses) {
			tags[std::make_pair(Glib::ustring("webpp://basic"), Glib::ustring("render"))].reset(new tag_render);
			xmlnses["webpp://format"].reset(new format_xmlns);
		}
	};
}}}
#endif // WEBPP_TAGLIB_HPP

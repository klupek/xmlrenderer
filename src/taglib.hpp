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
/*
	/// <a href.render="user.profilelink">...</a>
	/// <a href.render="user.nick" href.format="/users/%s" [ href.default="anonymous", defaults to "", optional ] [ required="false", defaults to true, optional ]>...</a>
	class subattribute_render : public subattribute {
	public:
		virtual std::wstring render(std::map<std::string, std::wstring>& attributes, const render_context& ctx) const {
			auto rnd = attributes.find("render"); // this attribute name			
			std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
			const std::string rndval = conv.to_bytes(rnd->second);
			attributes.erase(rnd);
			auto fmt = attributes.find("format");
			auto req = attributes.find("required");
			auto dfl = attributes.find("default");
			bool required = true;
			std::wstring defval;
			if(req != attributes.end()) {
				if(req->second == L"false")
					required = false;
				else if(req->second != L"true")
					throw std::runtime_error((boost::format("webpp::xml::taglib::tag_render: attribute .render required=\"true|false\", not '%s'") % *req).str());
				attributes.erase(req);
			}
			if(dfl != attributes.end()) {
				defval = dfl->second;
				attributes.erase(dfl);
			}

			
			auto ctxval = ctx.get(rndval, required);
			if(ctxval) {
				if(fmt != attributes.end()) {
					const std::wstring fmtstr = fmt->second;
					attributes.erase(fmt);
					return ctxval->format(fmtstr);
				} else
					return ctxval->output();
			} else {
				return defval;
			}
		}
	};
*/
	struct basic {
		template<typename TagsT, typename SubattributesT>
		static void process(TagsT& tags, SubattributesT& subattributes) {
			tags[std::make_pair(std::string("webpp://basic"), std::string("render"))].reset(new tag_render);
//			subattributes.emplace("render", new attribute_render);
		}
	};
	

/*

	// renderables 
	class renderable_base {
	public:
		virtual std::wstring render(std::list<std::string> filters) const = 0;
	};

	class string : public renderable_base {
		std::wstring value_;
	public:
		string(const std::string& value, const std::string& encoding) {
			std::wstring_convert<std::codecvt_byname<char, wchar_t, std::mbstate_t>> conv(encoding);
			value_ = conv.from_bytes(value);
		}

		virtual std::wstring render(std::list<std::string> filters) const {
			std::wstring value = value_;
			for(auto i : filters)
				value = filter_base<std::wstring>::find(i).filter(value);
			return value;
		}
	};

	template<typename T>
	class renderable : public renderable_base {
		T obj_;
	public:
		renderable(const T& object)
			: obj_(object) {}

		virtual std::wstring render(std::list<std::string> filters) const {
			if(filters.empty())
				return boost::lexical_cast<std::wstring>(obj_);
			else {
				auto i = filters.begin();
				std::wstring value = filter_base<T>::find(*i).filter(obj_);
				for(++i; i != filters.end(); ++i) {
					value = filter_base<std::wstring>::find(*i).filter(value);
				}
				return value;
			}
		}
	};
*/
}}}
#endif // WEBPP_TAGLIB_HPP

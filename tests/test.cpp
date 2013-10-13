#include <sstream>
#include <xmlrenderer/xmlrenderer.hpp>
#include <cassert>
#define BOOST_TEST_MODULE XmlRendererTest
#include <boost/test/unit_test.hpp>

std::string readfile(const std::string& name) {	
    auto filepath = (boost::filesystem::path(__FILE__).parent_path() / name).string();	
    std::ifstream ifs(filepath);
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return oss.str();
}

#define texcept(call, etype, emsg) \
	BOOST_CHECK_EXCEPTION((call), etype, [=](const etype& e) { \
		BOOST_CHECK_EQUAL(e.what(), std::string((emsg))); \
		return e.what() == std::string((emsg)); \
	})

// test basic int and bool values
BOOST_AUTO_TEST_CASE(context_render_value) {
	BOOST_TEST_CHECKPOINT("Test 1: context render value");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.abuse";
	ctx.create_value(key, 42);

	BOOST_CHECK_EQUAL(true, ctx.get("users").empty());
	BOOST_CHECK_EQUAL(true, ctx.get("users.asdf").empty());
	BOOST_CHECK_EQUAL(true, ctx.get("users..asdf.abuse").empty());
	BOOST_CHECK_EQUAL(false, ctx.get(key).empty());
	BOOST_CHECK_EQUAL(ctx.get(key).get_value().output(), "42");
	texcept(ctx.get(key).get_value().is_true(), webpp::stacked_exception, "render::value<i>::is_true(): '42' is not a boolean");
	key = "users.nolife.abuse";
	BOOST_CHECK_EQUAL(true, ctx.get(key).empty());

	key = "users.asdf.abuser";
	ctx.create_value(key, true);
	BOOST_CHECK_EQUAL(true, ctx.get(key).get_value().is_true());
}

// test arrays under keys
BOOST_AUTO_TEST_CASE(context_render_array) {
	BOOST_TEST_CHECKPOINT("Test 2: context render array");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.ofiary";
	auto& array = ctx.create_array(key);
	array.add().create_value("sot");
	array.add().create_value("drajwer");
	
	BOOST_CHECK_EQUAL(true, ctx.get("users").empty());
	BOOST_CHECK_EQUAL(true, ctx.get("users.asdf").empty());
	BOOST_CHECK_EQUAL(true, ctx.get("users..asdf.ofiary").empty());
	BOOST_CHECK_EQUAL(false, ctx.get(key).empty());

	auto& ar2 = ctx.get(key).get_array();
	ar2.reset();

	BOOST_CHECK_EQUAL(true, ar2.has_next());
	BOOST_CHECK_EQUAL(ar2.next().get_value().output(), "sot");

	BOOST_CHECK_EQUAL(true, ar2.has_next());
	BOOST_CHECK_EQUAL(ar2.next().get_value().output(), "drajwer");
	BOOST_CHECK_EQUAL(false, ar2.has_next());
}

// test lambdas returning bools or integers
BOOST_AUTO_TEST_CASE(context_render_lambda) {
	BOOST_TEST_CHECKPOINT("Test 3: lazy evaluated callback");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.abuse";

	ctx.create_lambda(key, []() {
		return 42;
	});

	BOOST_CHECK_EQUAL(ctx.get(key).get_value().output(), "42");
	texcept(ctx.get(key).get_value().is_true(), webpp::stacked_exception, "render::value<i>::is_true(): '42' is not a boolean");

	key = "users.asdf.abuser";
	ctx.create_lambda(key, []() { return true; });
	BOOST_CHECK_EQUAL(true, ctx.get(key).get_value().is_true());
	BOOST_CHECK_EQUAL(ctx.get(key).get_value().format("%d"), "1");
}

// test XML fragment rendering without using render context values
BOOST_AUTO_TEST_CASE(xml_fragment) {
	BOOST_TEST_CHECKPOINT("Test 4: XML fragment");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	webpp::xml::fragment f1("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>", ctx);
    webpp::xml::prepared_fragment pf1(f1, ctx);
	BOOST_CHECK_EQUAL(pf1.render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");


	webpp::xml::fragment f2("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>", ctx);
    webpp::xml::prepared_fragment pf2(f2, ctx);

	BOOST_CHECK_EQUAL(pf2.render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>\n");
}

// same as above, but use proper context loading
BOOST_AUTO_TEST_CASE(context_basics) {
	BOOST_TEST_CHECKPOINT("Test 5: context basics");
	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>");
	ctx.put("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>");

	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
	BOOST_CHECK_EQUAL(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><t:foo/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "required custom tag foo in ns webpp://test (or namespace handler) not found");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><foo t:abuse=\"1\"/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "unknown attribute namespace  webpp://test");
}


// context with taglib - formatting tags
BOOST_AUTO_TEST_CASE(taglib_format) {
	BOOST_TEST_CHECKPOINT("Test 6: context with taglib - formattings tags");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><b><f:text>#{testval}</f:text></b></rootnode>");

	// lets try it first without value
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "output: required variable 'testval' not found in render context");

	// then, with some value
	rnd.create_value("testval", "abuser<>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>abuser&lt;&gt;</b></rootnode>\n");

	// and some other value
	rnd.create_value("testval", 42);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>42</b></rootnode>\n");


	// now, lets try formatting

	ctx.put("testek2", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><f:b>#{testval|%.3f}</f:b></rootnode>");
	rnd.create_value("testval", 3.1415);
	BOOST_CHECK_EQUAL(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>3.142</b></rootnode>\n");

	// if-(not)-exists combination

	ctx.put("testek3", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\"><f:b c:visible-if=\"testval2 is not null\">#{testval2|%.3f}</f:b><b c:visible-if=\"testval2 is null\">bezcenne</b></rootnode>");
	BOOST_CHECK_EQUAL(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bezcenne</b></rootnode>\n");

	rnd.create_value("testval2", 12.34567);
	BOOST_CHECK_EQUAL(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>12.346</b></rootnode>\n");
}

// context with taglib - format attribute namespace
BOOST_AUTO_TEST_CASE(taglib_format_ns) {
	BOOST_TEST_CHECKPOINT("Test 7: context with taglib - format attribute namespace");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// syntax error
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"#{user.name\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "#{ not terminated by }");

	// bad format
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"#{user.name|}\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "empty format string");

	// missing variable
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"/users/#{user.name}\" f:title=\"user #{user.name} - abuse level #{user.abuse|%.2f}, wiec to abuser\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "output: required variable 'user.name' not found in render context");

	// missing second variable
	rnd.create_value("user.name", "asdf");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "format: required variable 'user.abuse' not found in render context");

	// everything set
	rnd.create_value("user.abuse", M_PI);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><a href=\"/users/asdf\" title=\"user asdf - abuse level 3.14, wiec to abuser\"/></rootnode>\n");
}


// visibility controls
BOOST_AUTO_TEST_CASE(ctrl_visibility) {
	BOOST_TEST_CHECKPOINT("Test 8: render visibility controls");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// random misspelled word from control namespace
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:if-egzists=\"testval\" f:title=\"#{testval}\">test <!-- test2 --> <i><f:text>#{testval}</f:text></i></b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "webpp://control atribute if-egzists is not implemented");

	// testval is not set, but won't be evaulated - element is not visible
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\">foobar!<b c:visible-if=\"testval is not null\" f:title=\"#{testval}\">test <!-- test2 --> <i><f:text>#{testval}</f:text></i></b>foobaz!</rootnode>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!foobaz!</rootnode>\n");

	rnd.create_value("testval", 42);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!<b title=\"42\">test <!-- test2 --> <i>42</i></b>foobaz!</rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:visible-if=\"testval2 is null\">testval2 is not set</b><f:b c:visible-if=\"testval2 is not null\">testval value is #{testval2}</f:b></rootnode>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval2 is not set</b></rootnode>\n");

	rnd.create_value("testval2", "abuse");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval value is abuse</b></rootnode>\n");

	// if-true missing variable
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:visible-if=\"testval3 is true\">foo</b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "Expression error: Expected boolean value\n1. At token is_true(value = variable(testval3))\n");

	// if-true on non-boolean value
	rnd.create_value("testval3", 42);
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "Expression error: render::value<i>::is_true(): '42' is not a boolean\n1. At token is_true(value = variable(testval3))\n");

	// if-(not)-true cascade test
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:visible-if=\"testval3 is true\"><i c:visible-if=\"testval4 is not true\">foo</i>bar</b><b c:visible-if=\"testval3 is not true\"><i c:visible-if=\"testval4 is true\">foo</i>baz</b></rootnode>");
	rnd.create_value("testval3", true);
	rnd.create_value("testval4", false);

	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>bar</b></rootnode>\n");

	rnd.create_value("testval3", false);
	rnd.create_value("testval4", true);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>baz</b></rootnode>\n");

	rnd.create_value("testval3", true);
	rnd.create_value("testval4", true);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bar</b></rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><f:text c:visible-if=\"testval3 is true\">#{testval3}</f:text></rootnode>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>1</rootnode>\n");

	rnd.create_value("testval3", false);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
}

// control - inner repeat
BOOST_AUTO_TEST_CASE(ctrl_inner_repeat) {
	BOOST_TEST_CHECKPOINT("Test 9: render inner repeat");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\" c:repeat=\"iner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><f:p>abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></root>");

	// misspeled repeat
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "repeat must be one of (inner,outer), not 'iner' in line '1', tag 'root'");

	// no array
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\" c:repeat=\"inner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><f:p f:data-level=\"#{abuser.level}\">abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></root>");
    texcept(ctx.get("testek").render(rnd).xml().to_string(), webpp::stacked_exception, "no array in this node");

	auto& array = rnd.create_array("abuserzy");

	auto& a = array.add();
	a.find("name").create_value("asdf");


	auto& b = array.add();
	b.find("name").create_value("abuser");


	// missing level in inner repeat
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "output: required variable 'abuser.level' not found in render context");

	a.find("level").create_value(M_PI);
	b.find("level").create_value(M_PI_4);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></root>\n");

	// same as above, but repeat other then root's children
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\">foo!<div c:repeat=\"inner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><f:p f:data-level=\"#{abuser.level}\">abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></div>bar!</root>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root>foo!<div><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></div>bar!</root>\n");
}

// control - outer repeat
BOOST_AUTO_TEST_CASE(ctrl_outer_repeat) {
	BOOST_TEST_CHECKPOINT("Test 10: render outer repeat");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// not possible repeat - outer on root element
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><f:p>abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></root>");
    texcept(ctx.get("testek").render(rnd).xml().to_string(), webpp::stacked_exception, "outer repeat on root element is not possible");

	// empty array
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><foo/><div c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\" f:data-level=\"dec(#{abuser.level|%03.4f})\"><f:p>abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></div><bar/></root>");
    texcept(ctx.get("testek").render(rnd).xml().to_string(),webpp::stacked_exception, "no array in this node");

	auto& array = rnd.create_array("abuserzy");

	auto& a = array.add();
	a.find("name").create_value("asdf");


	auto& b = array.add();
	b.find("name").create_value("abuser");

	// missing variable inside repeat
    texcept(ctx.get("testek").render(rnd).xml().to_string(), webpp::stacked_exception, "format: required variable 'abuser.level' not found in render context");

	a.find("level").create_value(M_PI);
	b.find("level").create_value(M_PI_4);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><div data-level=\"dec(3.1416)\"><p>abuser asdf, poziom 3.1</p></div><div data-level=\"dec(0.7854)\"><p>abuser abuser, poziom 0.8</p></div><bar/></root>\n");
}

// render context - lazy evaluated array
BOOST_AUTO_TEST_CASE(render_lazy_array) {
	BOOST_TEST_CHECKPOINT("Test 11: lazy evaluated array");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	class test_dynamic_array : public webpp::xml::render::array_base {
		int x; double y;
		std::shared_ptr<webpp::xml::render::tree_element> object_;
	public:
		test_dynamic_array() : x(0), y(1), object_(std::make_shared<webpp::xml::render::tree_element>()) {}
		virtual webpp::xml::render::tree_element& next() {
			object_->find("x").create_value(x);
			object_->find("y").create_value(y);
			x += 1;
			y *= M_PI;
			return *object_;
		}

		virtual bool has_next() const {
			return x != 3;
		}
		virtual bool empty() const {
			return false;
		}
		virtual void reset() {
			x = 0; y = 1;
		}
		virtual size_t size() const { return 3; }
	};

	rnd.get("abuserzy").create_array<test_dynamic_array>();

	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><foo/><div c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\" f:data-level=\"dec(#{abuser.y|%03.4f})\"><f:p>x = #{abuser.x}, poziom #{abuser.y|%.1f}</f:p></div><bar/></root>");
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><div data-level=\"dec(1.0000)\"><p>x = 0, poziom 1.0</p></div><div data-level=\"dec(3.1416)\"><p>x = 1, poziom 3.1</p></div><div data-level=\"dec(9.8696)\"><p>x = 2, poziom 9.9</p></div><bar/></root>\n");
}

// c:insert, insert view into current node
BOOST_AUTO_TEST_CASE(ctrl_insert) {
	BOOST_TEST_CHECKPOINT("Test 12: c:insert");

    webpp::xml::context ctx(".");
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();
    ctx.put("testek1", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert name=\"innertestek\" value-prefix=\"\" /><bar /></root>");
    ctx.put("testek2", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert name=\"innertestek\" value-prefix=\"foo.bar\" /><bar /></root>");
    ctx.put("innertestek", "<f:b xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" f:data-notb=\"#{numberofthebeast}\">notb = #{numberofthebeast}</f:b>");


    rnd.create_value("numberofthebeast", 667);
    rnd.create_value("foo.bar.numberofthebeast", 666);

	BOOST_CHECK_EQUAL(ctx.get("testek1").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"667\">notb = 667</b><bar/></root>\n");
	BOOST_CHECK_EQUAL(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"666\">notb = 666</b><bar/></root>\n");

    // and once again
	BOOST_CHECK_EQUAL(ctx.get("testek1").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"667\">notb = 667</b><bar/></root>\n");
	BOOST_CHECK_EQUAL(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"666\">notb = 666</b><bar/></root>\n");
    
    // more complicated 

    auto &array = rnd.create_array("beasts");
    array.add().find("numberofthebeast").create_value(42);
    array.add().find("numberofthebeast").create_value(139);
    
    ctx.put("testek3", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert c:repeat=\"outer\" c:repeat-variable=\"notb\" c:repeat-array=\"beasts\" name=\"innertestek\" value-prefix=\"notb\" /><bar /></root>");

	BOOST_CHECK_EQUAL(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"42\">notb = 42</b><b data-notb=\"139\">notb = 139</b><bar/></root>\n");
}

BOOST_AUTO_TEST_CASE(custom_namespace) {
	BOOST_TEST_CHECKPOINT("Test 13: test custom namespace");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();

    ctx.put("testek", "<root xmlns=\"http://example.org/example\" xmlns:f=\"webpp://format\" xmlns:c=\"http://example.org/example2\"><f:p>#{value}</f:p><c:example><f:text>#{value} - </f:text>blah</c:example></root>");
    rnd.create_value("value", 42);
	BOOST_CHECK_EQUAL(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root xmlns=\"http://example.org/example\" xmlns:c=\"http://example.org/example2\"><p>42</p><c:example>42 - blah</c:example></root>\n");
}

BOOST_AUTO_TEST_CASE(html5_boilerplate) {
	BOOST_TEST_CHECKPOINT("Test 14: html5 bolilerplate loading, parsing and writing");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();

    auto expected = readfile("boilerplate.output");
	BOOST_CHECK_EQUAL(ctx.get("boilerplate").render(rnd).xhtml5(webpp::xml::fragment_output::DOCTYPE | webpp::xml::fragment_output::REMOVE_XML_DECLARATION).to_string(), expected);

    auto expected2 = readfile("boilerplate-nocomment.output");
	BOOST_CHECK_EQUAL(
           ctx.get("boilerplate")
                .render(rnd)
                .xhtml5(webpp::xml::fragment_output::DOCTYPE | webpp::xml::fragment_output::REMOVE_XML_DECLARATION | webpp::xml::fragment_output::REMOVE_COMMENTS)
                .to_string(), expected2);
}

BOOST_AUTO_TEST_CASE(subview_insert) {
	BOOST_TEST_CHECKPOINT("Test 15: subview insertion and render");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();
    ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><div id=\"content\" /><bar /></root>");
    ctx.put("innertestek", "<f:b xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" f:data-notb=\"#{numberofthebeast}\">notb = #{numberofthebeast}</f:b>");

    rnd.create_value("test-value-prefix.numberofthebeast", M_PI);
    rnd.create_value("numberofthebeast", 42);

	BOOST_CHECK_EQUAL(
        ctx.get("testek").insert("content", "innertestek", "test-value-prefix").render(rnd).xml().to_string(),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"3.1415926535897931\" id=\"content\">notb = 3.1415926535897931</b><bar/></root>\n"
    );

	BOOST_CHECK_EQUAL(
        ctx.get("testek").insert("content", "innertestek", "").render(rnd).xml().to_string(),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"42\" id=\"content\">notb = 42</b><bar/></root>\n"
    );

}

BOOST_AUTO_TEST_CASE(xslt) {
	BOOST_TEST_CHECKPOINT("Test 16: XSL stylesheets");

	webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
	webpp::xml::render::context rnd;

	auto f2 = ctx.get("xslt-test-result");
	ctx.attach_xslt("xslt-test");
	auto f1 = ctx.get("xslt-test");
	const Glib::ustring sf1 = const_cast<xmlpp::Document*>(&f1.get_fragment().get_document())->write_to_string_formatted();
	const Glib::ustring sf2 = const_cast<xmlpp::Document*>(&f2.get_fragment().get_document())->write_to_string_formatted();
	BOOST_CHECK_EQUAL(sf1, sf2);
}

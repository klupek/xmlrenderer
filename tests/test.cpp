
#include "../xmlrenderer/xmlrenderer.hpp"
#include <cassert>

std::string readfile(const std::string& name) {
    auto filepath = (boost::filesystem::path(__FILE__).parent_path() / name).string();
    std::ifstream ifs(filepath);
    ifs.seekg(0, std::ios_base::end);
    std::vector<char> result;
    std::size_t size = ifs.tellg();
    result.reserve(size+1);
    ifs.seekg(0, std::ios_base::beg);
    ifs.read(result.data(), size);
    ifs.close();
    result[size] = 0;
    return std::string(result.data());
}

#define tbegin(name) std::cout << name << std::endl
#define tequal(a,b) if((a) != (b)) { std::cout << "\nFAIL: expected: " << b << "\nFAIL:   actual: " << a << std::endl; assert(false); }
#define ttrue(a) assert(a)
#define texcept(call, etype, emsg) try { (call); assert(false && "Expected exception " emsg ", but was not raised"); } catch(const etype& e) { /*std::cout << e.format() */; tequal(e.what(), std::string(emsg)); }

// test basic int and bool values
void t1() {
	tbegin("Test 1: context render value");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.abuse";
	ctx.create_value(key, 42);

	ttrue(ctx.get("users").empty());
	ttrue(ctx.get("users.asdf").empty());
	ttrue(ctx.get("users..asdf.abuse").empty());
	ttrue(!ctx.get(key).empty());
	tequal(ctx.get(key).get_value().output(), "42");
	texcept(ctx.get(key).get_value().is_true(), webpp::stacked_exception, "render::value<i>::is_true(): '42' is not a boolean");
	key = "users.nolife.abuse";
	ttrue(ctx.get(key).empty());

	key = "users.asdf.abuser";
	ctx.create_value(key, true);
	ttrue(ctx.get(key).get_value().is_true());
}

// test arrays under keys
void t2() {
	tbegin("Test 2: context render array");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.ofiary";
	auto& array = ctx.create_array(key);
	array.add().create_value("sot");
	array.add().create_value("drajwer");
	
	ttrue(ctx.get("users").empty());
	ttrue(ctx.get("users.asdf").empty());
	ttrue(ctx.get("users..asdf.ofiary").empty());
	ttrue(!ctx.get(key).empty());

	auto& ar2 = ctx.get(key).get_array();
	ar2.reset();

	ttrue(ar2.has_next());
	tequal(ar2.next().get_value().output(), "sot");

	ttrue(ar2.has_next());
	tequal(ar2.next().get_value().output(), "drajwer");
	ttrue(!ar2.has_next());
}

// test lambdas returning bools or integers
void t3() {
	tbegin("Test 3: lazy evaluated callback");

	webpp::xml::render::context ctx;
	std::string key = "users.asdf.abuse";

	ctx.create_lambda(key, []() {
		return 42;
	});

	tequal(ctx.get(key).get_value().output(), "42");
	texcept(ctx.get(key).get_value().is_true(), webpp::stacked_exception, "render::value<i>::is_true(): '42' is not a boolean");

	key = "users.asdf.abuser";
	ctx.create_lambda(key, []() { return true; });
	ttrue(ctx.get(key).get_value().is_true());
	tequal(ctx.get(key).get_value().format("%d"), "1");
}

// test XML fragment rendering without using render context values
void t4() {
	tbegin("Test 4: XML fragment");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	webpp::xml::fragment f1("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>", ctx);
    webpp::xml::prepared_fragment pf1(f1, ctx);
    tequal(pf1.render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");


	webpp::xml::fragment f2("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>", ctx);
    webpp::xml::prepared_fragment pf2(f2, ctx);

    tequal(pf2.render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>\n");
}

// same as above, but use proper context loading
void t5() {
	tbegin("Test 5: context basics");
	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>");
	ctx.put("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>");

    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
    tequal(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><t:foo/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "required custom tag foo in ns webpp://test (or namespace handler) not found");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><foo t:abuse=\"1\"/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "unknown attribute namespace  webpp://test");
}


// context with taglib - formatting tags
void t6() {
	tbegin("Test 6: context with taglib - formattings tags");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><b><f:text>#{testval}</f:text></b></rootnode>");

	// lets try it first without value
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "output: required variable 'testval' not found in render context");

	// then, with some value
	rnd.create_value("testval", "abuser<>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>abuser&lt;&gt;</b></rootnode>\n");

	// and some other value
	rnd.create_value("testval", 42);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>42</b></rootnode>\n");


	// now, lets try formatting

	ctx.put("testek2", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><f:b>#{testval|%.3f}</f:b></rootnode>");
	rnd.create_value("testval", 3.1415);
    tequal(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>3.142</b></rootnode>\n");

	// if-(not)-exists combination

	ctx.put("testek3", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\"><f:b c:if-exists=\"testval2\">#{testval2|%.3f}</f:b><b c:if-not-exists=\"testval2\">bezcenne</b></rootnode>");
    tequal(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bezcenne</b></rootnode>\n");

	rnd.create_value("testval2", 12.34567);
    tequal(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>12.346</b></rootnode>\n");
}

// context with taglib - format attribute namespace
void t7() {
	tbegin("Test 7: context with taglib - format attribute namespace");

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
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><a href=\"/users/asdf\" title=\"user asdf - abuse level 3.14, wiec to abuser\"/></rootnode>\n");
}


// visibility controls
void t8() {
	tbegin("Test 8: render visibility controls");

	webpp::xml::context ctx(".");
	webpp::xml::render::context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// random misspelled word from control namespace
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:if-egzists=\"testval\" f:title=\"#{testval}\">test <!-- test2 --> <i><f:text>#{testval}</f:text></i></b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "webpp://control atribute if-egzists is not implemented");

	// testval is not set, but won't be evaulated - element is not visible
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\">foobar!<b c:if-exists=\"testval\" f:title=\"#{testval}\">test <!-- test2 --> <i><f:text>#{testval}</f:text></i></b>foobaz!</rootnode>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!foobaz!</rootnode>\n");

	rnd.create_value("testval", 42);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!<b title=\"42\">test <!-- test2 --> <i>42</i></b>foobaz!</rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:if-not-exists=\"testval2\">testval2 is not set</b><f:b c:if-exists=\"testval2\">testval value is #{testval2}</f:b></rootnode>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval2 is not set</b></rootnode>\n");

	rnd.create_value("testval2", "abuse");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval value is abuse</b></rootnode>\n");

	// if-true missing variable
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:if-true=\"testval3\">foo</b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "variable 'testval3' required from <b> at line 1, attribute if-true, is missing");

	// if-true on non-boolean value
	rnd.create_value("testval3", 42);
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "render::value<i>::is_true(): '42' is not a boolean");

	// if-(not)-true cascade test
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><b c:if-true=\"testval3\"><i c:if-not-true=\"testval4\">foo</i>bar</b><b c:if-not-true=\"testval3\"><i c:if-true=\"testval4\">foo</i>baz</b></rootnode>");
	rnd.create_value("testval3", true);
	rnd.create_value("testval4", false);

    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>bar</b></rootnode>\n");

	rnd.create_value("testval3", false);
	rnd.create_value("testval4", true);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>baz</b></rootnode>\n");

	rnd.create_value("testval3", true);
	rnd.create_value("testval4", true);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bar</b></rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><f:text c:if-true=\"testval3\">#{testval3}</f:text></rootnode>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>1</rootnode>\n");

	rnd.create_value("testval3", false);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
}

// control - inner repeat
void t9() {
	tbegin("Test 9: render inner repeat");

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
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></root>\n");

	// same as above, but repeat other then root's children
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:c=\"webpp://control\">foo!<div c:repeat=\"inner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><f:p f:data-level=\"#{abuser.level}\">abuser #{abuser.name}, poziom #{abuser.level|%.1f}</f:p></div>bar!</root>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root>foo!<div><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></div>bar!</root>\n");
}

// control - outer repeat
void t10() {
	tbegin("Test 10: render outer repeat");

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
    tequal(ctx.get("testek").render(rnd).xml().to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><div data-level=\"dec(3.1416)\"><p>abuser asdf, poziom 3.1</p></div><div data-level=\"dec(0.7854)\"><p>abuser abuser, poziom 0.8</p></div><bar/></root>\n");
}

// render context - lazy evaluated array
void t11() {
	tbegin("Test 11: lazy evaluated array");

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
	};

	rnd.get("abuserzy").create_array<test_dynamic_array>();

	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><foo/><div c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\" f:data-level=\"dec(#{abuser.y|%03.4f})\"><f:p>x = #{abuser.x}, poziom #{abuser.y|%.1f}</f:p></div><bar/></root>");
    tequal(ctx.get("testek").render(rnd).xml().to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><div data-level=\"dec(1.0000)\"><p>x = 0, poziom 1.0</p></div><div data-level=\"dec(3.1416)\"><p>x = 1, poziom 3.1</p></div><div data-level=\"dec(9.8696)\"><p>x = 2, poziom 9.9</p></div><bar/></root>\n");
}

// c:insert, insert view into current node
void t12() {
    tbegin("Test 12: c:insert");

    webpp::xml::context ctx(".");
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();
    ctx.put("testek1", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert name=\"innertestek\" value-prefix=\"\" /><bar /></root>");
    ctx.put("testek2", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert name=\"innertestek\" value-prefix=\"foo.bar\" /><bar /></root>");
    ctx.put("innertestek", "<f:b xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" f:data-notb=\"#{numberofthebeast}\">notb = #{numberofthebeast}</f:b>");


    rnd.create_value("numberofthebeast", 667);
    rnd.create_value("foo.bar.numberofthebeast", 666);

    tequal(ctx.get("testek1").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"667\">notb = 667</b><bar/></root>\n");
    tequal(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"666\">notb = 666</b><bar/></root>\n");

    // and once again
    tequal(ctx.get("testek1").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"667\">notb = 667</b><bar/></root>\n");
    tequal(ctx.get("testek2").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"666\">notb = 666</b><bar/></root>\n");
    
    // more complicated 

    auto &array = rnd.create_array("beasts");
    array.add().find("numberofthebeast").create_value(42);
    array.add().find("numberofthebeast").create_value(139);
    
    ctx.put("testek3", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><c:insert c:repeat=\"outer\" c:repeat-variable=\"notb\" c:repeat-array=\"beasts\" name=\"innertestek\" value-prefix=\"notb\" /><bar /></root>");

    tequal(ctx.get("testek3").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"42\">notb = 42</b><b data-notb=\"139\">notb = 139</b><bar/></root>\n");
}

void t13() {
    tbegin("Test 13: test custom namespace");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();

    ctx.put("testek", "<root xmlns=\"http://example.org/example\" xmlns:f=\"webpp://format\" xmlns:c=\"http://example.org/example2\"><f:p>#{value}</f:p><c:example><f:text>#{value} - </f:text>blah</c:example></root>");
    rnd.create_value("value", 42);
    tequal(ctx.get("testek").render(rnd).xml().to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root xmlns=\"http://example.org/example\" xmlns:c=\"http://example.org/example2\"><p>42</p><c:example>42 - blah</c:example></root>\n");
}

void t14() {
    tbegin("Test 14: html5 bolilerplate loading, parsing and writing");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();

    auto expected = readfile("boilerplate.output");
    tequal(ctx.get("boilerplate").render(rnd).xhtml5(webpp::xml::fragment_output::DOCTYPE | webpp::xml::fragment_output::REMOVE_XML_DECLARATION).to_string(), expected);

    auto expected2 = readfile("boilerplate-nocomment.output");
    tequal(
           ctx.get("boilerplate")
                .render(rnd)
                .xhtml5(webpp::xml::fragment_output::DOCTYPE | webpp::xml::fragment_output::REMOVE_XML_DECLARATION | webpp::xml::fragment_output::REMOVE_COMMENTS)
                .to_string(), expected2);
}

void t15() {
    tbegin("Test 15: subview insertion and render");

    webpp::xml::context ctx(boost::filesystem::path(__FILE__).parent_path().string());
    webpp::xml::render::context rnd;

    ctx.load_taglib<webpp::xml::taglib::basic>();
    ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"> <foo /><div id=\"content\" /><bar /></root>");
    ctx.put("innertestek", "<f:b xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" f:data-notb=\"#{numberofthebeast}\">notb = #{numberofthebeast}</f:b>");

    rnd.create_value("test-value-prefix.numberofthebeast", M_PI);
    rnd.create_value("numberofthebeast", 42);

    tequal(
        ctx.get("testek").insert("content", "innertestek", "test-value-prefix").render(rnd).xml().to_string(),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"3.1415926535897931\">notb = 3.1415926535897931</b><bar/></root>\n"
    );

    tequal(
        ctx.get("testek").insert("content", "innertestek", "").render(rnd).xml().to_string(),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root> <foo/><b data-notb=\"42\">notb = 42</b><bar/></root>\n"
    );

}


int main()
{
	t1();
	t2();
	t3();
	t4();
	t5();
	t6();
	t7();
	t8();
	t9();
	t10();
	t11();
    t12();
    t13();
    t14();
    t15();
	return 0;
}

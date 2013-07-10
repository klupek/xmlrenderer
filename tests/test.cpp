
#include "../src/xmllib.hpp"
#include "../src/stacked_exception.h"
#include <cassert>


#define tbegin(name) std::cout << name << std::endl
#define tequal(a,b) if((a) != (b)) { std::cout << "\nFAIL: expected: " << b << "\nFAIL:   actual: " << a << std::endl; assert(false); }
#define ttrue(a) assert(a)
#define texcept(call, etype, emsg) try { (call); assert(false && "Expected exception " emsg ", but was not raised"); } catch(const etype& e) { /*std::cout << e.format() */; tequal(e.what(), std::string(emsg)); }

// test basic int and bool values
void t1() {
	tbegin("Test 1: context render value");

	webpp::xml::render_context ctx;
	std::string key = "users.asdf.abuse";
	ctx.val(key, 42);

	ttrue(ctx.get("users").empty());
	ttrue(ctx.get("users.asdf").empty());
	ttrue(ctx.get("users..asdf.abuse").empty());
	ttrue(!ctx.get(key).empty());
	tequal(ctx.get(key).value().output(), "42");
	texcept(ctx.get(key).value().is_true(), webpp::stacked_exception, "render_value<i>::is_true(): '42' is not a boolean");
	key = "users.nolife.abuse";
	ttrue(ctx.get(key).empty());

	key = "users.asdf.abuser";
	ctx.val(key, true);
	ttrue(ctx.get(key).value().is_true());
}

// test arrays under keys
void t2() {
	tbegin("Test 2: context render array");

	webpp::xml::render_context ctx;
	std::string key = "users.asdf.ofiary";
	ctx.add_to_array(key).put_value("sot");
	ctx.add_to_array(key).put_value("drajwer");
	
	ttrue(ctx.get("users").empty());
	ttrue(ctx.get("users.asdf").empty());
	ttrue(ctx.get("users..asdf.ofiary").empty());
	ttrue(!ctx.get(key).empty());

	auto i = ctx.get(key).begin();
	ttrue(i != ctx.get(key).end());
	tequal(i->value().output(), "sot");
	++i;
	ttrue(i != ctx.get(key).end());
	tequal(i->value().output(), "drajwer");
	++i;
	ttrue(i == ctx.get(key).end());
}

// test lambdas returning bools or integers
void t3() {
	tbegin("Test 3: lazy evaluated callback");

	webpp::xml::render_context ctx;
	std::string key = "users.asdf.abuse";

	ctx.lambda(key, []() {
		return 42;
	});

	tequal(ctx.get(key).value().output(), "42");
	texcept(ctx.get(key).value().is_true(), webpp::stacked_exception, "render_value<i>::is_true(): '42' is not a boolean");

	key = "users.asdf.abuser";
	ctx.lambda(key, []() { return true; });
	ttrue(ctx.get(key).value().is_true());
	tequal(ctx.get(key).value().format("%d"), "1");
}

// test XML fragment rendering without using render context values
void t4() {
	tbegin("Test 4: XML fragment");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	webpp::xml::fragment f1("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>", ctx);
	tequal(f1.render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");


	webpp::xml::fragment f2("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>", ctx);

	tequal(f2.render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf foo=\"bar\"/><foobar/><!-- test --></rootnode2>\n");
}

// same as above, but use proper context loading
void t5() {
	tbegin("Test 5: context basics");
	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>");
	ctx.put("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>");

	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
	tequal(ctx.get("testek2").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><t:foo/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "required custom tag foo in ns webpp://test (or namespace handler) not found");

	ctx.put("testek3", "<rootnode2 xmlns=\"webpp://xml\" xmlns:t=\"webpp://test\"><foo t:abuse=\"1\"/><asdf/><foobar/></rootnode2>");
	texcept(ctx.get("testek3").render(rnd), webpp::stacked_exception, "unknown attribute namespace  webpp://test");
}

#include "../src/taglib.hpp"

// context with taglib - <render> tag
void t6() {
	tbegin("Test 6: context with taglib - render tag");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render value=\"testval\"/></b></rootnode>");

	// lets try it first without value
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "<render> requires value 'testval' in render context");

	// and without attribute
	ctx.put("testek2", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render/></b></rootnode>");
	texcept(ctx.get("testek2").render(rnd), webpp::stacked_exception, "<render> requires value attribute");

	// then, with some value
	rnd.val("testval", "abuser<>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>abuser&lt;&gt;</b></rootnode>\n");

	// and some other value
	rnd.val("testval", 42);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>42</b></rootnode>\n");


	// now, lets try formatting

	ctx.put("testek2", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval\"/></b></rootnode>");
	rnd.val("testval", 3.1415);
	tequal(ctx.get("testek2").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>3.142</b></rootnode>\n");

	// default value

	ctx.put("testek3", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval2\" default=\"bezcenne\"/></b></rootnode>");
	tequal(ctx.get("testek3").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bezcenne</b></rootnode>\n");

	rnd.val("testval2", 12.34567);
	tequal(ctx.get("testek3").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>12.346</b></rootnode>\n");

	// required attribute
	ctx.put("testek4", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval2\" required=\"abuser\"/></b></rootnode>");
	texcept(ctx.get("testek4").render(rnd), webpp::stacked_exception, "webpp::xml::taglib::tag_render: <render> at line 1: required=\"true|false\", not 'abuser'");

	ctx.put("testek4", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval3\" required=\"false\"/></b></rootnode>");
	tequal(ctx.get("testek4").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b/></rootnode>\n");
}

// context with taglib - format attribute namespace
void t7() {
	tbegin("Test 7: context with taglib - format attribute namespace");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// not implemented namespace tag
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><f:a href=\"#{user.name\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "tag() is not implemented in this xmlns");

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
	rnd.val("user.name", "asdf");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "format: required variable 'user.abuse' not found in render context");

	// everything set
	rnd.val("user.abuse", M_PI);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><a href=\"/users/asdf\" title=\"user asdf - abuse level 3.14, wiec to abuser\"/></rootnode>\n");
}


// visibility controls
void t8() {
	tbegin("Test 8: render visibility controls");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// random misspelled word from control namespace
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\"><b c:if-egzists=\"testval\" f:title=\"#{testval}\">test <!-- test2 --> <i><b:render value=\"testval\"/></i></b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "webpp://control atribute if-egzists is not implemented");

	// testval is not set, but won't be evaulated - element is not visible
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\">foobar!<b c:if-exists=\"testval\" f:title=\"#{testval}\">test <!-- test2 --> <i><b:render value=\"testval\"/></i></b>foobaz!</rootnode>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!foobaz!</rootnode>\n");

	rnd.val("testval", 42);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>foobar!<b title=\"42\">test <!-- test2 --> <i>42</i></b>foobaz!</rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\"><b c:if-not-exists=\"testval2\">testval2 is not set</b><b c:if-exists=\"testval2\">testval value is <b:render value=\"testval2\"/></b></rootnode>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval2 is not set</b></rootnode>\n");

	rnd.val("testval2", "abuse");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>testval value is abuse</b></rootnode>\n");

	// if-true missing variable
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\"><b c:if-true=\"testval3\">foo</b></rootnode>");
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "variable 'testval3' required from <b> at line 1, attribute if-true, is missing");

	// if-true on non-boolean value
	rnd.val("testval3", 42);
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "render_value<i>::is_true(): '42' is not a boolean");

	// if-(not)-true cascade test
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\"><b c:if-true=\"testval3\"><i c:if-not-true=\"testval4\">foo</i>bar</b><b c:if-not-true=\"testval3\"><i c:if-true=\"testval4\">foo</i>baz</b></rootnode>");
	rnd.val("testval3", true);
	rnd.val("testval4", false);

	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>bar</b></rootnode>\n");

	rnd.val("testval3", false);
	rnd.val("testval4", true);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b><i>foo</i>baz</b></rootnode>\n");

	rnd.val("testval3", true);
	rnd.val("testval4", true);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bar</b></rootnode>\n");

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\"><b:render c:if-true=\"testval3\" value=\"testval3\"/></rootnode>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode>1</rootnode>\n");

	rnd.val("testval3", false);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
}

// control - inner repeat
void t9() {
	tbegin("Test 9: render inner repeat");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\" xmlns:c=\"webpp://control\" c:repeat=\"iner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><p>abuser <b:render value=\"abuser.name\"/>, poziom <b:render value=\"abuser.level\" format=\"%.1f\"/></p></root>");

	// misspeled repeat
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "repeat must be one of (inner,outer), not 'iner' in line '1', tag 'root'");

	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\" xmlns:c=\"webpp://control\" c:repeat=\"inner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><p f:data-level=\"#{abuser.level}\">abuser <b:render value=\"abuser.name\"/>, poziom <b:render value=\"abuser.level\" format=\"%.1f\"/></p></root>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root/>\n");

	auto& a = rnd.add_to_array("abuserzy");
	a.find("name").put_value("asdf");


	auto& b = rnd.add_to_array("abuserzy");
	b.find("name").put_value("abuser");


	// missing level in inner repeat
	texcept(ctx.get("testek").render(rnd), webpp::stacked_exception, "output: required variable 'abuser.level' not found in render context");

	a.find("level").put_value(M_PI);
	b.find("level").put_value(M_PI_4);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></root>\n");

	// same as above, but repeat other then root's children
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\" xmlns:b=\"webpp://basic\" xmlns:c=\"webpp://control\">foo!<div c:repeat=\"inner\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><p f:data-level=\"#{abuser.level}\">abuser <b:render value=\"abuser.name\"/>, poziom <b:render value=\"abuser.level\" format=\"%.1f\"/></p></div>bar!</root>");
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root>foo!<div><p data-level=\"3.1415926535897931\">abuser asdf, poziom 3.1</p><p data-level=\"0.78539816339744828\">abuser abuser, poziom 0.8</p></div>bar!</root>\n");
}

// control - outer repeat
void t10() {
	tbegin("Test 10: render outer repeat");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// not possible repeat - outer on root element
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\" xmlns:c=\"webpp://control\" c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\"><p>abuser <b:render value=\"abuser.name\"/>, poziom <b:render value=\"abuser.level\" format=\"%.1f\"/></p></root>");
	texcept(ctx.get("testek").render(rnd).to_string(), webpp::stacked_exception, "outer repeat on root element is not possible");

	// empty array
	ctx.put("testek", "<root xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\" xmlns:c=\"webpp://control\" xmlns:f=\"webpp://format\"><foo/><div c:repeat=\"outer\" c:repeat-array=\"abuserzy\" c:repeat-variable=\"abuser\" f:data-level=\"dec(#{abuser.level|%03.4f})\"><p>abuser <b:render value=\"abuser.name\"/>, poziom <b:render value=\"abuser.level\" format=\"%.1f\"/></p></div><bar/></root>");
	tequal(ctx.get("testek").render(rnd).to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><bar/></root>\n");

	auto& a = rnd.add_to_array("abuserzy");
	a.find("name").put_value("asdf");


	auto& b = rnd.add_to_array("abuserzy");
	b.find("name").put_value("abuser");

	// missing variable inside repeat
	texcept(ctx.get("testek").render(rnd).to_string(), webpp::stacked_exception, "format: required variable 'abuser.level' not found in render context");

	a.find("level").put_value(M_PI);
	b.find("level").put_value(M_PI_4);
	tequal(ctx.get("testek").render(rnd).to_string(),"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><foo/><div data-level=\"dec(3.1416)\"><p>abuser asdf, poziom 3.1</p></div><div data-level=\"dec(0.7854)\"><p>abuser abuser, poziom 0.8</p></div><bar/></root>\n");
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
	return 0;
}

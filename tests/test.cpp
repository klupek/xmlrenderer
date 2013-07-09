
#include "../src/xmllib.hpp"
#include <cassert>

#define tbegin(name) std::cout << name << std::endl
#define tequal(a,b) if((a) != (b)) { std::cout << "\nFAIL: expected: " << b << "\nFAIL:   actual: " << a << std::endl; assert(false); }
#define ttrue(a) assert(a)
#define texcept(call, etype, emsg) try { (call); assert(false && "Expected exception " emsg ", but was not raised"); } catch(const etype& e) { tequal(e.what(), std::string(emsg)); }

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
	texcept(ctx.get(key).value().is_true(), std::runtime_error, "webpp::xml::render_value<i>::is_true(): '42' is not a boolean");
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
	ctx.add_to_array(key, "sot");
	ctx.add_to_array(key, "drajwer");
	
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
	texcept(ctx.get(key).value().is_true(), std::runtime_error, "webpp::xml::render_value<i>::is_true(): '42' is not a boolean");

	key = "users.asdf.abuser";
	ctx.lambda(key, []() { return true; });
	ttrue(ctx.get(key).value().is_true());
}

// test XML fragment rendering without using render context values
void t4() {
	tbegin("Test 4: XML fragment");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	webpp::xml::fragment f1("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>", ctx);
	tequal(f1.render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");


	webpp::xml::fragment f2("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>", ctx);

	tequal(f2.render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");
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
	texcept(ctx.get("testek").render(rnd), std::runtime_error, "webpp::xml::taglib::tag_render: <render> at line 1 requires value 'testval' in render context");

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
	texcept(ctx.get("testek4").render(rnd), std::runtime_error, "webpp::xml::taglib::tag_render: <render> at line 1: required=\"true|false\", not 'abuser'");

	ctx.put("testek4", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval3\" required=\"false\"/></b></rootnode>");
	tequal(ctx.get("testek4").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b/></rootnode>\n");
}

// context with taglib - format attribute namespace
void t7() {
	tbegin("Test 7: context with taglib - format attribute namespace");

	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	// syntax error
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"#{user.name\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), std::runtime_error, "#{ not terminated by }");

	// bad format
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"#{user.name|}\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), std::runtime_error, "empty format string");

	// missing variable
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:f=\"webpp://format\"><a f:href=\"/users/#{user.name}\" f:title=\"user #{user.name} - abuse level #{user.abuse|%.2f}\"/></rootnode>");
	texcept(ctx.get("testek").render(rnd), std::runtime_error, "output: required variable 'user.name' not found in render context");

	// missing second variable
	rnd.val("user.name", "asdf");
	texcept(ctx.get("testek").render(rnd), std::runtime_error, "format: required variable 'user.abuse' not found in render context");

	// everything set
	rnd.val("user.abuse", M_PI);
	tequal(ctx.get("testek").render(rnd).to_string(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><a href=\"/users/asdf\" title=\"user asdf - abuse level 3.14\"/></rootnode>\n");
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
	return 0;
}

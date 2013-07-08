
#include "../src/xmllib.hpp"
#include <cassert>

// test basic int and bool values
void t1() {
	std::cout << "Test 1: context render value\n";
	webpp::xml::render_context ctx;
	std::string key = "users.asdf.abuse";
	ctx.val(key, 42);
	assert(ctx.get("users").empty());
	assert(ctx.get("users.asdf").empty());
	assert(ctx.get("users..asdf.abuse").empty());
	assert(!ctx.get(key).empty() && ctx.get(key).value().output() == "42");	
	try {
		ctx.get(key).value().is_true();
		assert(false && "is_true should throw exception");
	} catch(const std::runtime_error& e) {
		assert(std::string("webpp::xml::render_value<i>::is_true(): '42' is not a boolean") == e.what());
	}

	key = "users.nolife.abuse";
	assert(ctx.get(key).empty());

	key = "users.asdf.abuser";
	ctx.val(key, true);
	assert(ctx.get(key).value().is_true());
}

// test arrays under keys
void t2() {
	std::cout << "Test 2: context render array\n";
	webpp::xml::render_context ctx;
	std::string key = "users.asdf.ofiary";
	ctx.add_to_array(key, "sot");
	ctx.add_to_array(key, "drajwer");
	
	assert(ctx.get("users").empty());
	assert(ctx.get("users.asdf").empty());
	assert(ctx.get("users..asdf.ofiary").empty());
	assert(!ctx.get(key).empty());
	auto i = ctx.get(key).begin();
	assert(i != ctx.get(key).end());
	std::cout << "asdf zjadl: " << i->value().output() << std::endl;
	assert(i->value().output() == "sot");	
	++i;
	assert(i != ctx.get(key).end());
	std::cout << "asdf zjadl: " << i->value().output() << std::endl;
	assert(i->value().output() == "drajwer");
	++i;
	assert(i == ctx.get(key).end());
}

// test lambdas returning bools or integers
void t3() {
	std::cout << "Test 3: lazy evaluated callback\n";

	webpp::xml::render_context ctx;
	std::string key = "users.asdf.abuse";

	ctx.lambda(key, []() {
		return 42;
	});

	assert(ctx.get(key).value().output() == "42");

	try {
		ctx.get(key).value().is_true();
		assert(false && "is_true should throw exception");
	} catch(const std::runtime_error& e) {
		assert(std::string("webpp::xml::render_value<i>::is_true(): '42' is not a boolean") == e.what());
	}

	key = "users.asdf.abuser";
	ctx.lambda(key, []() { return true; });
	assert(ctx.get(key).value().is_true());
}

// test XML fragment rendering without using render context values
void t4() {
	std::cout << "Test 4: XML fragment\n";
	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	webpp::xml::fragment f1("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>", ctx);
	assert(f1.render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");


	webpp::xml::fragment f2("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>", ctx);
	//std::cout << f2.render(rnd).to_string() << std::endl;

	assert(f2.render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");
}

// same as above, but use proper context loading
void t5() {
	std::cout << "Test 5: context basics\n";
	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\"></rootnode>");
	ctx.put("testek2", "<rootnode2 xmlns=\"webpp://xml\"><asdf/><foobar/></rootnode2>");


	assert(ctx.get("testek").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode/>\n");
	assert(ctx.get("testek2").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode2><asdf/><foobar/></rootnode2>\n");
}

#include "../src/taglib.hpp"

// context with taglib
void t6() {
	std::cout << "Test 6: context with taglib\n";
	webpp::xml::context ctx(".");
	webpp::xml::render_context rnd;
	ctx.load_taglib<webpp::xml::taglib::basic>();

	ctx.put("testek", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render value=\"testval\"/></b></rootnode>");
	// lets try it first without value
	try {
		ctx.get("testek").render(rnd);
		assert(false && "exception was expected");
	} catch(const std::runtime_error& e) {
		assert(e.what() == std::string("webpp::xml::taglib::tag_render: <render> at line 1 requires value 'testval' in render context"));
	}

	// then, with some value
	rnd.val("testval", "abuser<>");
	//std::cout << ctx.get("testek").render(rnd).to_string();
	assert(ctx.get("testek").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>abuser&lt;&gt;</b></rootnode>\n");

	// and some other value
	rnd.val("testval", 42);
	//std::cout << ctx.get("testek").render(rnd).to_string();
	assert(ctx.get("testek").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>42</b></rootnode>\n");


	// now, lets try formatting

	ctx.put("testek2", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval\"/></b></rootnode>");
	rnd.val("testval", 3.1415);
	//std::cout << ctx.get("testek2").render(rnd).to_string();
	assert(ctx.get("testek2").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>3.142</b></rootnode>\n");

	// default value

	ctx.put("testek3", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval2\" default=\"bezcenne\"/></b></rootnode>");
	//std::cout << ctx.get("testek3").render(rnd).to_string();
	assert(ctx.get("testek3").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>bezcenne</b></rootnode>\n");

	rnd.val("testval2", 12.34567);
	//std::cout << ctx.get("testek3").render(rnd).to_string();
	assert(ctx.get("testek3").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b>12.346</b></rootnode>\n");

	// required attribute
	ctx.put("testek4", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval2\" required=\"abuser\"/></b></rootnode>");
	try {
		ctx.get("testek4").render(rnd);
	} catch(const std::runtime_error& e) {
		assert(e.what() == std::string("webpp::xml::taglib::tag_render: <render> at line 1: required=\"true|false\", not 'abuser'"));
	}

	ctx.put("testek4", "<rootnode xmlns=\"webpp://xml\" xmlns:b=\"webpp://basic\"><b><b:render format=\"%.3f\" value=\"testval3\" required=\"false\"/></b></rootnode>");
	//std::cout << ctx.get("testek4").render(rnd).to_string();
	assert(ctx.get("testek4").render(rnd).to_string() == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rootnode><b/></rootnode>\n");
}



int main(int argc, char* argv[])
{
	t1();
	t2();
	t3();
	t4();
	t5();
	t6();
	return 0;
}

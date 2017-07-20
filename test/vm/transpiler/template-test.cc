#include <vm/transpiler/template.h>
#include <gtest/gtest.h>

#define STRINGIFY(...) #__VA_ARGS__

namespace vcl {
namespace vm  {
namespace transpiler {


TEST(Template,Basic) {
  {
    Template t;
    Template::Argument arg;

    arg["a"] = Template::Str("hello");
    arg["b.key"] = Template::Str("world");

    std::string output;

    ASSERT_TRUE( t.Render("${a} ${b.key}",arg,&output) );
    ASSERT_TRUE( output == "hello world" );
  }

  {
    Template t;
    Template::Argument arg;
    arg["a"] = Template::Str("hello world");
    std::string output;
    ASSERT_TRUE( t.Render("${ a} ${a } ${ a }",arg,&output) );
    ASSERT_TRUE( output == "hello world hello world hello world" );
  }
  {
    Template t;
    Template::Argument arg;
    arg["a"] = Template::Str("A");
    arg["b"] = Template::Str("B");

    std::string output;
    ASSERT_TRUE( t.Render("AABB ${a}${a}${b}${b}",arg,&output) );
    ASSERT_TRUE( output == "AABB AABB" );
  }

  {
    Template t;
    Template::Argument arg;
    arg["a"] = Template::Str("A");
    arg["b"] = Template::Str("B");

    std::string output;
    ASSERT_TRUE( t.Render("${a}${a}${b}${b} AABB",arg,&output) );
    ASSERT_TRUE( output == "AABB AABB" );
  }

  {
    Template t;
    Template::Argument arg;
    arg["a"] = Template::Str("A");
    arg["b"] = Template::Str("B");

    std::string output;
    ASSERT_TRUE( t.Render("AABB ${a}A${b}B AABB",arg,&output) );
    ASSERT_TRUE( output == "AABB AABB AABB" );
  }
}

} // namespace transpiler
} // namespace vm
} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

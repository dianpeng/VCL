#include <gtest/gtest.h>
#include <vm/transpiler/target-lua51.h>
#include <vm/runtime.h>
#include <vm/compiler.h>
#include <vm/vcl-pri.h>
#include <vm/compilation-unit.h>
#include <vm/parser.h>
#include <vm/procedure.h>
#include <boost/scoped_ptr.hpp>

#include <string>
#include <iostream>

#define STRINGIFY(...) #__VA_ARGS__

namespace vcl {
namespace vm  {
namespace transpiler {

namespace lua51 {
bool TranspileCode( const char* source , std::string* output ) {
  boost::shared_ptr<CompiledCode> cc( new CompiledCode( NULL ) );
  CompilationUnit cu;
  std::string error;
  SourceRepo source_repo;

  if(!source_repo.Initialize(":test",source,&error)) {
    std::cerr<<error;
    return false;
  }

  if(!CompilationUnit::Generate(&cu,
      cc.get(),
      &source_repo,
      100,
      std::string(),
      false,
      &error)) {
    std::cerr<<error;
    return false;
  }

  if(!Transpile("test","blabla",*cc,cu,Options(),output,&error))
  {
    std::cerr<<error;
    return false;
  }
  return true;
}

TEST(Lua51,Basic)
{
  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            global a = 10;
            global b = foo(10);
            global c = bar(10).c.d.e[10+2] -f;
            global d = a * b;
            global e = b * c;
            global f = f / d;
            global a = a + b;
            global c = 1 + 2 + f;
            import std;
            global e = std.foo(xxx);
            ),&output));
    std::cout<<output<<std::endl;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;

            import std;


            sub my_foo {
              declare x = 10;
              unset x;

              new y = std.foo;
              return {x+y};
            }
            ),&output));
    std::cout<<output;
  }
}

} // namespace lua51

}
}
}

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

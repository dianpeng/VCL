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
  SourceRepo source_repo(NULL,false,false);

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

              if(x+y>gg) return {x+y};
              elseif (x+y > ggg && (x != 2) || x == 3)
                return {x*y*2};
              else return {x*y};
            }

            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            import std;
            global x = a.b:f;
            global y = a.b:f.c:d();
            sub my_foo(a) {
              if(a == 0 || a == 1 || a == 2)
                return {a};
              else
                return {my_foo(a-1) + my_foo(a-2)};

              unset a.b.c[d]:e.f().g:h;
              unset a.b.c.d;
            }

            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            global a = {};
            global b = [];
            global c = [ 1,2,3,4,5 , true , false , null , "string" ];
            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            global a = {};
            global b = [];
            global c = [ 1,2,3,4,5 , true , false , null , "string" ];
            global d = '${a} ${b} ${c+d+e} fff';
            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            sub foo {
              new a = 10;
              new b = 20;
              call print(a+b);

              set a = "string";
              set b = "hh";
              call print(a+b);


              set a = "string";
              set b = 1;
              call print(a+b);
            }
            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;

            global object = {
              function : sub { return {10}; },
              foo      : sub(a,b) { return {a+b}; },
              bar      : sub(a) { return { if( a == 0 || a == 1 || a == 2 , a , object.bar(a-1) + object.bar(a-2))}; }
            };

            ),&output));
    std::cout<<output;
  }

  {
    std::string output;
    ASSERT_TRUE( TranspileCode( STRINGIFY(
            vcl 4.0;
            global x = a::b();
            global y = a.b.c.d.e::b();
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

#include <gtest/gtest.h>
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
namespace vm {

Context* CompileCode( const char* source ) {
  boost::shared_ptr<CompiledCode> cc( new CompiledCode( NULL ) );
  Context* context = new Context( ContextOption() , cc );
  CompilationUnit cu;
  std::string error;
  SourceRepo source_repo;

  std::cout<<"=====================================================\n";
  std::cout<<source<<'\n';
  std::cout<<"=====================================================\n";

  if(!source_repo.Initialize(":test",source,&error)) {
    std::cerr<<error;
    return NULL;
  }

  if(!CompilationUnit::Generate(&cu,
      cc.get(),
      &source_repo,
      100,
      std::string(),
      false,
      &error)) {
    delete context;
    std::cerr<<error;
    return NULL;
  }

  if(!Compile(cc.get(),source_repo.zone(),cu,&error)) {
    delete context;
    std::cerr<<error;
    return NULL;
  }
  return context;
}

bool TestCase( const char* source ) {
  Context* context = CompileCode(source);
  if(!context) return false;
  context->compiled_code()->Dump(std::cout);
  delete context;
  return true;
}

#define CC(...) ASSERT_TRUE(TestCase(#__VA_ARGS__))

TEST(Compiler,Arithmatic) {
  CC( vcl 4.0;
      global a = 0;
    );
  CC( vcl 4.0;
      global a = b;
    );
  CC( vcl 4.0;
      global a = b + 2 * c / d;
    );
  CC( vcl 4.0;
      global a = (b >= 10) && e;
    );
  CC( vcl 4.0;
      global a = (c >= f ) && d;
    );
  CC( vcl 4.0;
      global a = (c >= f ) && d && true || false && (e == f);
    );
  CC( vcl 4.0;
      global a = if(a,b,c);
      global c = if(a,ef,100);
    );
  CC( vcl 4.0;
      global a = if(a,b,c) + if(b,c,a);
      global d = if(a,if(b,c,a),d) + if(if(if(if(a,b,c),c,d),c,d),c,d);
    );
  CC( vcl 4.0;
      global a = b.c.d.e;
    );
  CC( vcl 4.0;
      global a = a[0].b.d:f;
    );
  CC( vcl 4.0;
      global a = -10;
      global b = -a;
      global c = !a;
    );
  CC( vcl 4.0;
      global a = foo();
      global b = foo(a,b,c);
      global d = foo(a)()(e).f:g[10]["a"];
    );
}

TEST(Compiler,ListAndDict) {
  CC( vcl 4.0;
      global a = [];
      global b = {};
    );

  CC( vcl 4.0;
      global a = [1,2,3,4,5,6];
      global b = { "string" : "value" ,
                   "key1" : gvar ,
                   "key2" : gvar2,
                   [key3] : true ,
                   [key4] : false
                 };
    );
}

TEST(Compiler,Global) {
  CC( vcl 4.0;
      import std;
    );
  CC( vcl 4.0;
      /* importion */
      import many;
      import b;
      import c;
      import a_lot_of;
    );
}

TEST(Compiler,Extension) {
  CC( vcl 4.0;
      import std;
      Backend backend {
        .host = 1;
        .port = "some_port";
        .cc = AnotherBackend {
          .UU = 1;
          .VV = 2;
          .CC = {};
          .DD = "string";
          .EE = true;
          .FF = false;
          .GG = null;
        };
      }
    );
  CC( vcl 4.0;
      import std;
      Backend my {} // Empty backend
    );
  CC( vcl 4.0;
      global value = Backend {
        .host = "www.google.com";
        .port = 12345;
      };
    );
}

TEST(Compiler,Sub) {
  CC( vcl 4.0;
      sub my_foo {}
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = 0;
        declare b = 10;
        return { a + b };
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        new a = 10;
        new b = foo.bar.goo();
        new c = "";
        return { a + b + c };
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = 100;

        set a = "string";
        set a += "hello";
        return { a };
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = "a string";
        set a -= "uu";
        set a = 100;
        set a *= 3;
        set a /= 4;
        set a %= 10;
        return {a};
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = {};
        set a.b += "UU";
        set a.b -= "UU";
        set a.b *= 3;
        set a.b /= 4;
        set a.f %= 10;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = [];
        set a[0] = 10;
        set a[100] += 10;
        set a[100] -= 10;
        set a[100] *= 10;
        set a[100] /= 10;
        set a[100] %= 100;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = {};
        set a:f-x = 10;
        set a:f-b -= "string";
        set a:f-c *= "string";
        set a:f-x /= "string";
        set a:f-x:f-x:f-x %= 1000;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = {};
        unset a;
        declare foo = 1000;
        unset foo;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        unset g1;
        unset g2;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        declare a = {};
        unset a[0];
        unset a.b;
        unset a:f-x;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        return (synth("MyString"));
        return (hash);
        return { 1 };
        return {{}};
        return;
        return { 100 };
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        call foo;
        call foo(1,2,3,4,5);
        foo(1,2,3,4,5);
        foo();
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        foo;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        foo();
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        call foo;
      }
    );

  CC( vcl 4.0;
      sub my_foo {
        if(a) return;
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        if(a) {
          set b = 10;
        } else {
          set b = 100;
        }
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        if(a) {
          set a[0] = 10;
        } else if (b) {
          set a[1] = 10;
        } else if (c == 100) {
          set a[2] = c;
        }
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        if(a) {
          set a = 100;
        } else if(a == 100) {
          set a = 1000;
        } else {
          set pp = true;
        }
      }
    );
  CC( vcl 4.0;
      sub my_foo {
        return { "A" "" "CDEF" "" "GGG" };
      }
    );

  // Sub with argument
  CC( vcl 4.0;
      sub my_foo() {}
    );

  CC( vcl 4.0;
      sub my_foo(a,b,c) {
        return { a + b + c };
      }
      sub my_foo(a,b,c) {
        return { a * b * c };
      }
    );
}

TEST(Compiler,For) {
  CC( vcl 4.0;
      sub my_foo() {
        declare a = [1,2,3,4,5];
        declare sum = 0;
        for( i : a ) {
          new a1 = i;
          new a2 = a1;
          new a3 = a2;
          new a4 = a3;
          set sum += i;
        }
        return {sum};
       }
    );
  CC( vcl 4.0;
      sub my_foo() {
        declare a = [];
        declare sum = 0;
        for( _ , v : a ) {
          if(v %2) set sum += v;
        }
        new b = [];
        set sum = 0;
        for( _ , v : b ) {
          set sum += v;
        }
       }
    );
  CC( vcl 4.0;
      global a = -2.0 / 2.0 == 1.0;
    );
}


} // namespace vm
} // namespace vcl


int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

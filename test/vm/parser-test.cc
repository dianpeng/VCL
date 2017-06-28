#include <vm/parser.h>
#include <gtest/gtest.h>

#define STRINGIFY(...) #__VA_ARGS__

namespace vcl {
namespace vm {

using namespace vcl::vm::zone;

static const bool kDebug = true;
static const bool kDumpError = true;

namespace test{
bool _test( const char* source ) {
  Zone zone(1);
  std::string src = (source);
  const std::string file_name = "test";
  std::string error;
  Parser parser(&zone,file_name,src,&error);
  ast::File* root = parser.DoParse();
  if(!root) {
    if(kDumpError) std::cerr<<error<<std::endl;
    return false;
  }
  if(kDebug) std::cout<<*root;
  return true;
}

bool no_for_parser( const char* source ) {
  Zone zone(1);
  std::string src = (source);
  const std::string file_name = "test";
  std::string error;
  Parser parser(&zone,file_name,src,&error,0,false);
  ast::File* root = parser.DoParse();
  if(!root) {
    if(kDumpError) std::cerr<<error<<std::endl;
    return false;
  }
  if(kDebug) std::cout<<*root;
  return true;
}
} // namespace test

#define positive(...) ASSERT_TRUE(test::_test(#__VA_ARGS__))
#define negative(...) ASSERT_FALSE(test::_test(#__VA_ARGS__))
#define no_for(...)   ASSERT_FALSE(test::no_for_parser(#__VA_ARGS__))

TEST(Parser,Basic) {
  positive( vcl 4.0;
    global a = 0;
    );

  positive(
      vcl 4.0;
      global a = a;
      );

  positive(
      vcl 4.0;
      include "file";
      );

  positive(
      vcl 4.0;
      );

  positive(
      vcl 4.0;
      import amodule;
      );

  positive(
      vcl 4.0;
      sub foo {}
      );
  positive(
      vcl 4.0;
      sub foo() {}
      );
  positive(
      vcl 4.0;
      sub foo(a,b,c,d,e,f) {}
      );
  positive( vcl 4.0; backend a {}; );
  negative();
  negative( vcl 4.0 );
  negative( vcl 4.0; include "file" );
  negative( vcl 4.0; sub {} );
  negative( vcl 4.0; sub foo {}; );
  negative( vcl 4.0; import a );
  negative( vcl 4.0; import "a" );
  negative( vcl 4.0; global a = 0 );
  negative (vcl 4.0; call foo(); );
  negative( vcl 4.0; global x = global; );
  // Extension
  positive( vcl 4.0;
      ext1 my_ext {}
      );

  positive( vcl 4.0;
      ext my_ext {
        .field1 = [];
        .field2 = {};
        }
      );
  positive( vcl 4.0;
      ext my_ext {
        .field1 = [];
        .field2 = u {
          .field3 = backend {
            .f = [[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]];
            };
          };
        }
       );
}

TEST(Parser,Expression) {
  positive( vcl 4.0;
      global a = 1; );
  positive( vcl 4.0;
      global a = 1 + 2;
      );
  positive( vcl 4.0;
      global a = 1 + 2 * 3 /4 - 5 % 6;
      );
  positive( vcl 4.0;
      global a = a >= 10;
      );
  positive( vcl 4.0;
      global a = a > 10;
      );
  positive( vcl 4.0;
      global a = a > 10 && b < 10 || c ==10 && u ~ 20;
      );
  positive( vcl 4.0;
      global a = a > 10 || true && false;
      global b = a == 10 && null;
      global c = c != 10 || c !~ "string";
      );
  positive( vcl 4.0;
      global a = true && false;
      global b = true || false && c;
      Backend my_foo {}
      );
  positive( vcl 4.0;
      global a = if(a,b,c);
      global e = if(a,c,d) + if(a,if(a,b,c),e) + "string" + 1000;
      );
  positive( vcl 4.0;
      global a = if(a,c,e) + 100 / foo() % _;
      global c = if(if(if(if(if(if(a,b,c),1,2),2,3),3,4),4,5),6,7) + 10;
      );
  positive( vcl 4.0;
      global a = _._._._._._._._._._._."string"[10000][a+b+c+d+e*10]().U-X;
      global b = _()._()._()._()._____().______.X-UF-A;
      );

  positive( vcl 4.0;
      global a = 1 + 2 * a - [] + "string" / foo();
      global b = a % 10 ;
      global c = if(a,b,c) + if(b,c,d);
      global e = 1 + 2 * 100 * (a + b + c);
      global str = "string" " " "world" "eeee" + "another";
      );

  positive( vcl 4.0;
      global a = a.b;
      global a = a[1];
      global a = a."string";
      global a = a.X-Header;
      global a = a.'string';
      global a = a[1]["string"]();
      global b = a()()()()()()().ab.c.e.f.d.g.h.h[1][2][3][4][5][6];
      global b = a.'string'.'another'."another"[1][2]().c().e(10,2,3,4,5);
      global b = a:X-F-4:key-value:v.'true'.f().'false';
      );
  positive( vcl 4.0;
      global a = [1,2,3,4,5];
      global a = [];
      global b = [a,2,[1,23,[]],[[[[]]]],type {.host=1 ; .value = 2; .serialize = 3; .arr = [1,2,3,4,s]; },{}];
      );
  positive( vcl 4.0;
      global a = w {
        .a = 1,
        .b = 2,
        .c = 3,
        .d = 4,
        };
      global b = MyObject {
        .a = backend {
           .f1 = 10;
           .f2 = 20;
         },
        .c = 100,
        .d = [],
      };
      global empty = u {};

      Backend a {
        .hostname = "sssd";
      }
      );
  positive( vcl 4.0;
      global a = {
        "u" : "v",
        "v" : "w",
        [va]  : expression,
        [1+2+3] : [1,2,3,4,5,6,7,98],
        "quoa": backend { .host = 1; .host_ip = 2; }
        };
      global b = {};
      global c = { [{}] : { [{}] : { [{}] : { [{}] : 1 } } } , [2] : [[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]] };
      );

  //////////////////////////////////////////////
  // ALL NEGATIVE CASES
  //////////////////////////////////////////////

  negative( vcl 4.0;
      global a = 1
      );

  negative( vcl 4.0;
      global a = b;
      global b = a.b.c.
      );

  negative( vcl 4.0;
      global c = c.3;
      );

  negative( vcl 4.0;
      global a = c.;
      );

  negative( vcl 4.0;
      global a = c.d[e].x[1].;
      );
  negative( vcl 4.0;
      global a = foo(1,);
      );
  negative( vcl 4.0;
      global a = foo(100,);
      );
  negative( vcl 4.0;
      global a = foo().a.c.b.c.d.e.f;
      foo();
      bar().a.d.f."string"()()()()()().x;
      );
  negative( vcl 4.0;
      global x = 1.string;
      global x = a[-1].[-2];
      global x = _[1].();
      );
  negative( vcl 4.0;
      global x = .1.2.3.4._.5;
      );
  negative( vcl 4.0;
      global x = foo((()));
      );
  negative( vcl 4.0;
      global x = a + (x+5) * _ *1 / a;
      global x = y % _ + _
      );
  negative( vcl 4.0;
      global x = a + _.x.X-U.F.V-----;
      global x = ( a >= 10 && b ~= && u);
      );
  negative( vcl 4.0;
      global x = ( a ~ [] );
      global b = ( a !~ [] );
      global b = x + b + {} + _.x()._1.true
      );
  negative( vcl 4.0;
      global x = {
        a : 10,
        b : c ,
        "string" : 100 ,
        "vv" : [],
        ___  : 10 + 200 * / a(),
        };
      global x = [
        { "A" : [] } ,
        { "B" : 1234 },
        { "C" : xx },
        ];
      );
}

TEST(Parser,Statement) {
  positive( vcl 4.0;
      sub my_foo {
        if(a) {
          set bb = true;
        } else if(b) {
          set cc = false;
        }
        }
      );
  positive( vcl 4.0;
      sub my_foo {
        if(a) {
          set bb = true;
        }
      }
    );
  positive( vcl 4.0;
      sub my_foo {
        if(a)
          if(b)
            if(c)
              if(d)
                if(e)
                  if(f)
                    set ss.xx.ff.uu += true + "string";
       }
     );
  positive( vcl 4.0;
      sub my_foo {
        if(a)
          if(b)
            if(c) {
              set xx.ss.ff[10] = "true";
              unset xx.ss.ff;
            }
        }
      sub my_foo2 {
        if(a) set xx = "string";
        else set uu = "xx" + "vv" + [];
      }
      );
  positive( vcl 4.0;
      sub foo {
        if(a) set x = 100;
        elif(a == 100) set y =true;
        elif(a == 101) set z = false;
        elif(a == 102 && b == 303 || x ~ []) set u = 101;
        else {
          return {a+b+c+d+e+f};
        }
      }
      );
  positive( vcl 4.0;
      sub handle_uri_clean {
        if(a) set x = 100;
        elif(a && b > 100 || b < 20) set x.u.v = xal;
        elif(a && u ~ [] && c !~ {}) set u.v.x = ppa;
        set a = 10;
        unset a;
        call foo;
        call foo(10,200,a.b.c.d.e());
        return {{}};
        return (ok);
        return (pipe);
        return (lookup);
        return (a.b.c.d.e.f[0]());
      }
    );
  positive( vcl 4.0;
      sub bar() {
        declare a = null;
        declare b;
        if( a == b ) {
          call print( "Here" );
        } else if(a != b) {
          return {1000};
        }
      }
    );
}

TEST(Parser,For) {
  positive( vcl 4.0;
      sub foo {
      for( i , _ : g ) {
        break;
        continue;
      }
      for( _ , i : x ) {
        continue;
      }
      for( _ , _ : g + 100 / 2 * aaa ) {
        continue;
      }
      }
      );
  positive( vcl 4.0;
      sub foo {
        for( i : a )
          for( i : b )
            for( i : c )
              for( i : d )
                for( i : e )
                  if( i == 100 ) break;
      }
      );
}

TEST(Parser,ForUnsport) {
  no_for(vcl 4.0;
         sub foo {
           for( i : a ) call print("Hehe");
         }
      );
  no_for( vcl 4.0;
          sub foo { break; }
        );
  no_for( vcl 4.0;
          sub foo { continue; }
        );
}

} // namespace vm
} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>
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
namespace vm {

Context* CompileCode( const char* source ) {
  boost::shared_ptr<CompiledCode> cc( new CompiledCode( NULL ) );
  Context* context = new Context( ContextOption() , cc );
  CompilationUnit cu;
  std::string error;
  SourceRepo source_repo;

  if(!source_repo.Initialize(":test",source,&error)) {
    delete context;
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

// Helper primitives to check the specific value and other stuff
#define XX(TYPE,NAME) \
  bool GVar##NAME( Context* context , const std::string& name , TYPE value ) { \
    Value v; \
    if(!(context->GetGlobalVariable(name,&v))) return false; \
    if(!v.Is##NAME()) { std::cout<<"Type:"<<v.type_name()<<std::endl; return false; } \
    if(v.Get##NAME() == value) return true; \
    std::cout<<"Expect:"<<value<<"|Actual:"<<v.Get##NAME()<<std::endl; \
    return false; \
  }

XX(int64_t,Integer)
XX(double,Real)
XX(bool,Boolean)
XX(const vcl::util::Size&,Size)
XX(const vcl::util::Duration&,Duration)

bool GVarString( Context* context , const std::string& name , const std::string& value ) {
  Value v;
  if(!(context->GetGlobalVariable(name,&v))) return false;
  if(!v.IsString()) return false;
  if(*v.GetString() == value) return true;
  std::cout<<"Expect:"<<value<<"|Actual:"<<v.GetString()->data()<<std::endl;
  return false;
}
#undef XX

template< size_t N >
bool GVarList( Context* context , const std::string& name ,
    Value (&arr)[N] ) {
  Value v;
  if(!(context->GetGlobalVariable(name,&v))) return false;
  if(!v.IsList()) return false;
  List* l = v.GetList();
  if(l->size() != N) return false;
  for( size_t i = 0 ; i < l->size() ; ++i ) {
    bool result;
    if(!((*l)[i].Equal(context,arr[i],&result))) return false;
    if(!result) return false;
  }
  return true;
}

template< size_t N >
bool GVarMap( Context* context , const std::string& name ,
    std::pair<std::string,Value> (&arr)[N] ) {
  Value v;
  if(!context->GetGlobalVariable(name,&v)) return false;
  if(!v.IsDict()) return false;
  Dict* d = v.GetDict();
  for( size_t i = 0 ; i < N ; ++i ) {
    bool result;
    Value val;
    std::pair<std::string,Value>& ele = arr[i];
    if(!d->Find(ele.first,&val)) return false;
    if(!val.Equal(context,ele.second,&result)) return false;
    if(!result) return false;
  }
  if( d->size() != N ) return false;
  return true;
}

MethodStatus CallFunc( Context* context , const char* name , Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , output );
}

MethodStatus CallFunc( Context* context, const char* name , const Value& a1 ,
    Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , a1 , output );
}

MethodStatus CallFunc( Context* context , const char* name , const Value& a1 ,
    const Value& a2 , Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , a1 , a2 , output );
}

#define CTX(context) \
  do { \
    ASSERT_TRUE(context.get()); \
    MethodStatus result = context->Construct(); \
    if(result.is_fail()) { \
      std::cerr<<result.fail()<<std::endl; \
      ASSERT_TRUE(0); \
    } else if(result.is_unimplemented()) { \
      std::cerr<<"not implemented"<<std::endl; \
      ASSERT_TRUE(0); \
    } \
  } while(false)


#define GVAR(TYPE,NAME,VALUE) ASSERT_TRUE(GVar##TYPE(context.get(),NAME,VALUE))

#define GLIST(NAME,...) \
  do { \
    Value temp[] = __VA_ARGS__; \
    ASSERT_TRUE(GVarList(context.get(),NAME,temp)); \
  } while(false)

#define GMAP(NAME,...) \
  do { \
    std::pair<std::string,Value> temp[] = __VA_ARGS__; \
    ASSERT_TRUE(GVarMap(context.get(),NAME,temp)); \
  } while(false)

TEST(VM,Expression1) {
  {
    boost::scoped_ptr<Context> context(CompileCode( STRINGIFY(
          vcl 4.0;
          global a = 10;
          global b = a * 100;
          global c = a + 2 * 1000;
          )
        ));
    ASSERT_TRUE(context.get());
    ASSERT_TRUE(context->Construct());
    GVAR(Integer,"a",10);
    GVAR(Integer,"b",1000);
    GVAR(Integer,"c",2010);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode( STRINGIFY(
          vcl 4.0;
          global a = 10;
          global b = 10 + 2 * 3 - a;
          global c = a / 5 + 1;
          global d = a % 100;
          global e = 1 + 1.0;
          global f = 1 - 1.0;
          global h = f + 2.0 / 1.0 * 3.0 - 1.0000;
          )
        ));
    ASSERT_TRUE(context.get());
    ASSERT_TRUE(context->Construct());
    GVAR(Integer,"a",10);
    GVAR(Integer,"b",6);
    GVAR(Integer,"c",3);
    GVAR(Integer,"d",10);
    GVAR(Real,"e",2.0);
    GVAR(Real,"f",0.0);
    GVAR(Real,"h",5.0);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode( STRINGIFY(
          vcl 4.0;
          global a = true;
          global b = false;
          global c = a + b;
          global d = c * 1.0;
          global e = null;
          global r1 = a * 1;
          global r2 = a / 1;

          global b1 = a * 1.0;
          global b2 = a * 1;
          global b3 = a / 1.0;
          global b4 = a / 1;
          global b5 = a * b;
          global b6 = a - b;
          global b7 = b / a;
          global b8 = (b+1) % (a+1);
          )
        ));
    ASSERT_TRUE(context.get());
    ASSERT_TRUE(context->Construct());
    GVAR(Boolean,"a",true);
    GVAR(Boolean,"b",false);
    GVAR(Integer,"c",1);
    GVAR(Real,"d",1.0);

    GVAR(Real,"b1",1.0);
    GVAR(Integer,"b2",1);
    GVAR(Real,"b3",1.0);
    GVAR(Integer,"b4",1);
    GVAR(Integer,"b5",0);
    GVAR(Integer,"b6",1);
    GVAR(Integer,"b7",0);
    GVAR(Integer,"b8",1);

    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("e",&v) );
      ASSERT_TRUE( v.IsNull() );
    }
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
          vcl 4.0;
          global config_a = 10;
          global config_b = "string";
          global config_c = config_b + " world";
          global s = "";
          )));
    ASSERT_TRUE(context.get());
    ASSERT_TRUE(context->Construct());
    GVAR(String,"config_b","string");
    GVAR(String,"config_c","string world");
    GVAR(Integer,"config_a",10);
    GVAR(String,"s","");
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = 10;
            global c1 = a > 10;
            global c2 = a < 10;
            global c3 = a == 10;
            global c4 = a >= 10;
            global c5 = a >= 9;
            global c6 = a <= 9;
            global c7 = a <= 10;
            global c8 = a != 10;
            )));
    ASSERT_TRUE(context && context->Construct());
    GVAR(Integer,"a",10);
    GVAR(Boolean,"c1",false);
    GVAR(Boolean,"c2",false);
    GVAR(Boolean,"c3",true);
    GVAR(Boolean,"c4",true);
    GVAR(Boolean,"c5",true);
    GVAR(Boolean,"c6",false);
    GVAR(Boolean,"c7",true);
    GVAR(Boolean,"c8",false);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = "string";
            global c1 = a == "string";
            global c2 = a != "string";
            global c3 = a > "string";
            global c4 = a < "string";
            global c5 = a >= "string";
            global c6 = a <= "string";
            )));
    ASSERT_TRUE( context && context->Construct());
    GVAR(String,"a","string");
    GVAR(Boolean,"c1",true);
    GVAR(Boolean,"c2",false);
    GVAR(Boolean,"c3",false);
    GVAR(Boolean,"c4",false);
    GVAR(Boolean,"c5",true);
    GVAR(Boolean,"c6",true);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = true;
            global b = false;
            global c1 = a || b;
            global c2 = a && b;
            global c3 = b || a && true && 100;
            )));
    ASSERT_TRUE(context && context->Construct());
    GVAR(Boolean,"a",true);
    GVAR(Boolean,"b",false);
    GVAR(Boolean,"c1",true);
    GVAR(Boolean,"c2",false);
    GVAR(Boolean,"c3",true);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = 10;
            global b = -a;
            global c = -100;
            global d = !true;
            global f = !false;
            global g = ------10;
            global x = ++++++10;
            global ef= --1.0;
            global ee= !2.0;
            global eh =!0.0;
            global eg = !1;
            global em = !0;
            )));
    CTX(context);
    GVAR(Integer,"a",10);
    GVAR(Integer,"b",-10);
    GVAR(Integer,"c",-100);
    GVAR(Boolean,"d",false);
    GVAR(Boolean,"f",true);
    GVAR(Integer,"g",10);
    GVAR(Integer,"x",10);
    GVAR(Real,"ef",1.0);
    GVAR(Boolean,"ee",false);
    GVAR(Boolean,"eh",true);
    GVAR(Boolean,"eg",false);
    GVAR(Boolean,"em",true);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = 10;
            global b = true;
            global c = -1;
            global d = if(b,a,c);
            global e = if(d,true,false);
            global f = if(e,true,false);
            global r1 = if(if(if(if(if(true,false,true),false,true),false,true),true,false),false,true);
            )));
    CTX(context);
    GVAR(Integer,"d",10);
    GVAR(Boolean,"e",true);
    GVAR(Boolean,"f",true);
    GVAR(Boolean,"r1",true);
  }
}

TEST(VM,List) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = [];
            global b = [1];
            global c = [1,2];
            global d = [1.0,2];
            global e = ["string",1.0,true,false,null];
            global n1 = 1;
            global n2 = 1.0;
            global n3 = true;
            global n4 = "xx";
            global n5 = [n1,n2,n3,n4];
            global aa = 10;
            global bb = 20;
            global cc = [aa,bb];
            )));
    CTX(context);
    { // Check empty array which is not allowed
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("a",&v) );
      ASSERT_TRUE( v.IsList() );
      ASSERT_TRUE( v.GetList()->empty() );
    }
    GLIST("b",{ Value(1) });
    GLIST("c",{ Value(1) , Value(2) });
    GLIST("d",{ Value(1.0), Value(2) });
    GLIST("e",{ Value( context->gc()->NewString("string") ) , Value(1.0) , Value(true) , Value(false) , Value() });
    GLIST("n5",{Value(1),Value(1.0),Value(true),Value(context->gc()->NewString("xx"))});
    GLIST("cc",{Value(10),Value(20)});
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = [[]];
            global b = [a,[]];
            )));
    CTX(context);
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("a",&v) );
      ASSERT_TRUE( v.IsList() );
      ASSERT_TRUE( v.GetList()->Index(0).IsList() );
      ASSERT_TRUE( v.GetList()->Index(0).GetList()->empty() );
    }
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("b",&v) );
      ASSERT_TRUE( v.IsList() );
      ASSERT_TRUE( v.GetList()->Index(0).IsList() );
      ASSERT_TRUE( v.GetList()->Index(1).IsList() );
      ASSERT_TRUE( v.GetList()->Index(1).GetList()->empty() );
    }
  }
}

TEST(VM,Dict) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global l1 = 10;
            global l2 = 20;
            global l3 = "string";

            global a = {};
            global b = { "a" : "b" };
            global c = {
              "a" : 100,
              "b" : true,
              "c" : false,
              "d" : null
            };
            global d = {
              "a" : 1 + 2 * 3 ,
              "b" : true != false ,
              "c" : l1 ,
              "d" : l2 ,
              "e" : l3 + " w"
            };
            global e = {
              ["str"] : 1 ,
              [l3] : 2 ,
              [l3 + " world"] : 3
            };
            )));
    CTX(context);
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("a",&v) );
      ASSERT_TRUE( v.IsDict() );
      ASSERT_TRUE( v.GetDict()->empty() );
    }
    GMAP("b",{{std::string("a"),Value( context->gc()->NewString("b"))}});
    GMAP("c",{
               {std::string("a"),Value(100)},
               {std::string("b"),Value(true)},
               {std::string("c"),Value(false)},
               {std::string("d"),Value()}
             });
    GMAP("d",{
              {std::string("a"),Value(7)},
              {std::string("b"),Value(true)},
              {std::string("c"),Value(10)},
              {std::string("d"),Value(20)},
              {std::string("e"),Value(context->gc()->NewString("string w"))}
             });
    GMAP("e",{
              {std::string("str"),Value(1)},
              {std::string("string"),Value(2)},
              {std::string("string world"),Value(3)}
              });
  }
}


TEST(VM,PrefixExpr) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global list = [1,2,3,4,5,"string",true,false,null];
            global a = list[0]; /* 1 */
            global b = list[4]; /* 5 */
            global c = list[5]; /* stirng */
            global d = list[6]; /* true */
            global e = list[7]; /* false */
            global f = list[8]; /* null */
            )));
    CTX(context);
    GVAR(Integer,"a",1);
    GVAR(Integer,"b",5);
    GVAR(String,"c","string");
    GVAR(Boolean,"d",true);
    GVAR(Boolean,"e",false);
    {
      Value v;
      ASSERT_TRUE(context->GetGlobalVariable("f",&v));
      ASSERT_TRUE(v.IsNull());
    }
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global list = ["string",[1,2,4,5]];
            global a = list[0];
            global b = list[1];
            global c = b[0];
            global d = b[1];
            global e = b[2];
            global f = b[3];
            )));
    CTX(context);
    GVAR(String,"a","string");
    GVAR(Integer,"c",1);
    GVAR(Integer,"d",2);
    GVAR(Integer,"e",4);
    GVAR(Integer,"f",5);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global map = {
              "a" : "string",
              "b" : 1,
              "c" : true,
              "d" : false,
              "e" : [ 1,2,3,4 ],
              "f" : {},
              "g" : null
            };
            global a = map.a;
            global b = map.b;
            global c = map.c;
            global d = map.d;
            global e = map.e;
            global f = map.f;
            global g = map.g;
            )));
    CTX(context);
    GVAR(String,"a","string");
    GVAR(Integer,"b",1);
    GVAR(Boolean,"c",true);
    GVAR(Boolean,"d",false);
    GLIST("e",{ Value(1),Value(2),Value(3),Value(4) });
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("f",&v) );
      ASSERT_TRUE( v.IsDict() );
      ASSERT_TRUE( v.GetDict()->empty() );
    }
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("g",&v));
      ASSERT_TRUE( v.IsNull() );
    }
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global map = {
              "a" : "b",
              "c" : "d",
              "e" : 2  ,
              "f" : true,
              "e-x":"g-f"
            };
            global a = map:a;
            global c = map:c;
            global e = map:e;
            global f = map:f;
            global othre = map:e-x;
            )));
    CTX(context);
    GVAR(String,"a","b");
    GVAR(String,"c","d");
    GVAR(Integer,"e",2);
    GVAR(Boolean,"f",true);
    GVAR(String,"othre","g-f");
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global map = {
              "a": { "b" : { "c" : { "d" : { "e" : "f" }}}}
              };
            global struct = {
              "a" : [ { "b" : [ { "c" : { "d" : [ { "e" : true }] }} ] }]
            };
            global result = map.a.b.c.d.e;
            global r2 = struct.a[0].b[0].c.d[0].e;
            )));
    CTX(context);
    GVAR(String,"result","f");
    GVAR(Boolean,"r2",true);
  }

  // Expression as key
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global key1 = "string1";
            global key2 = "string2";
            global key3 = "string3";
            global c = {
              [key1] : "a",
              [key2] : true,
              [key3] : 100,
              [key3 + "hello"] : "world"
            };
            global r1 = c."string1";
            global r2 = c.string2;
            global r3 = c["string3"];
            global r4 = c[key3 + "hello"];
            )));
    CTX(context);
    GVAR(String,"r1","a");
    GVAR(Boolean,"r2",true);
    GVAR(Integer,"r3",100);
    GVAR(String,"r4","world");
  }

  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global value = "hello";
            global map = {
              "a" : 1 + 2 * 3 ,
              "b" : value + "world",
              "c" : "nothing"
            };
            global r1 = map.a;
            global r2 = map.b;
            global r3 = map.c;
            )));
    CTX(context);
    GVAR(Integer,"r1",7);
    GVAR(String,"r2","helloworld");
    GVAR(String,"r3","nothing");
  }
}

TEST(VM,Expression2) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global map = {
              "a" : 1 ,
              "b" : 2 ,
              "c" : 3 ,
              "d" : 4
            };
            global a = 10 + map.a * 3 - 20;
            global b = 20 * map.b;
            global c = 30 / map.c;
            global d = 1.0 + map.d;
            )));
    CTX(context);
    GVAR(Integer,"a",-7);
    GVAR(Integer,"b",40);
    GVAR(Integer,"c",10);
    GVAR(Real,"d",5.0);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global list = [1,2,3,4,5,6,7,8];
            global map = { "a" : 1 , "b" : true , "c" : 2.0 , "d" : "string" };
            global r = list[0] + map.a + list[6] + map.b;
            )));
    CTX(context);
    GVAR(Integer,"r",10);
  }
}

// ======================================================================
// Extension
// ======================================================================
class MyExtension : public Extension {
 public:
  MyExtension() : Extension( "MyExtension" ) {}

  virtual MethodStatus SetProperty(Context* context , const String& key,
      const Value& value ) {
    if( key == "a" ) {
      if(!value.IsInteger()) {
        return MethodStatus::NewFail("MyExtension attribute:a expect integer,but got %s",
            value.type_name());
      }
      m_attribute_a = value.GetInteger();
    } else if( key == "b" ) {
      if(!value.IsString()) {
        return MethodStatus::NewFail("MyExtension attribute:b expect string,but got %s",
            value.type_name());
      }
      m_attribute_b = value.GetString()->ToStdString();
    } else if( key == "c" ) {
      if(!value.IsReal()) {
        return MethodStatus::NewFail("MyExtension attribute:c expect real,but got %s",
            value.type_name());
      }
      m_attribute_c = value.GetReal();
    } else {
      return MethodStatus::NewFail("MyExtension attribute:%s doesn't exist!",
          key.data());
    }
    return MethodStatus::kOk;
  }

  virtual MethodStatus UpdateProperty(Context* context , const String& key ,
      const Value& value ) {
    return SetProperty(context,key,value);
  }

  virtual MethodStatus GetProperty(Context* context , const String& key ,
      Value* output ) const {
    if( key == "a" ) {
      output->SetInteger(m_attribute_a);
    } else if( key == "b" ) {
      output->SetString( context->gc()->NewString(m_attribute_b) );
    } else if( key == "c" ) {
      output->SetReal( m_attribute_c );
    } else {
      return MethodStatus::NewFail("MyExtension attribute:% doesn't exist!",
          key.data());
    }
    return MethodStatus::kOk;
  }

 private:
  int m_attribute_a;
  std::string m_attribute_b;
  double m_attribute_c;
};

class MyExtensionFactory : public ExtensionFactory {
 public:
  virtual Extension* NewExtension( Context* context ) {
    return context->gc()->New<MyExtension>();
  }
};

TEST(VM,Extension) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            MyExtension my_ext {
              .a = 10 ;
              .b = "hello world";
              .c = 20.0;
            }
            global a = my_ext.a;
            global b = my_ext.b;
            global c = my_ext.c;
            )));
    ASSERT_TRUE( context->RegisterExtensionFactory("MyExtension",
          new MyExtensionFactory()) );
    CTX(context);
    GVAR(Integer,"a",10);
    GVAR(String,"b","hello world");
    GVAR(Real,"c",20.0);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global my_ext = MyExtension {
              .a = 10;
              .b = "hello world";
              .c = 20.0;
            };
            global a = my_ext.a;
            global b = my_ext.b;
            global c = my_ext.c;
            MyExtension my_ext2 {
              .a = 10;
              .b = "string";
              .c = 30.0;
            };
            )));
    ASSERT_TRUE( context->RegisterExtensionFactory("MyExtension",
          new MyExtensionFactory()) );
    CTX(context);
    GVAR(Integer,"a",10);
    GVAR(String,"b","hello world");
    GVAR(Real,"c",20.0);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            MyExtension my_ext {
              .ccc = "ccc";
            }
            global a = my_ext.ccc; /* will not execute */
            )));
    ASSERT_TRUE( context->RegisterExtensionFactory("MyExtension",
          new MyExtensionFactory()) );
    MethodStatus result = context->Construct();
    ASSERT_TRUE( result.is_fail() );
    std::cerr << result.fail();
  }
}


// ======================================================================
// Extend the runtime with new C++ functions
// ======================================================================
class Add : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    int64_t sum = 0;

    // Variable length
    for( size_t i = 0 ; i < context->GetArgumentSize() ; ++i ) {
      Value arg = context->GetArgument(i);
      if(arg.IsInteger()) {
        sum += arg.GetInteger();
      } else {
        return MethodStatus::NewFail("function::Add's %zu argument is type %s,"
            "but expect a integer",i+1,arg.type_name());
      }
    }

    output->SetInteger(sum);
    return MethodStatus::kOk;
  }

  Add():Function("Add") {}
};

class ToDouble : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) return MethodStatus::NewFail("function::ToDouble "
        "expect 1 argument!");

    double real;
    if(context->GetArgument(0).ToReal(context,&real)) {
      output->SetReal(real);
      return MethodStatus::kOk;
    } else {
      return MethodStatus::NewFail("function::ToDouble cannot convert type %s",
          context->GetArgument(0).type_name());
    }
  }

  ToDouble():Function("ToDouble"){}
};

class Print : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    for( size_t i = 0 ; i < context->GetArgumentSize() ; ++i ) {
      if(!context->GetArgument(i).ToDisplay(context,&std::cerr))
        return MethodStatus::NewFail("function::Print cannot print out type %s",
            context->GetArgument(i).type_name());
      std::cerr<<'\n';
    }
    output->SetNull();
    return MethodStatus::kOk;
  }
  Print():Function("Print"){}
};

// Negative return functions
class Negative : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    VCL_UNUSED(context);
    VCL_UNUSED(output);
    return MethodStatus::NewFail("Just fail it!");
  }
  Negative():Function("Negative"){}
};

class UnimplementedFunc: public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    VCL_UNUSED(context);
    VCL_UNUSED(output);
    return MethodStatus::NewUnimplemented("Not implemented");
  }
  UnimplementedFunc():Function("UnimplementedFunc"){}
};

class TerminateFunc : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    VCL_UNUSED(context);
    output->SetNull();
    return MethodStatus::kTerminate;
  }
  TerminateFunc():Function("TerminateFunc"){}
};

// Wired functions
TEST(VM,Function) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = Add(1,2,3,4,5,6,7,Add(8,9));
            global b = (a == (1+2+3+4+5+6+7+8+9));
            )));
    context->AddOrUpdateGlobalVariable("Add", Value( context->gc()->New<Add>() ) );
    CTX(context);
    GVAR(Boolean,"b",true);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = Negative();
            global b = c + d; /* will not execute */
            )));
    context->AddOrUpdateGlobalVariable("Negative",Value( context->gc()->New<Negative>()));
    MethodStatus result = context->Construct();
    ASSERT_TRUE( result.is_fail() );
    std::cerr<<result.fail();
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = UnimplementedFunc();
            global b = c + d; /* will not execute */
            )));
    context->AddOrUpdateGlobalVariable("UnimplementedFunc",Value( context->gc()->New<UnimplementedFunc>()));
    MethodStatus result = context->Construct();
    ASSERT_TRUE( result.is_fail() );
    std::cerr<<result.fail();
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = TerminateFunc();
            global b = c + d; /* will not execute */
            )));
    context->AddOrUpdateGlobalVariable("TerminateFunc",Value( context->gc()->New<TerminateFunc>()));
    MethodStatus result = context->Construct();
    ASSERT_TRUE( result.is_terminate() );
  }
}

// Modules
TEST(VM,Module) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            import test;
            global a = test.Add(1,2,3,4,5,6,7,8);
            global b = test.Add;
            global c = b(1,2);
            )));
    Module* module = context->AddModule("test");
    module->AddProperty(*context->gc()->NewString("Add"),
                        Value( context->gc()->New<Add>()));
    CTX(context);
    GVAR(Integer,"a",1+2+3+4+5+6+7+8);
    GVAR(Integer,"c",3);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            import test;
            global ladd = test.Add;
            global b = ladd(100,200);
            global lprint = test.Print;
            global c = lprint(1,2,3,4,5);
            global d = {};
            global e = { "a" : true , "b" : null };
            global ll= [1,2,3,4,true,false,null,"string",1.0];
            global f = lprint(e,d,ll);
            )));
    Module* module = context->AddModule("test");
    module->AddProperty(*context->gc()->NewString("Add"),
        Value(context->gc()->New<Add>()));
    module->AddProperty(*context->gc()->NewString("Print"),
        Value(context->gc()->New<Print>()));
    CTX(context);
    GVAR(Integer,"b",300);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            import test;
            global r1 = test.DEBUG;
            global r2 = test.IsProduction;
            )));
    Module* module = context->AddModule("test");
    module->AddProperty(*context->gc()->NewString("DEBUG"),
        Value(true));
    module->AddProperty(*context->gc()->NewString("IsProduction"),
        Value(false));
    CTX(context);
    GVAR(Boolean,"r1",true);
    GVAR(Boolean,"r2",false);
  }
}

// =========================================================================
// Sub
// =========================================================================
TEST(VM,Sub) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub a { return {true}; }
            sub foo(a,b) { return {a + b}; }
            global my_result = a();
            global my_result2= foo(100,200);
            )));
    CTX(context);
    GVAR(Boolean,"my_result",true);
    GVAR(Integer,"my_result2",300);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub a {} /* Empty functions */
            global result = a();
            )));
    CTX(context);
    {
      Value v;
      ASSERT_TRUE( context->GetGlobalVariable("result",&v) );
      ASSERT_TRUE( v.IsNull() );
    }
  }
}

TEST(VM,DeclareOrNew) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              declare local = a + b;
              new shit = a * b;
              return { local + shit };
            }
            global result = foo(1,2);
           )));
    CTX(context);
    GVAR(Integer,"result",5);
  }

  // Multiple scope's variable definition
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              declare local = a + b;
              new shit = a * b;
              return { local + shit };
            }
            sub bar(c,d,e) {
              declare f = e * 10;
              return { foo(c,d) + f };
            }
            global result = bar(1,2,10);
            global u = bar(1,2,1);
            global r = result + u;
           )));
    CTX(context);
    GVAR(Integer,"result",105);
    GVAR(Integer,"r",120);
  }
}

TEST(VM,If) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              declare l = a + b;
              new c = a * b;
              if( l >= 10 ) {
                return { 100 };
              }
              return { 1000 };
            }
            global result = foo(1,2);
            global r2 = foo(10,20);
            )));
    CTX(context);
    GVAR(Integer,"result",1000);
    GVAR(Integer,"r2",100);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              if(a * b > 10) {
                return { 100 };
              } else {
                return { 1000 };
              }
            }
            global result = foo(1,2);
            global result2= foo(100,200);
            )));
    CTX(context);
    GVAR(Integer,"result",1000);
    GVAR(Integer,"result2",100);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              if(a * b > 10) {
                return {1};
              } else if( a*b < 1) {
                return {2};
              } elsif( a*b > 5) {
                return {3};
              } elif( a*b > 3) {
                return {4};
              }
              return {5};
            }
            global r1 = foo(10,20);
            global r2 = foo(0,10);
            global r3 = foo(1,6);
            global r4 = foo(1,4);
            global r5 = foo(1,1);
            )));
    CTX(context);
    GVAR(Integer,"r1",1);
    GVAR(Integer,"r2",2);
    GVAR(Integer,"r3",3);
    GVAR(Integer,"r4",4);
    GVAR(Integer,"r5",5);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              if(a * b > 10) {
                return {1};
              } else if( a*b < 1) {
                return {2};
              } elsif( a*b > 5) {
                return {3};
              } elif( a*b > 3) {
                return {4};
              } else {
                return {5};
              }

              return {100}; /* dead code */
            }
            global r1 = foo(10,20);
            global r2 = foo(0,10);
            global r3 = foo(1,6);
            global r4 = foo(1,4);
            global r5 = foo(1,1);
            )));
    CTX(context);
    GVAR(Integer,"r1",1);
    GVAR(Integer,"r2",2);
    GVAR(Integer,"r3",3);
    GVAR(Integer,"r4",4);
    GVAR(Integer,"r5",5);
  }
  // Nested
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              if(a)
                if(b)
                  if(a*b)
                    if(a+b)
                      return {10};
              return {100};
            }
            global result = foo(1,2);
            global r2 = foo(0,1);
            )));
    CTX(context);
    GVAR(Integer,"result",10);
    GVAR(Integer,"r2",100);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              if(a)
                if(b) return {1};
                else if(b==0) return {2};
                else return {3};
              else return {4};
            }
            global r1 = foo(0,1);
            global r2 = foo(1,0);
            global r3 = foo(1,1);
            )));
    CTX(context);
    GVAR(Integer,"r1",4);
    GVAR(Integer,"r2",2);
    GVAR(Integer,"r3",1);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              declare l1 = 10;
              if(a) {
                declare l2 = 20;
                declare l3 = l1 + l2;
              } else {
                if(b) {
                  declare l2 = 30;
                  declare l3 = l1 + l2;
                }
                declare l4 = 40;
                declare l5 = 50;
                declare l6 = 60;
                call bar(l4,l5,l6);
              }
              declare l2 = 100;
              declare l3 = 200;
              return { l2 + l3 + l1 };
           }
           sub bar(a,b,c) { return {a+b+c}; }
           global r1 = foo(1,1);
           global r2 = foo(0,0);
           global r3 = foo(1,0);
           global r4 = foo(0,1);
           )));
    CTX(context);
    GVAR(Integer,"r1",310);
    GVAR(Integer,"r2",310);
    GVAR(Integer,"r3",310);
    GVAR(Integer,"r4",310);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              declare l = 10;
              if(a) {
                declare l = 100;
                if(b) {
                  declare l = 1000;
                  return {l};
                }
                return {l};
              }
              return {l};
            }
            global r1 = foo(1,1);
            global r2 = foo(1,0);
            global r3 = foo(0,1);
            global r4 = foo(0,0);
            )));
    CTX(context);
    GVAR(Integer,"r1",1000);
    GVAR(Integer,"r2",100);
    GVAR(Integer,"r3",10);
    GVAR(Integer,"r4",10);
  }
}

TEST(VM,Call) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) { return {a+b}; }
            sub bar(a,b) { return {a*b}; }
            sub noarg { return {1000}; }
            sub main {
              call foo(10,20);
              call bar(20,30);
              call noarg;
              declare a = 100;
              declare b = 100;
              return {a + b};
            }
            global result = main();
            )));
    CTX(context);
    GVAR(Integer,"result",200);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global DEBUG = true;
            sub foo(a) {
              if(DEBUG) {
                return {a*10};
              } else {
                return {a+10};
              }
            }
            sub wrapper(a) {
              declare bb = a;
              declare cc = bb * 10;
              declare dd = cc * 10;
              return { foo(dd) };
            }
            global a = wrapper(1);
            )));
    CTX(context);
    GVAR(Integer,"a",1000);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a) {
              return { a * 10 };
            }

            global object = {
              "foo" : foo
            };

            sub main() {
              object.foo(100);
              return {true};
            }

            global result = main();
            )));
    CTX(context);
    GVAR(Boolean,"result",true);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo(a,b) {
              return {a*10};
            }
            sub noarg { return {true}; }
            sub main() {
              foo(1,2);
              noarg;
              return {true};
            }
            global r = main();
            )));
    CTX(context);
    GVAR(Boolean,"r",true);
  }
}

// ========================================================================
// Set/Unset
// ========================================================================
TEST(VM,SetUnset1) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo {
              declare a = 10;
              set a = 100;
              return {a};
            }
            sub bar {
              declare a = true;
              unset a;
              return {a};
            }
            global r1 = foo();
            global r2 = bar();
            )));
    CTX(context);
    GVAR(Integer,"r1",100);
    GVAR(Boolean,"r2",false);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub f1 { declare a = 1; set a += 1; return {a}; }
            sub f2 { declare a = 1; set a -= 1; return {a}; }
            sub f3 { declare a = 1; set a *= 2; return {a}; }
            sub f4 { declare a = 2; set a /= 2; return {a}; }
            sub f5 { declare a = 2; set a %= 3; return {a}; }

            sub f6 { declare a = 1.0; set a += 2.0; return {a}; }
            sub f7 { declare a = 2.0; set a -= 2.0; return {a}; }
            sub f8 { declare a = 1.0; set a *= 2.0; return {a}; }
            sub f9 { declare a = 2.0; set a /= 2.0; return {a}; }

            global r1 = f1();
            global r2 = f2();
            global r3 = f3();
            global r4 = f4();
            global r5 = f5();

            global r6 = f6();
            global r7 = f7();
            global r8 = f8();
            global r9 = f9();
            )));
    CTX(context);
    GVAR(Integer,"r1",2);
    GVAR(Integer,"r2",0);
    GVAR(Integer,"r3",2);
    GVAR(Integer,"r4",1);
    GVAR(Integer,"r5",2);

    GVAR(Real,"r6",3.0);
    GVAR(Real,"r7",0.0);
    GVAR(Real,"r8",2.0);
    GVAR(Real,"r9",1.0);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global gvar = 2;
            sub f1 { declare a = 1; set a += 1*2 / gvar; return {a}; }
            sub f2 { declare a = 2; set a -= a / gvar; return {a}; }
            sub f3 { declare a = 10; declare b = 20; set a += b + (a + 2); return {a}; }
            sub f4 { declare a = 2.0; declare b = 3.0; set a *= b + 1.0; return {a}; }
            sub f5 { declare a = 3.0; declare b = 3.0; set a /= (b - 1.0 + 1.0)*1.0; return {a}; }

            global r1 = f1();
            global r2 = f2();
            global r3 = f3();
            global r4 = f4();
            global r5 = f5();
            )));
    CTX(context);
    GVAR(Integer,"r1",2);
    GVAR(Integer,"r2",1);
    GVAR(Integer,"r3",42);
    GVAR(Real,"r4",8.0);
    GVAR(Real,"r5",1.0);
  }
  // Type promotion
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global gvar = 2;
            sub f1 { declare a = 1; set a += 2.0; return {a}; }
            sub f2 { declare a = 2.0;set a+= 1  ; return {a}; }
            sub f3 { declare a = 1 ; set a -= 2.0; return {a}; }
            sub f4 { declare a = 2.0;set a -= 2; return {a}; }
            sub f5 { declare a = 1; set a*= 2.0; return {a}; }
            sub f6 { declare a = 2.0;set a*= 1 ; return {a}; }
            // boolean conversion
            sub f7 { declare a = true; set a += 2; return {a}; }
            sub f8 { declare a = false; set a *= 2.0; return {a}; }
            sub f9 { declare a = 2.0; set a /= true ; return {a}; }
            sub f10{ declare a = 2; set a *= false; return {a}; }

            global r1 = f1();
            global r2 = f2();
            global r3 = f3();
            global r4 = f4();
            global r5 = f5();
            global r6 = f6();
            global r7 = f7();
            global r8 = f8();
            global r9 = f9();
            global r10= f10();
            )));
    CTX(context);
    GVAR(Real,"r1",3.0);
    GVAR(Real,"r2",3.0);
    GVAR(Real,"r3",-1.0);
    GVAR(Real,"r4",0.0);
    GVAR(Real,"r5",2.0);
    GVAR(Real,"r6",2.0);
    GVAR(Integer,"r7",3);
    GVAR(Real,"r8",0.0);
    GVAR(Real,"r9",2.0);
    GVAR(Integer,"r10",0);
  }
}

TEST(VM,Return) {
  // Terminated Return
#define SOURCE STRINGIFY( \
    vcl 4.0; \
    sub ok { return (ok); } \
    sub fail { return (fail); } \
    sub pipe { return (pipe); } \
    sub hash { return (hash); } \
    sub purge{ return (purge); } \
    sub lookup { return (lookup); } \
    sub restart{ return (restart); } \
    sub fetch{ return (fetch); } \
    sub miss { return (miss);  } \
    sub deliver { return (deliver); } \
    sub retry{ return (retry); } \
    sub abandon { return (abandon); } \
    )

#define XX(TYPE,CODE) \
  do { \
    boost::scoped_ptr<Context> context(CompileCode(SOURCE)); \
    CTX(context); \
    Value output; \
    ASSERT_TRUE( CallFunc(context.get(),TYPE,&output).is_terminate() ); \
    ASSERT_TRUE( output.IsAction() ); \
    ASSERT_TRUE( output.GetAction()->action_code() == CODE ); \
  } while(false)

  XX("ok",ACT_OK);
  XX("fail",ACT_FAIL);
  XX("pipe",ACT_PIPE);
  XX("hash",ACT_HASH);
  XX("purge",ACT_PURGE);
  XX("lookup",ACT_LOOKUP);
  XX("restart",ACT_RESTART);
  XX("fetch",ACT_FETCH);
  XX("miss",ACT_MISS);
  XX("deliver",ACT_DELIVER);
  XX("retry",ACT_RETRY);
  XX("abandon",ACT_ABANDON);

#undef XX
#undef SOURCE

  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub foo { return {true}; }
            sub term{ return (foo()); }
            )));
    CTX(context);
    Value output;
    ASSERT_TRUE( CallFunc(context.get(),"term",&output) );
    ASSERT_TRUE( output.IsBoolean() );
    ASSERT_TRUE( output.GetBoolean() == true );
  }
}

// =======================================================================
// Call SubRoutine at C++ side
// =======================================================================
TEST(VM,CallSubRoutine) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            sub add(a,b) { return {a+b}; }
            sub concate(a,b) { return {a+b}; }
            sub _10 { return { 10 }; }
            )));
    CTX(context);
    {
      Value output;
      ASSERT_TRUE( CallFunc(context.get(),"add",Value(1),Value(2),&output) );
      ASSERT_TRUE( output.IsInteger() );
      ASSERT_EQ( 3 , output.GetInteger() );
    }
    {
      Value output;
      ASSERT_TRUE( CallFunc(context.get(),"concate",
            Value(context->gc()->NewString("XX")),
            Value(context->gc()->NewString("YY")),
            &output));
      ASSERT_TRUE( output.IsString() );
      ASSERT_EQ( *output.GetString(), "XXYY" );
    }
    {
      Value output;
      ASSERT_TRUE( CallFunc(context.get(),"_10",&output) );
      ASSERT_TRUE( output.IsInteger() );
      ASSERT_EQ( 10 , output.GetInteger() );
    }
  }
}

// =====================================================================
// Anonymous Function/Subroutines
// =====================================================================
TEST(VM,AnonymousSub) {
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global a = sub { return { 10 }; };
            global foo = sub(a,b) { return { a + b }; };
            global b = a();
            global d = foo(1,2);
            )));
    CTX(context);
    GVAR(Integer,"b",10);
    GVAR(Integer,"d",3);
  }
  {
    boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
            vcl 4.0;
            global fib = sub(a) { if(a == 0 || a == 1 || a == 2) return {a}; else return {fib(a-1) + fib(a-2)}; };
            global val = fib(5);
            )));
    CTX(context);
    GVAR(Integer,"val",8);
  }
}

// =====================================================================
// Operators with C++ extended objects
// =====================================================================

class CppObject1 : public Extension {
  int m_integer;
  double m_real;
  String* m_string;
  bool m_boolean;
 public:
  CppObject1() : Extension("object1") {}
  virtual MethodStatus GetProperty( Context* context , const String& key ,
                                                       Value* output ) const {
    if(key == "integer") {
      output->SetInteger(m_integer);
    } else if(key == "real") {
      output->SetReal(m_real);
    } else if(key == "string") {
      output->SetString(m_string);
    } else if(key == "boolean") {
      output->SetBoolean(m_boolean);
    } else {
      return MethodStatus::NewFail("property %s doesn't exists",key.data());
    }
    return MethodStatus::kOk;
  }

  virtual MethodStatus SetProperty( Context* context , const String& key ,
                                                       const Value& value ) {
    if(key == "integer") {
      if(value.IsInteger()) {
        m_integer = static_cast<int>(value.GetInteger());
      } else {
        return MethodStatus::NewFail("property integer must be integer type");
      }
    } else if(key == "real") {
      if(value.IsReal()) {
        m_real = static_cast<double>(value.GetReal());
      } else {
        return MethodStatus::NewFail("property real must be real type");
      }
    } else if(key == "string") {
      if(value.IsString()) {
        m_string = value.GetString();
      } else {
        return MethodStatus::NewFail("property string must be string type");
      }
    } else if(key == "boolean") {
      if(value.IsBoolean()) {
        m_boolean = value.GetBoolean();
      } else {
        return MethodStatus::NewFail("property boolean must be boolean type");
      }
    } else {
      return MethodStatus::NewFail("property %s doesn't exists",key.data());
    }
    return MethodStatus::kOk;
  }

  virtual void Mark() {
    m_string->Mark();
  }
};

} // namespace vm
} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

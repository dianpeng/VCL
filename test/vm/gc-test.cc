#include <gtest/gtest.h>
#include <vm/runtime.h>
#include <vm/compiler.h>
#include <vm/vcl-pri.h>
#include <vm/compilation-unit.h>
#include <vm/parser.h>
#include <vm/procedure.h>
#include <boost/scoped_ptr.hpp>
#include <vcl/vcl.h>
#include <iostream>

namespace vcl {

TEST(GC,Basic) {
  {
    ContextGC gc(1,0.9,NULL);
    {
      for( size_t i = 0 ; i < 1000 ; ++i ) {
        Handle<String> str( gc.NewString("A") , &gc );
      }
      gc.ForceCollect();
      ASSERT_EQ( 0 , gc.gc_size() );

      Handle<String> str(gc.NewString("B"),&gc);
      for( size_t i = 0 ; i < 1000 ; ++i ) {
        Handle<String> str( gc.NewString("AAA"),&gc );
      }
      gc.ForceCollect();
      ASSERT_EQ(1,gc.gc_size());
    }
    {
      std::vector<Handle<String>*> ptr;
      for( size_t i = 0 ; i < 1000 ; ++i ) {
        ptr.push_back( new Handle<String>( gc.NewString("A"), &gc ) );
      }
      gc.ForceCollect();
      ASSERT_EQ(1000,gc.gc_size());
      for( size_t i = 0 ; i < ptr.size() ; ++i ) {
        delete ptr[i];
      }
      ptr.clear();
      gc.ForceCollect();
      ASSERT_EQ(0,gc.gc_size());
    }
  }

  // List
  {
    ContextGC gc(1,0.9,NULL);

    {
      Handle<List> my_list( gc.NewList() , &gc );
      my_list->Push( Value(1) );
      my_list->Push( Value() );
      my_list->Push( Value(true) );
      my_list->Push( Value( gc.NewString("A-String") ) );
      my_list->Push( Value( gc.NewString("A-String2") ) );

      for( size_t i = 0 ; i < 1000 ; ++i ) {
        gc.NewString("B");
      }
      gc.ForceCollect();

      ASSERT_EQ( 3 , gc.gc_size() );

      ASSERT_TRUE( my_list->Index(0).IsInteger() );
      ASSERT_EQ(1,my_list->Index(0).GetInteger());
      ASSERT_TRUE( my_list->Index(1).IsNull() );
      ASSERT_TRUE( my_list->Index(2).IsBoolean() );
      ASSERT_TRUE( my_list->Index(2).GetBoolean() );
      ASSERT_TRUE( my_list->Index(3).IsString() );
      ASSERT_TRUE( my_list->Index(3).GetString()->ToStdString() == "A-String");
      ASSERT_TRUE( my_list->Index(4).IsString() );
      ASSERT_EQ( *my_list->Index(4).GetString() , "A-String2" );
    }

    gc.ForceCollect();
    ASSERT_EQ( 0 , gc.gc_size() );
  }

  // Dict
  {
    ContextGC gc(1,0.9,NULL);
    {
      Handle<Dict> my_dict( gc.NewDict(), &gc );
      ASSERT_TRUE( my_dict->Insert( ( *gc.NewString("A") ) , Value(1) ) );
      Handle<List> l(gc.NewList(),&gc);
      Handle<String> k(gc.NewString("B"),&gc);
      ASSERT_TRUE(my_dict->Insert(*k ,Value(l)));
      Handle<String> nk(gc.NewString("C"),&gc);
      Handle<String> nv(gc.NewString("D"),&gc);
      ASSERT_TRUE(my_dict->Insert( *nk , Value(nv)));
      ASSERT_EQ(6,gc.gc_size());

      for( size_t i = 0 ; i < 1000 ; ++i ) {
        gc.NewDict();
        gc.NewList();
        gc.NewString("__");
      }

      gc.ForceCollect();
      ASSERT_EQ(6,gc.gc_size());

      Value v;
      ASSERT_TRUE( my_dict->Find( *gc.NewString("A") , &v ) );
      ASSERT_TRUE( v.IsInteger() );
      ASSERT_EQ( 1 , v.GetInteger());

      ASSERT_TRUE( my_dict->Find( *gc.NewString("B") , &v ) );
      ASSERT_TRUE( v.IsList() );
      ASSERT_TRUE( v.GetList()->empty() );

      ASSERT_TRUE( my_dict->Find( *gc.NewString("C") , &v ) );
      ASSERT_TRUE( v.IsString() );
      ASSERT_EQ(*v.GetString(),"D");
    }
    gc.ForceCollect();
    ASSERT_EQ(0,gc.gc_size());
  }
}

// Module ======================================================

class MyFoo : public Function {
 public:
  MyFoo() : Function("MyFoo") {}

  virtual MethodStatus Invoke( Context* context , Value* output ) {
    output->SetNull();
    VCL_UNUSED(context);
    return MethodStatus::kOk;
  }
};


class MyExt : public Extension {
 public:
  MyExt() : Extension( "MyExt" ) {}
};

TEST(GC,Module) {
  {
    ContextGC gc(1,0.5,NULL);
    {
      Handle<Module> module( gc.NewModule("my_module") , &gc);
      module->AddProperty( *gc.NewString("A") , Value(1) );
      {
        Handle<String> key(gc.NewString("MyFoo"),&gc);
        Handle<Function> val(gc.New<MyFoo>(),&gc);
        module->AddProperty( *key , Value(val) );
      }
      {
        Handle<String> key(gc.NewString("MyExt"),&gc);
        Handle<Extension> val(gc.New<MyExt>(),&gc);
        module->AddProperty( *key , Value(val) );
      }
      gc.ForceCollect();
      ASSERT_EQ(6,gc.gc_size());

      {
        Value v;
        ASSERT_TRUE( module->GetProperty(NULL,*gc.NewString("A"),&v));
        ASSERT_TRUE( v.IsInteger() );
        ASSERT_EQ( 1 , v.GetInteger() );
      }
      {
        Value v;
        ASSERT_TRUE( module->GetProperty(NULL,*gc.NewString("MyFoo"),&v));
        ASSERT_TRUE( v.IsFunction() );
        ASSERT_EQ( v.GetFunction()->name() , "MyFoo" );
      }
      {
        Value v;
        ASSERT_TRUE( module->GetProperty(NULL,*gc.NewString("MyExt"),&v));
        ASSERT_TRUE( v.IsExtension() );
        ASSERT_EQ( v.GetExtension()->extension_name() , "MyExt");
      }
    }
    gc.ForceCollect();
    ASSERT_EQ(0,gc.gc_size());
  }
}

// ===================================================================
// Runtime based GC testing
// ===================================================================

using namespace vm;

Context* CompileCode( const char* source , size_t trigger = 1 , double ratio = 0.5 ) {
  boost::shared_ptr<CompiledCode> cc( new CompiledCode( NULL ) );
  ContextOption opt;
  opt.gc_trigger = trigger;
  opt.gc_ratio = ratio;

  Context* context = new Context( opt , cc );
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

MethodStatus CallFunc( Context* context , const char* name , Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , output );
}

#define CC(...) CompileCode(#__VA_ARGS__)
#define CCC(TRIGGER,RATIO,...) CompileCode(#__VA_ARGS__,TRIGGER,RATIO)

TEST(GC,Runtime) {
  {
    boost::scoped_ptr<Context> ctx(CC( vcl 4.0;
                       global a = "string";
                       global b = a + "hello";
                       global c = "you-are-right";
                     ));
    ASSERT_TRUE(ctx.get());
    ASSERT_TRUE(ctx->Construct());
    ctx->gc()->ForceCollect();
    ASSERT_EQ(3,ctx->gc()->gc_size());
  }

  {
    boost::scoped_ptr<Context> ctx(CC( vcl 4.0;
                       global a = "string";
                       sub foo {
                        return { a + "World" };
                       }
                     ));
    ASSERT_TRUE(ctx.get());
    ASSERT_TRUE(ctx->Construct());
    ctx->gc()->ForceCollect();
    ASSERT_EQ(4,ctx->gc()->gc_size());
    Value result;
    ASSERT_TRUE( CallFunc(ctx.get(),"foo",&result) );
    Handle<Value> value(result,ctx->gc());
    ctx->gc()->ForceCollect();
    ASSERT_EQ(5,ctx->gc()->gc_size());
  }

  {
    boost::scoped_ptr<Context> ctx(CC( vcl 4.0;
          global a = [];
          global b = [a , "string" , true , false , null , {}];
          global c = { "a" : "b" , "c" : "dd" , "e" : {} };
          sub foo { return {a}; }
          sub bar { return {b}; }
          sub fee { return {c}; }
          ));
    ASSERT_TRUE( ctx.get() );
    ASSERT_TRUE( ctx->Construct() );
    ctx->gc()->ForceCollect();
    ASSERT_EQ( 13 , ctx->gc()->gc_size() );
    Value result;
    ASSERT_TRUE( CallFunc(ctx.get(),"foo",&result) );
    Handle<Value> v1(result,ctx->gc());
    ASSERT_TRUE( CallFunc(ctx.get(),"bar",&result) );
    Handle<Value> v2(result,ctx->gc());
    ASSERT_TRUE( CallFunc(ctx.get(),"fee",&result) );
    Handle<Value> v3(result,ctx->gc());

    ctx->gc()->ForceCollect();
    ASSERT_EQ( 13 , ctx->gc()->gc_size() );
  }
}

class Add : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    std::string sum;

    // Variable length
    for( size_t i = 0 ; i < context->GetArgumentSize() ; ++i ) {
      Value arg = context->GetArgument(i);
      if(arg.IsString()) {
        sum += arg.GetString()->ToStdString();
      } else {
        return MethodStatus::NewFail("function::Add's %zu argument is type %s,"
            "but expect a string",i+1,arg.type_name());
      }
    }

    output->SetString( context->gc()->NewString(sum) );
    return MethodStatus::kOk;
  }

  Add():Function("Add") {}
};

TEST(GC,Runtime2) {
  {
    boost::scoped_ptr<Context> ctx(CC(vcl 4.0;
          global a = Add("A","B","C","D","EEEE");
          global b = Add("A",Add("B","C",a));
          ));
    ctx->AddOrUpdateGlobalVariable("Add",Value(ctx->gc()->New<Add>()));
    ASSERT_TRUE( ctx->Construct() );
    ctx->gc()->ForceCollect();
    ASSERT_EQ( 6 , ctx->gc()->gc_size() );
  }
}


} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

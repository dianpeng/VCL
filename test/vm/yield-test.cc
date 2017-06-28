#include <gtest/gtest.h>
#include <vm/runtime.h>
#include <vm/compiler.h>
#include <vm/vcl-pri.h>
#include <vm/compilation-unit.h>
#include <vm/parser.h>
#include <vm/procedure.h>
#include <boost/scoped_ptr.hpp>

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

MethodStatus CallFunc( Context* context , const char* name , Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , output );
}


// --------------------------------------------------------------------------
// Testing Yield
// --------------------------------------------------------------------------
class FunctionYield : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    std::cout<<"I am yieldding\n";
    output->SetNull();
    return MethodStatus::kYield;
  }
  FunctionYield() : Function("yield") {}
};


TEST(Yield,Function) {
  boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
          vcl 4.0;
          global x = 10;
          sub foo {
            yield();
            return { "xx" };
          }

          sub multi_yield {
            declare x = "xx";
            yield();
            declare y = "yy";
            yield();
            declare z = "zz";
            yield();
            return { x + y + z };
          }
          )));

  ASSERT_TRUE(context.get());
  // Register global function for this context
  {
    Handle<String> key( context->gc()->NewString("yield") ,
                        context->gc() );
    Handle<Function> val( context->gc()->New<FunctionYield>() ,
                          context->gc() );
    context->AddOrUpdateGlobalVariable( *key , Value(val) );
  }
  ASSERT_TRUE(context->Construct());

  {
    Value output;
    MethodStatus status = CallFunc(context.get(),"foo",&output);
    ASSERT_TRUE( context->is_yield() );
    ASSERT_TRUE( status.is_yield() );
    context->Resume(&output);
    ASSERT_TRUE( output.IsString() );
    ASSERT_EQ( *output.GetString() , "xx" );
  }
  {
    Value output;
    MethodStatus status = CallFunc(context.get(),"multi_yield",&output);
    ASSERT_TRUE( context->is_yield() );
    ASSERT_TRUE( status.is_yield() );
    ASSERT_TRUE( context->Resume(&output).is_yield() );
    ASSERT_TRUE( context->is_yield() );
    ASSERT_TRUE( context->Resume(&output).is_yield() );
    ASSERT_TRUE( context->is_yield() );
    ASSERT_TRUE( context->Resume(&output).is_ok() );
    ASSERT_TRUE( output.IsString() );
    ASSERT_EQ( *output.GetString(), "xxyyzz" );
  }
}

} // namespace vm
} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

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

class FakeLoadFileInterface : public LoadFileInterface {
 public:
  virtual bool LoadFile( const std::string& path , std::string* content ) {
    if(path == "a.vcl") {
      content->assign(
          STRINGIFY(
            vcl 4.0;
            global u = 100;
            global v = "string";
            sub foo() { return {1}; }
            sub bar { return {2}; }
            ));
    } else if( path == "b.vcl" ) {
      content->assign(
          STRINGIFY(
            vcl 4.0;
            global x = 100;
            global y = 200;
            sub foo() { return {100}; }
            sub bar() { return {200}; }
            ));
    } else {
      return false;
    }
    return true;
  }
};

class CircularLoadFileInterface : public LoadFileInterface {
 public:
  virtual bool LoadFile( const std::string& path , std::string* content ) {
    if(path == "a.vcl") {
      content->assign(
          STRINGIFY(
            vcl 4.0;
            include "b.vcl";
            global a = 1000;
          ));
    } else if( path == "b.vcl" ) {
      content->assign(
          STRINGIFY(
            vcl 4.0;
            include "a.vcl";
            global b = 1000;
            ));
    } else {
      return false;
    }
    return true;
  }
};

namespace vm {

Context* CompileCode( const char* source , LoadFileInterface* interface = NULL ) {
  boost::shared_ptr<CompiledCode> cc( new CompiledCode( NULL ) );
  Context* context = new Context( ContextOption() , cc );
  CompilationUnit cu;
  std::string error;
  SourceRepo source_repo(interface);

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

TEST(CU,Runtime1) {
  boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
        vcl 4.0;
        include "a.vcl";
        include "b.vcl";
        global a = [];
        global b = {};
        sub foo() { if(a) return {2}; else return {3}; }
        sub bar { if(a) return {3}; else return {4}; }
        ),new FakeLoadFileInterface()));
  ASSERT_TRUE(context.get());
  context->compiled_code()->Dump(std::cerr);
}

TEST(CU,RuntimeFail) {
  boost::scoped_ptr<Context> context(CompileCode(STRINGIFY(
          vcl 4.0;
          include "a.vcl";
          ),new CircularLoadFileInterface()));
  ASSERT_FALSE(context.get());
}

} // namespace vm
} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

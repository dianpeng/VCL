#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <vcl/vcl.h>

namespace vcl {
namespace vm {

Context* CompileCode( Engine* engine , const char* path ) {
  std::string error;
  boost::shared_ptr<CompiledCode> cc( engine->LoadFile(path,ScriptOption(),&error) );
  if(!cc.get()) { std::cerr<<error<<std::endl; return NULL; }
  ContextOption copt;
  copt.gc_trigger = 2;
  copt.gc_ratio = 0.5;
  return new Context( copt , cc );
}

MethodStatus CallFunc( Context* context , const char* name , Value* output ) {
  Value f;
  if(!context->GetGlobalVariable(name,&f)) return false;
  if(!f.IsSubRoutine()) return false;
  return context->Invoke( f.GetSubRoutine() , output );
}

class Assert : public Function {
 public:
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    CHECK( context->GetArgumentSize() == 2 ||
           context->GetArgumentSize() == 1 );
    bool value;
    CHECK( context->GetArgument(0).ToBoolean(context,&value) );
    output->SetNull();
    if(!value && context->GetArgumentSize() == 2) {
      CHECK( context->GetArgument(1).IsString() );
      std::cerr<<context->GetArgument(1).GetString()->data()<<std::endl;
    }
    return value ? MethodStatus::kOk : MethodStatus::kFail;
  }

  Assert() : Function("assert") {}
};

bool IsValidVCLFile( const boost::filesystem::path& p ) {
  boost::filesystem::path::string_type name = p.filename().string();
  if( name == ".." || name == "." || name[0] == '.' ) {
    return false;
  } else {
    if(boost::filesystem::extension(p) == ".vcl") {
      return true;
    }
    return false;
  }
}

void Driver( const char* folder ) {
  size_t count = 0;
  size_t ok = 0;
  Engine engine;
  for( boost::filesystem::directory_iterator itr(folder) ; itr !=
      boost::filesystem::directory_iterator() ; ++itr ) {
    if( boost::filesystem::is_regular_file( itr->status() ) ) {
      if(IsValidVCLFile(*itr)) {
        ++count;
        std::cerr<<"Processing "<<itr->path().c_str()<<'\n';
        std::string content;
        boost::scoped_ptr<Context> context(CompileCode(&engine,itr->path().c_str()));
        if(context) {
          context->AddOrUpdateGlobalVariable("assert",Value(context->gc()->New<Assert>()));
          Value v;
          CHECK( context->Construct() );
          MethodStatus status = CallFunc(context.get(),"test",&v);
          if(!status) { std::cerr<<"test function failed:"<<status.fail()<<'\n'; }
          else { ++ok; }
          (void)v;
        }
      } else {
        std::cerr<<"Skipping file "<<itr->path().filename().string()<<'\n';
      }
    }
  }
  std::cerr<<"*************************** SUMMARY *****************************\n";
  std::cerr<<"TestCount:"<<count<<"\n";
  std::cerr<<"Success:"<<ok<<"\n";
  if(count != 0)
    std::cerr<<"SuccessRate:"<<(static_cast<double>(ok) / static_cast<double>(count))<<"\n";
  else
    std::cerr<<"SuccessRate:0.0\n";
  std::cerr<<"*****************************************************************\n";
}

} // namespace vm
} // namespace vcl


int main( int argc , char* argv[] ) {
  vcl::InitVCL( argv[0] );
  if(argc != 2) {
    std::cerr<<"Usage <path>\n";
    return -1;
  } else {
    vcl::vm::Driver(argv[1]);
    return 0;
  }
}

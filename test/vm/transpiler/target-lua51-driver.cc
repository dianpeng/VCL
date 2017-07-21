#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <vcl/vcl.h>

namespace vcl {

bool Driver( const char* folder ) {
  if( boost::filesystem::status(folder).type() == boost::filesystem::regular_file ) {
    std::string error , output;
    if(!::vcl::experiment::TranspileFile(folder,::vcl::experiment::TranspilerOptionTable(),
                                                ::vcl::ScriptOption(),
                                                ::vcl::experiment::TARGET_LUA51,
                                                &output,
                                                &error)) {
      std::cerr<<error<<std::endl;
      return false;
    } else {
      std::cout<<output<<std::endl;
      return true;
    }
  }
  std::cerr<<"Invalid file path "<<folder<<std::endl;
  return false;
}

} // namespace vcl

int main( int argc , char* argv[] ) {
  if(argc != 2) {
    std::cerr<<"Usage: filepath\n";
    return -1;
  }
  return vcl::Driver(argv[1]) ? 0 : -1;
}

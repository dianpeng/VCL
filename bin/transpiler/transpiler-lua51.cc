#include <vcl/vcl.h>
#include <boost/program_options.hpp>
#include <iterator>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;

int main( int argc , char* argv[] ) {
  po::options_description desc("VCL to Lua51 Transpiler");

  desc.add_options()
    ("help","vcl2lua51 [options]...")
    ("input",po::value<std::string>()->default_value("-"),"file for transpile")
    ("output",po::value<std::string>()->default_value("-"),"file for output")
    ("comment",po::value<std::string>()->default_value(""),"comment to put into result,cannot have linebreak!")
    ("allow-terminate-return",po::bool_switch()->default_value(true),"allow terminate return")
    ("ok-code",po::value<int>()->default_value(0),"ok status code,default 0")
    ("fail-code",po::value<int>()->default_value(1),"fail status code,default 1")
    ("pipe-code",po::value<int>()->default_value(2),"pipe status code,default 2")
    ("hash-code",po::value<int>()->default_value(3),"hash status code,default 3")
    ("purge-code",po::value<int>()->default_value(4),"purge status code,default 4")
    ("lookup-code",po::value<int>()->default_value(5),"lookup status code,default 5")
    ("restart-code",po::value<int>()->default_value(6),"restart status code,default 6")
    ("fetch-code",po::value<int>()->default_value(7),"fetch status code,default 7")
    ("miss-code",po::value<int>()->default_value(8),"miss status code,default 8")
    ("deliver-code",po::value<int>()->default_value(9),"deliver status code,default 9")
    ("retry-code",po::value<int>()->default_value(10),"retry status code,default 10")
    ("abandon-code",po::value<int>()->default_value(11),"abandon status code,default 11")
    ("empty-code",po::value<int>()->default_value(-1),"empty status code,used to indicate nothing happened,default -1")
    ("allow-module-inline",po::bool_switch()->default_value(false),"allow module to be inlined instead of require" \
                                                                   ",once defined must define inline_module_name!")
    ("inline-module-name",po::value<std::string>()->default_value(""),"inline_module_name specify name for inline module")
    ("runtime-namespace",po::value<std::string>()->default_value("__vcl"),"speicify customized the namespace for "
                                                                                "all needed runtime function,default to "
                                                                                "__vcl")
    ("runtime-path",po::value<std::string>()->default_value(""),"specify a path which will be loaded as internal runtime object");


  po::variables_map vmap;
  try {
    po::store(po::parse_command_line(argc,argv,desc),vmap);
    po::notify(vmap);
  } catch( ... ) {
    std::cerr<<desc<<'\n';
    return -1;
  }

  ::vcl::experiment::TranspilerOptionTable table;

#define DO(A,B,C) \
  table.insert( std::make_pair(std::string(A),::vcl::experiment::TranspilerOptionValue( (vmap[B].as<C>()))) )

  DO("comment","comment",std::string);
  DO("allow_terminate_return","allow-terminate-return",bool);
  DO("ok_code","ok-code",int);
  DO("fail_code","fail-code",int);
  DO("pipe_code","pipe-code",int);
  DO("hash_code","hash-code",int);
  DO("purge_code","purge-code",int);
  DO("lookup_code","lookup-code",int);
  DO("restart_code","restart-code",int);
  DO("fetch_code","fetch-code",int);
  DO("miss_code","miss-code",int);
  DO("deliver_code","deliver-code",int);
  DO("retry_code","retry-code",int);
  DO("abandon_code","abandon-code",int);
  DO("empty_code","empty-code",int);
  DO("allow_module_inline","allow-module-inline",bool);
  DO("inline_module_name","inline-module-name",std::string);
  DO("runtime_namespace","runtime-namespace",std::string);
  DO("runtime_path","runtime-path",std::string);

#undef DO // DO

  ::vcl::InitVCL( argv[0] );

  std::istreambuf_iterator<char> start , end;
  std::ifstream input_file;

  if( vmap["input"].as<std::string>() == "-" ) {
    std::cin>>std::noskipws;
    start = std::istreambuf_iterator<char>(std::cin);
  } else {
    input_file.open( vmap["input"].as<std::string>().c_str(), std::ios_base::in );
    if(!input_file) {
      std::cerr<<"Cannot open input file "<<vmap["input"].as<std::string>()<<'\n';
      return -1;
    }
    input_file>>std::noskipws;
    start = std::istreambuf_iterator<char>(input_file);
  }
  std::string input(start,end);
  std::string output;
  std::string error;

  if(!::vcl::experiment::TranspileString( vmap["input"].as<std::string>() ,
                                          input,
                                          table,
                                          ::vcl::ScriptOption(),
                                          ::vcl::experiment::TARGET_LUA51,
                                          &output,
                                          &error)) {
    std::cerr<<error<<std::endl;
    return -1;
  }

  if( vmap["output"].as<std::string>() == "-" ) {
    std::cout<<output;
  } else {
    std::ofstream output_file;
    output_file.open(vmap["output"].as<std::string>().c_str(),std::ios_base::out|std::ios_base::trunc);
    if(!output_file) {
      std::cerr<<"Cannot open output file "<<vmap["output"].as<std::string>()<<std::endl;
      return -1;
    }
    output_file << output;
  }

  return 0;
}

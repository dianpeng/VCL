#ifndef TARGET_LUA51_H_
#define TARGET_LUA51_H_

#include <string>

#include "../ast.h"
#include "../zone.h"
#include "../compilation-unit.h"
#include "../vcl-pri.h"


namespace vcl {
namespace vm  {
namespace transpiler {
namespace lua51 {

// Transpiler options

struct Options {
  // Temporary variable prefix
  std::string temporary_variable_prefix;

  std::string vcl_main;
  std::string vcl_main_coroutine;
  std::string vcl_terminate_code;
  std::string vcl_type_name;

  // Support action return
  bool allow_terminate_return;

  // -----------------------------------------------------------------
  // If we allow action return (terminate style) , then what code will
  // be used to map these action code :
  //
  // 1. OK
  // 2. Fail
  // 3. Pipe
  // 4. Hash
  // 5. Purge
  // 6. Lookup
  // 7. Restart
  // 8. Fetch
  // 9. Miss
  // 10.Deliver
  // 11.Retry
  // 12.Abandon
  // ------------------------------------------------------------------
  int ok_code;
  int fail_code;
  int pipe_code;
  int hash_code;
  int purge_code;
  int lookup_code;
  int restart_code;
  int fetch_code;
  int miss_code;
  int deliver_code;
  int retry_code;
  int abandon_code;
  int empty_code;

  // Assume module cannot be loaded but is part of the runtime
  //
  // If this is true, then all the "import" statement will be ignored
  // but all module reference call will be direct to the inline_module_name
  bool allow_module_inline;

  std::string inline_module_name;

  // Runtime support -------------------------------------------------
  std::string runtime_namespace;


  Options():
    temporary_variable_prefix("__VCL_temp_"),
    vcl_main                 ("__VCL_main__"),
    vcl_main_coroutine       ("__VCL_main_coroutine__"),
    vcl_terminate_code       ("__VCL_terminate_code__"),
    vcl_type_name            ("__VCL_type__"),
    allow_terminate_return   (true),
    ok_code                  (0),
    fail_code                (1),
    pipe_code                (2),
    hash_code                (3),
    purge_code               (4),
    lookup_code              (5),
    restart_code             (6),
    fetch_code               (7),
    miss_code                (8),
    deliver_code             (9),
    retry_code               (10),
    abandon_code             (11),
    empty_code               (-1),
    allow_module_inline      (false),
    inline_module_name       (),
    runtime_namespace        ("vcl.runtime.")
  {}
};


// Do the transpiling , convert the CompilationUnit to valid LUA code
bool Transpile( const std::string& filename , const std::string& comment ,
                const CompiledCode& cc , const CompilationUnit& ,
                zone::Zone* , const Options& ,
                std::string* , std::string* );


#endif // TARGET_LUA51_H_

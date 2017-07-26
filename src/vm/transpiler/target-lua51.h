#ifndef TARGET_LUA51_H_
#define TARGET_LUA51_H_
#include <vcl/vcl.h>
#include <string>

namespace vcl {
class CompiledCode;
namespace vm  {
class CompilationUnit;
namespace transpiler {
namespace lua51 {

// Transpiler options

struct Options {
  // Script comment
  std::string comment;

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

  // If we need to inline runtime based on the runtime path
  // this means we will generate a line of code like this
  //
  // local ${runtime_namespace} = require(${runtime_path})
  std::string runtime_path;

  // Creation function for creating a Options object from the TranspilerOptionTable
  static bool Create( const ::vcl::experiment::TranspilerOptionTable& , Options* ,
                                                                        std::string* );

 public:
  Options():
    comment                  (),
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
    runtime_namespace        ("__vcl"),
    runtime_path             ("")
  {}
};


// Do the transpiling , convert the CompilationUnit to valid LUA code
bool Transpile( const std::string& filename , const CompiledCode& cc ,
                const CompilationUnit& , const Options& ,
                std::string* , std::string* );

} // namespace lua51
} // namespace transpiler
} // namespace vm
} // namespace vcl


#endif // TARGET_LUA51_H_

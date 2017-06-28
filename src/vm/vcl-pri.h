#ifndef VCL_PRI_H_
#define VCL_PRI_H_
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <vcl/vcl.h>

#include "zone.h"
#include "ast.h"

namespace vcl {
class CompiledCode;
namespace vm {
namespace ast{
struct Sub;
} // namespace ast
} // namespace vm

extern const std::string kEntryProcName;
extern const std::string kEntryProcProtocol;

// To effectively representing the source code and compiled code we need
// some specific data structure to handle it. A *single* source code file
// will be compiled into an AST which has its own Zone object and the zone
// object will be put onto a SourceCode object. Because the compiled code
// is actually a collection of multiple source code, then each SourceCode
// will have a list of pointer points to an CompiledObject this source
// contribute to. This information is built up by the CompilationUnit phase.
// This pointer is important because when a SourceCode/file is invalided by
// user automatically, then we are able to invalid *all the realted compiled
// code* at once. Then later on we could regenerate CompilationUnit and do
// the recompile.
struct SourceCode {
  boost::shared_ptr<SourceCodeInfo> source_code_info;
  vm::ast::File* root;

  SourceCode():
    source_code_info( new SourceCodeInfo() ),
    root       (NULL)
  {}
};

// For unittest purpose
struct LoadFileInterface {
  virtual bool LoadFile( const std::string& path ,
                         std::string* content ) = 0;
  virtual ~LoadFileInterface() {}
};

class SourceRepo {
  typedef boost::ptr_map<std::string,SourceCode> SourceCodeTable;
 public:
  SourceRepo( LoadFileInterface* interface = NULL , bool allow_loop = true ):
    m_source_code_table(),
    m_entry(NULL),
    m_interface(interface),
    m_zone(),
    m_rand_name_seed(0),
    m_allow_loop    (allow_loop)
  {}

  ~SourceRepo() { delete m_interface; }

 public:
  bool Initialize( const std::string& source_filename ,
                   const std::string& source_code ,
                   std::string* error );

  SourceCode* GetEntry() {
    DCHECK(m_entry);
    return m_entry;
  }

  SourceCode* FindOrLoadSourceCode( const std::string& file_path ,
      std::string* error );

  vm::zone::Zone* zone() { return &m_zone; }

 private:
  SourceCodeTable m_source_code_table;
  SourceCode* m_entry;
  LoadFileInterface* m_interface;
  vm::zone::Zone m_zone;
  int m_rand_name_seed; // Used for parser to generate random name for
                        // anonymous sub/function names
  bool m_allow_loop;    // Whether we allow loop

  VCL_DISALLOW_COPY_AND_ASSIGN(SourceRepo);
};


class CompiledCodeBuilder {
 public:
  CompiledCodeBuilder( CompiledCode* cc ):
    m_cc(cc)
  {}

 public:
  // Create a new SubRoutine and push it back to
  vm::Procedure* CreateSubRoutine( const vm::ast::Sub& , uint32_t* index );

  // Get the SubRoutine index from this CompiledCode object
  int GetSubRoutineIndex( vm::zone::ZoneString* ) const;

  // Index a specific SubRoutine
  vm::Procedure* IndexSubRoutine( uint32_t index ) const {
    if(index < m_cc->m_sub_routine_list.size())
      return &(m_cc->m_sub_routine_list[index]);
    else
      return NULL;
  }

 private:
  CompiledCode* m_cc;

  VCL_DISALLOW_COPY_AND_ASSIGN(CompiledCodeBuilder);
};

// ------------------------------------------------------------------
// This class is used to do object allocation that is *not* supposed
// to be used by the user's . For example, newing a SubRoutine will
// be useless for users.
// It is called as allocator, but it is really just a wrapper around
// GC to provide convinient function for creating internal GC object.
class InternalAllocator {
 public:
  InternalAllocator( GC* gc ):
    m_gc(gc)
  {}

  SubRoutine* NewSubRoutine( vm::Procedure* procedure ) {
    return m_gc->New<SubRoutine>(procedure);
  }

  vm::Procedure* NewEntryProcedure();

  ACL* NewACL( vm::IPPattern* pattern ) {
    return m_gc->New<ACL>(pattern);
  }
 private:
  GC* m_gc;
};


// Private APIS ------------------------------------------------------
// Translate an action string into builtin action index. If the string
// cannot be translated , it will return type as ACT_EXTENSION.
ActionType GetActionNameEnum( const char* );

const char* GetActionName( ActionType );

} // namespace vcl

#endif // VCL_PRI_H_

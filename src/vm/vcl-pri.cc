#include <glog/logging.h>
#include <boost/format.hpp>
#include <boost/range.hpp>
#include "ast.h"
#include "parser.h"
#include "procedure.h"

namespace vcl {
using namespace vm;
using namespace ast;

const std::string kEntryProcName("__ctor__");
const std::string kEntryProcProtocol("__ctor__()");

namespace {
#define __(A, B) B,
static const char* kActionStringTable[] = {VCL_ACTION_LIST(__) NULL};
#undef __  // __
}  // namespace

// ===========================================================================
// SourceRepo
// SourceRepo represents a transient parsing collection. It used to
// parse *one* entry file during CompilationUnit generation. The reason
// we don't permentally store the Parsed Code into a global table is
// for simplicity.
// 1. User will rarely re-parsing and re-emitting code for any existed
//    file. Once user get its Context object then user is able to reuse
//    it multiple times.
// 2. The only time we want to recompile the file is when the file changes.
//    Since the file has been changed, then we need to update existed parsed
//    file so there is no point to store it.
// ===========================================================================
bool SourceRepo::Initialize(const std::string& source_code_name,
                            const std::string& source_code,
                            std::string* error) {
  vcl::util::scoped_ptr<SourceCode> pu(new SourceCode());
  pu->source_code_info->file_path = source_code_name;
  pu->source_code_info->source_code = source_code;
  Parser parser(&m_zone,
                pu->source_code_info->file_path,
                pu->source_code_info->source_code,
                error,
                m_rand_name_seed,
                m_allow_loop);
  if (!(pu->root = parser.DoParse())) {
    return false;
  }
  m_source_code_table.insert(pu->source_code_info->file_path, pu.get());
  m_entry = pu.release();                      // Release the ownership
  m_rand_name_seed = parser.rand_name_seed();  // Get the next rand_name_seed
  return true;
}

SourceCode* SourceRepo::FindOrLoadSourceCode(const std::string& file_path,
                                             std::string* error) {
  std::string temp_file_path(file_path);
  SourceCodeTable::iterator itr = m_source_code_table.find(temp_file_path);
  if (itr == m_source_code_table.end()) {
    vcl::util::scoped_ptr<SourceCode> pu(new SourceCode());
    pu->source_code_info->file_path = file_path;

    if (m_interface) {
      if (!m_interface->LoadFile(file_path,
                                 &pu->source_code_info->source_code)) {
        *error = (boost::format("cannot load source file from path %s") %
                  (file_path))
                     .str();
        return NULL;
      }
    } else if (!vcl::util::LoadFile(file_path,
                                    &pu->source_code_info->source_code)) {
      *error =
          (boost::format("cannot load source file from path %s") % (file_path))
              .str();
      return NULL;
    }

    {  // Do the parsing
      Parser parser(&m_zone,
                    file_path,
                    pu->source_code_info->source_code,
                    error,
                    m_rand_name_seed,
                    m_allow_loop);
      if (!(pu->root = parser.DoParse())) {
        return NULL;
      }
      SourceCode* ret = pu.get();
      m_source_code_table.insert(temp_file_path, pu.release());
      m_rand_name_seed = parser.rand_name_seed();
      return ret;
    }
  }
  return itr->second;
}

vm::Procedure* CompiledCodeBuilder::CreateSubRoutine(const vm::ast::Sub& sub,
                                                     uint32_t* index) {
  DCHECK(GetSubRoutineIndex(sub.sub_name) == -1);
  m_cc->m_sub_routine_list.push_back(
      new vm::Procedure(sub.sub_name ? sub.sub_name->ToStdString() : "",
                        vm::ast::Sub::FormatProtocol(sub),
                        sub.arg_list.size()));
  if (index)
    *index = static_cast<uint32_t>(m_cc->m_sub_routine_list.size() - 1);
  return &(m_cc->m_sub_routine_list.back());
}

// Get the SubRoutine index from this CompiledCode object
int CompiledCodeBuilder::GetSubRoutineIndex(vm::zone::ZoneString* name) const {
  const size_t len = m_cc->m_sub_routine_list.size();

  for (size_t i = 0; i < len; ++i) {
    if (*name == m_cc->m_sub_routine_list[i].name()) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

vm::Procedure* InternalAllocator::NewEntryProcedure() {
  vm::Procedure* procedure =
      new vm::Procedure(kEntryProcName, kEntryProcProtocol, 0);
  return procedure;
}

ActionType GetActionNameEnum(const char* string) {
  for (size_t i = 0; i < boost::size(kActionStringTable) - 2; ++i) {
    if (strcmp(string, kActionStringTable[i]) == 0)
      return static_cast<ActionType>(i);
  }
  return ACT_EXTENSION;
}

const char* GetActionName(ActionType index) {
  CHECK(index >= 0 && index <= vcl::ACT_EXTENSION);
  return kActionStringTable[index];
}

}  // namespace vcl

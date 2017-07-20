#include "compilation-unit.h"
#include <boost/make_shared.hpp>
#include <map>
#include <set>
#include "ast.h"

namespace vcl {
namespace vm {
namespace detail {

using namespace vcl::vm::ast;
class IncludeGuard;
class PUPusher;

// Internal builder
class CompilationUnitBuilder {
  typedef std::map<std::string, CompilationUnit::SubListPtr> SubListIndex;

 public:
  CompilationUnitBuilder(CompiledCode* cc,
                         CompilationUnit* cu,
                         SourceRepo* repo,
                         size_t max_include,
                         const std::string& folder_hint,
                         bool allow_absolute_path,
                         std::string* error)
      : m_repo(repo),
        m_cur_pu(repo->GetEntry()),
        m_cc(cc),
        m_cu(cu),
        m_error(error),
        m_include_stack(),
        m_cur_source_index(0),
        m_total_include(0),
        m_max_include(max_include),
        m_folder_hint(folder_hint),
        m_allow_absolute_path(allow_absolute_path) {
    m_cur_source_index = m_cc->AddSourceCodeInfo(m_cur_pu->source_code_info);
    m_include_stack.insert(m_cur_pu->source_code_info->file_path);
  }

  bool Build() { return DoExpand(*m_cur_pu->root); }

  bool DoExpand(const File&);
  bool DoInclude(const Include&);
  bool DoSub(const Sub&);
  void ReportError(const vcl::util::CodeLocation& loc, const char* fmt, ...);
  CompilationUnit::SubList* AddSub(const std::string& sub_name);
  vcl::util::FilePathStatus FormatFilePath(const std::string& postfix,
                                           std::string* output) const;

 private:
  SourceRepo* m_repo;
  SourceCode* m_cur_pu;
  CompiledCode* m_cc;
  CompilationUnit* m_cu;
  std::string* m_error;
  std::set<std::string> m_include_stack;
  uint32_t m_cur_source_index;
  size_t m_total_include;
  const size_t m_max_include;
  SubListIndex m_sub_index;
  std::string m_folder_hint;
  bool m_allow_absolute_path;

  friend class IncludeGuard;
  friend class PUPusher;
};

class IncludeGuard {
 public:
  IncludeGuard(CompilationUnitBuilder* builder, const std::string& path)
      : m_builder(builder), m_path(path), m_itr(), m_fail(false) {
    std::set<std::string>::iterator itr =
        m_builder->m_include_stack.find(m_path);
    if (itr != m_builder->m_include_stack.end()) {
      m_fail = true;
    } else {
      m_itr = m_builder->m_include_stack.insert(m_path).first;
    }
  }

  const std::string& path() const { return m_path; }

  ~IncludeGuard() {
    if (!m_fail) m_builder->m_include_stack.erase(m_itr);
  }

  operator bool() const { return m_fail; }

 private:
  CompilationUnitBuilder* m_builder;
  std::string m_path;
  std::set<std::string>::iterator m_itr;
  bool m_fail;
};

class PUPusher {
 public:
  PUPusher(CompilationUnitBuilder* builder, SourceCode* pu)
      : m_builder(builder),
        m_old_pu(builder->m_cur_pu),
        m_old_source_code_index(builder->m_cur_source_index) {
    m_builder->m_cur_pu = pu;
    builder->m_cur_source_index =
        builder->m_cc->AddSourceCodeInfo(pu->source_code_info);
  }

  ~PUPusher() {
    m_builder->m_cur_pu = m_old_pu;
    m_builder->m_cur_source_index = m_old_source_code_index;
  }

 private:
  CompilationUnitBuilder* m_builder;
  SourceCode* m_old_pu;
  uint32_t m_old_source_code_index;
};

void CompilationUnitBuilder::ReportError(const vcl::util::CodeLocation& loc,
                                         const char* format,
                                         ...) {
  va_list vl;
  va_start(vl, format);

  *m_error = vcl::util::ReportErrorV(m_cur_pu->source_code_info->source_code,
                                     loc,
                                     "[compilation-unit]",
                                     format,
                                     vl);
}

vcl::util::FilePathStatus CompilationUnitBuilder::FormatFilePath(
    const std::string& postfix, std::string* output) const {
  vcl::util::FilePathStatus status = vcl::util::GetFilePathStatus(postfix);
  if (status == vcl::util::FILE_PATH_RELATIVE) {
    if (m_folder_hint.empty()) {
      output->assign(postfix);
      return vcl::util::FILE_PATH_RELATIVE;
    } else {
      output->assign(m_folder_hint);
      output->append(postfix);
      return vcl::util::GetFilePathStatus(*output);
    }
  } else if (status == vcl::util::FILE_PATH_ABSOLUTE) {
    if (m_allow_absolute_path) {
      output->assign(postfix);
      return vcl::util::FILE_PATH_ABSOLUTE;
    } else {
      return vcl::util::FILE_PATH_UNKNOWN;
    }
  } else {
    return vcl::util::FILE_PATH_UNKNOWN;
  }
}

CompilationUnit::SubList* CompilationUnitBuilder::AddSub(
    const std::string& sub_name) {
  SubListIndex::iterator itr = m_sub_index.find(sub_name);
  if (itr == m_sub_index.end()) {
    CompilationUnit::SubListPtr sub_list =
        boost::make_shared<CompilationUnit::SubList>();
    m_sub_index.insert(std::make_pair(sub_name, sub_list));
    m_cu->m_statement.push_back(
        CompilationUnit::Statement(sub_list, m_cur_source_index));
    return sub_list.get();
  } else {
    return itr->second.get();
  }
}

bool CompilationUnitBuilder::DoExpand(const File& root) {
  for (size_t i = 0; i < root.chunk->statement_list.size(); ++i) {
    AST* ast = root.chunk->statement_list.Index(i);
    switch (ast->type) {
      case AST_INCLUDE:
        if (!DoInclude(static_cast<const Include&>(*ast))) return false;
        break;
      case AST_SUB:
        if (!DoSub(static_cast<const Sub&>(*ast))) return false;
        break;
      default:
        m_cu->m_statement.push_back(
            CompilationUnit::Statement(ast, m_cur_source_index));
        break;
    }
  }
  return true;
}

bool CompilationUnitBuilder::DoInclude(const Include& inc) {
  if (m_total_include >= m_max_include) {
    ReportError(inc.location,
                "too many include directives, more than %zu",
                m_total_include);
    return false;
  }
  // Do the path expansion and checking
  std::string temp_path(inc.path->ToStdString());
  std::string real_path;
  vcl::util::FilePathStatus status = FormatFilePath(temp_path, &real_path);
  if (status == vcl::util::FILE_PATH_UNKNOWN) {
    ReportError(inc.location,
                "Statement include \"%s\", path is invalid or forbidden!",
                inc.path->data());
    return false;
  }

  {
    // Guard against too many include or nested include
    IncludeGuard inc_guard(this, real_path);
    if (inc_guard) {
      ReportError(inc.location, "circular include found!");
      return false;
    }

    std::string err;
    SourceCode* pu = m_repo->FindOrLoadSourceCode(real_path, &err);
    if (!pu) {
      ReportError(inc.location, "%s", err.c_str());
      return false;
    }
    {
      PUPusher pusher(this, pu);
      return DoExpand(*pu->root);
    }
  }
}

bool CompilationUnitBuilder::DoSub(const Sub& sub) {
  CompilationUnit::SubList* sub_list =
      AddSub(std::string(sub.sub_name->data()));
  if (!sub_list->empty()) {
    // Check argument list to be identical ; otherwise report error
    const Sub* another = sub_list->front().sub;
    bool fail = false;

    for (size_t i = 0; i < another->arg_list.size(); ++i) {
      zone::ZoneString* arg = another->arg_list.Index(i);
      if (i >= sub.arg_list.size() || *arg != *(sub.arg_list.Index(i))) {
        fail = true;
        break;
      }
    }

    if (another->arg_list.size() != sub.arg_list.size()) {
      fail = true;
    }

    if (fail) {
      ReportError(
          sub.location,
          "Sub %s, when merge other same name sub from "
          "source file %s,got different argument list or prototype",
          sub.sub_name->data(),
          m_cc->IndexSourceCodeInfo(m_cur_source_index)->file_path.c_str());
      return false;
    }
  }

  sub_list->push_back(CompilationUnit::SubStatement(&sub,m_cur_source_index));
  return true;
}

}  // namespace detail

void CompilationUnit::Serialize(std::ostream& output) {
  for (size_t i = 0; i < m_statement.size(); ++i) {
    const Statement& stmt = m_statement[i];
    if (stmt.code.which() == STMT_AST) {
      output << *boost::get<const ast::AST*>(stmt.code);
    } else {
      const SubListPtr& p = boost::get<SubListPtr>(stmt.code);
      const SubList& l = *p;
      for (size_t i = 0; i < l.size(); ++i) {
        output << *l[i].sub;
      }
    }
  }
}

bool CompilationUnit::Generate(CompilationUnit* cu,
                               CompiledCode* cc,
                               SourceRepo* repo,
                               size_t max_include,
                               const std::string& folder_hint,
                               bool allow_absolute_path,
                               std::string* error) {
  detail::CompilationUnitBuilder builder(
      cc, cu, repo, max_include, folder_hint, allow_absolute_path, error);

  return builder.Build();
}

}  // namespace vm
}  // namespace vcl

#ifndef COMPILATION_UNIT_H_
#define COMPILATION_UNIT_H_
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "vcl-pri.h"

// The compilation unit is a unit that assemble multiple AST starts from
// File object. The reason why we have this extra layer is because VCL allows
// function concatenation.
//
// In VCL, user is allowed to *include* multiple source file and also in each
// source file, user is also allow to define multiple functions and the function
// could have same name. If mutilple function definition has same function name,
// then the body of them will be concatenated. This semantic make us have to
// have this extra phase which does following preprocessing stuff:
// 1. Expand the include directives
// 2. Reshuffle the constructs to allow code generator(compiler) generate
// correct
//    concatenated function bodies.

namespace vcl {
class Engine;
namespace vm {
namespace ast {
struct File;
struct Sub;
struct AST;
}  // namespace ast

namespace detail {
class CompilationUnitBuilder;
}  // namespace detail

class CompilationUnit {
 public:
  static bool Generate(CompilationUnit* output, CompiledCode* cc,
                       SourceRepo* repo, size_t max_include,
                       const std::string& folder_hint, bool allow_absolute_path,
                       std::string* error);

 public:
  // This list of Sub is used to represent all same name sub entry after
  // the expansion. And it means the code generator needs to concatenate
  // those sub into a large sub.
  typedef std::vector<const ast::Sub*> SubList;
  typedef boost::shared_ptr<SubList> SubListPtr;
  typedef boost::variant<const ast::AST*, SubListPtr> CodeLine;

  // Represents a statement inside of a compilation unit object. Each
  // statement will be assocciated with a source code tag to tell where
  // this statement's source code are. This is for disagnostic reason
  // for virtual machine
  struct Statement {
    uint32_t source_index;
    CodeLine code;
    Statement(uint32_t si) : source_index(si), code() {}

    Statement(const SubListPtr& ptr, uint32_t si)
        : source_index(si), code(ptr) {}

    Statement(const ast::AST* a, uint32_t si) : source_index(si), code(a) {}
  };

 public:
  size_t size() const { return m_statement.size(); }
  bool empty() const { return m_statement.empty(); }

 public:
  // Order matters
  enum { STMT_AST = 0, STMT_SUBLIST };

  Statement& Index(size_t index) { return m_statement[index]; }
  const Statement& Index(size_t index) const { return m_statement[index]; }
  void Serialize(std::ostream& output);

  CompilationUnit() : m_statement() {}

 private:
  std::vector<Statement> m_statement;

  friend class detail::CompilationUnitBuilder;
};

}  // namespace vm
}  // namespace vcl

#endif  // COMPILATION_UNIT_H_

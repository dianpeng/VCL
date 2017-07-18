#ifndef COMPILER_H_
#define COMPILER_H_
#include <string>

namespace vcl {
class Context;
class CompiledCode;
namespace vm {
class CompilationUnit;
namespace zone {
class Zone;
}  // namespace zone

// Compile a CompilationUnit into a Component object which represents a compiled
// VCL source code unit
bool Compile(CompiledCode*, zone::Zone*, const CompilationUnit&, std::string*);

}  // namespace vm
}  // namespace vcl

#endif  // COMPILER_H_

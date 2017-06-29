#ifndef CONSTANT_FOLD_H_
#define CONSTANT_FOLD_H_
#include <string>

namespace vcl {
namespace vm  {

namespace ast {
struct AST;
} // namespace ast

namespace zone {
class Zone;
} // namespace zone

ast::AST* ConstantFold( ast::AST* , zone::Zone* , std::string* error );

} // namespace vm
} // namespace vcl

#endif // CONSTANT_FOLD_H_

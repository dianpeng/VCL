#ifndef CONSTANT_FOLD_H_
#define CONSTANT_FOLD_H_
#include <string>

namespace vcl {
namespace vm  {

namespace ast {
struct Binary;
struct Ternary;
struct AST;
} // namespace ast

namespace zone {
class Zone;
} // namespace zone

ast::AST*  Fold( ast::Binary* , zone::Zone* , std::string* error );
ast::AST*  Fold( ast::Ternary*, zone::Zone* , std::string* error );

} // namespace vm
} // namespace vcl

#endif // CONSTANT_FOLD_H_

#include "constant-fold.h"
#include "ast.h"

#include <boost/variant.hpp>

namespace vcl {
namespace vm  {

namespace {

enum ExpressionKind {
  EKIND_STRING,
  EKIND_INTEGER,
  EKIND_REAL,
  EKIND_BOOLEAN,
  EKIND_NULL,
  EKIND_COMPLEX,
  EKIND_ERROR
};

struct FoldResult {
  ExpressionKind kind;
  boost::variant<zone::ZoneString*,
                 int64_t,
                 double,
                 bool> value;
  vcl::util::CodeLocation location;
  FoldResult():
    kind( EKIND_COMPLEX ),
    value(),
    location()
  {}
};

class ConstantFold {
 public:
  ConstantFold():
    m_zone(NULL),
    m_error(NULL)
  {}

  ast::AST* FoldBinary( ast::Binary* , zone::Zone* , std::string* );
  ast::AST* FoldTernary(ast::Ternary* , zone::Zone* , std::string* );

 private:
  void ReportError( const char* format , ... );
  // Conversion function that is compatible with our runtime's type conversion
  int64_t ToInteger( const FoldResult& value );
  double ToReal   ( const FoldResult& value );
  zone::ZoneString* ToString ( const FoldResult& value );
  bool ToBoolean( const FoldResult& value );

  ast::AST* Fold( ast::Binary* , FoldResult* );
  ast::AST* Fold( ast::Ternary*, FoldResult* );
  ast::AST* Fold( ast::Unary* , FoldResult*  );
  ast::AST* Fold( ast::StringConcat* , FoldResult* );
  ast::AST* Fold( ast::AST* , FoldResult* );
  ast::AST* GenNode( const FoldResult& );
  ExpressionKind ArithPromotion( ExpressionKind , ExpressionKind );

 private:
  zone::Zone* m_zone;
  std::string* m_error;
};

void ConstantFold::ReportError( const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  vcl::util::FormatV(m_error,format,vl);
}

ast::AST* ConstantFold::GenNode( const FoldResult& value ) {
  switch(value.kind) {
    case EKIND_NULL: return new (m_zone) ast::Null(value.location);
    case EKIND_INTEGER:
      return new (m_zone) ast::Integer(value.location,boost::get<int64_t>(value.value));
    case EKIND_REAL:
      return new (m_zone) ast::Real(value.location,boost::get<double>(value.value));
    case EKIND_STRING:
      return new (m_zone) ast::String(value.location,boost::get<zone::ZoneString*>(value.value));
    case EKIND_BOOLEAN:
      return new (m_zone) ast::Boolean(value.location,boost::get<bool>(value.value));
    default:
      VCL_UNREACHABLE();
      return NULL;
  }
}

int64_t ConstantFold::ToInteger( const FoldResult& value ) {
  if(value.kind == EKIND_INTEGER ) {
    return boost::get<int64_t>(value.value);
  } else if(value.kind == EKIND_REAL) {
    return static_cast<int64_t>(boost::get<double>(value.value));
  } else if(value.kind == EKIND_BOOLEAN) {
    return boost::get<bool>(value.value) ? 1 : 0;
  } else {
    VCL_UNREACHABLE();
    return 0;
  }
}

double ConstantFold::ToReal( const FoldResult& value ) {
  if(value.kind == EKIND_REAL) {
    return boost::get<double>(value.value);
  } else if(value.kind == EKIND_INTEGER) {
    return static_cast<double>(boost::get<int64_t>(value.value));
  } else if(value.kind == EKIND_BOOLEAN) {
    return boost::get<bool>(value.value) ? 1.0 : 0.0;
  } else {
    VCL_UNREACHABLE();
    return 0;
  }
}

bool ConstantFold::ToBoolean( const FoldResult& value ) {
  switch(value.kind) {
    case EKIND_INTEGER: return boost::get<int64_t>(value.value) != 0;
    case EKIND_REAL: return boost::get<double>(value.value) != 0.0;
    case EKIND_BOOLEAN: return boost::get<bool>(value.value);
    case EKIND_NULL: return false;
    case EKIND_STRING: return true;
    default: VCL_UNREACHABLE(); return false;
  }
}

zone::ZoneString* ConstantFold::ToString( const FoldResult& value ) {
  if(value.kind == EKIND_STRING) {
    return boost::get<zone::ZoneString*>(value.value);
  } else {
    VCL_UNREACHABLE();
    return NULL;
  }
}


ExpressionKind ConstantFold::ArithPromotion( ExpressionKind left ,
                                             ExpressionKind right ) {
  switch(left) {
    case EKIND_INTEGER:
    case EKIND_BOOLEAN:
      if(right == EKIND_BOOLEAN || right == EKIND_INTEGER)
        return EKIND_INTEGER;
      else if(right == EKIND_REAL)
        return EKIND_REAL;
      else
        return EKIND_ERROR;
    case EKIND_REAL:
      if(right == EKIND_BOOLEAN || right == EKIND_INTEGER ||
         right == EKIND_REAL )
        return EKIND_REAL;
      else
        return EKIND_ERROR;
    default:
      VCL_UNREACHABLE();
      return EKIND_ERROR;
  }
}

ast::AST* ConstantFold::Fold( ast::AST* node , FoldResult* result ) {
  switch(node->type) {
    case ast::AST_BINARY:
      return Fold(static_cast<ast::Binary*>(node),result);
    case ast::AST_UNARY:
      return Fold(static_cast<ast::Unary*>(node), result);
    case ast::AST_TERNARY:
      return Fold(static_cast<ast::Ternary*>(node),result);
    case ast::AST_STRING_CONCAT:
      return Fold(static_cast<ast::StringConcat*>(node),result);
    case ast::AST_INTEGER:
      result->kind = EKIND_INTEGER;
      result->value= static_cast<ast::Integer*>(node)->value;
      result->location = node->location;
      return NULL;
    case ast::AST_REAL:
      result->kind = EKIND_REAL;
      result->value = static_cast<ast::Real*>(node)->value;
      result->location = node->location;
      return NULL;
    case ast::AST_BOOLEAN:
      result->kind = EKIND_BOOLEAN;
      result->value = static_cast<ast::Boolean*>(node)->value;
      result->location = node->location;
      return NULL;
    case ast::AST_NULL:
      result->kind = EKIND_NULL;
      result->location = node->location;
      return NULL;
    case ast::AST_STRING:
      result->kind = EKIND_STRING;
      result->value = static_cast<ast::String*>(node)->value;
      result->location = node->location;
      return NULL;
    default:
      result->kind = EKIND_COMPLEX;
      return node;
  }
}


ast::AST* ConstantFold::Fold( ast::Binary* binary , FoldResult* result ) {
  FoldResult rhs_result;
  ast::AST* lhs = Fold(binary->lhs,result);
  ast::AST* rhs;

  switch(result->kind) {
    case EKIND_ERROR:
      DCHECK(!lhs);
      return NULL;
    case EKIND_COMPLEX:
      DCHECK(lhs);
      binary->lhs = lhs;
      return binary;
    default: break;
  }

  // Now fold the other part
  rhs = Fold(binary->rhs,&rhs_result);

  switch(rhs_result.kind) {
    case EKIND_ERROR:
      DCHECK(!rhs); return NULL;
    case EKIND_COMPLEX:
      DCHECK(rhs);
      binary->lhs = lhs ? lhs : GenNode(*result);
      binary->rhs = rhs;
      return binary;
    default: break;
  }

  if((result->kind == EKIND_NULL ||  rhs_result.kind == EKIND_NULL) &&
     (binary->op == TK_EQ || binary->op == TK_NE )) {
    // null related comparison
    result->kind = EKIND_BOOLEAN;
    if(binary->op == TK_EQ) {
      result->value = (result->kind == EKIND_NULL && rhs_result.kind == EKIND_NULL);
    } else {
      result->value = !(result->kind == EKIND_NULL && rhs_result.kind == EKIND_NULL);
    }
  } else if(result->kind == EKIND_INTEGER || result->kind == EKIND_REAL ||
            result->kind == EKIND_BOOLEAN ) {
    // arithmatic operations
    ExpressionKind type = ArithPromotion(result->kind,rhs_result.kind);
    if(type == EKIND_ERROR) {
      ReportError("type mismatch,cannot perform arithmatic/comparison operation!");
      result->kind = EKIND_ERROR;
      return NULL;
    }

    if(type == EKIND_INTEGER) {
      int64_t lval = ToInteger(*result);
      int64_t rval = ToInteger(rhs_result);
      result->kind = EKIND_INTEGER;
      switch(binary->op) {
        case TK_ADD: result->value = (lval + rval); break;
        case TK_SUB: result->value = (lval - rval); break;
        case TK_MUL: result->value = (lval * rval); break;
        case TK_DIV:
          if(!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval / rval); break;
        case TK_MOD:
          if(!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval % rval); break;
        default:
          result->kind = EKIND_BOOLEAN;
          switch(binary->op) {
            case TK_LT: result->value = (lval < rval); break;
            case TK_LE: result->value = (lval <=rval); break;
            case TK_GT: result->value = (lval > rval); break;
            case TK_GE: result->value = (lval >=rval); break;
            case TK_EQ: result->value = (lval ==rval); break;
            case TK_NE: result->value = (lval !=rval); break;
            default:
              ReportError("type mistmatch,cannot perform match/not-match operation!");
              result->kind = EKIND_ERROR;
              return NULL;
          }
      }
    } else {
      DCHECK(type == EKIND_REAL);
      double lval = ToReal(*result);
      double rval = ToReal(rhs_result);
      result->kind = EKIND_REAL;
      switch(binary->op) {
        case TK_ADD: result->value = (lval + rval); break;
        case TK_SUB: result->value = (lval - rval); break;
        case TK_MUL: result->value = (lval * rval); break;
        case TK_DIV:
          if(!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval / rval); break;
        case TK_MOD:
          ReportError("type mistmatch,real type cannot perform % operation!");
          result->kind = EKIND_ERROR;
          return NULL;
        default:
          result->kind = EKIND_BOOLEAN;
          switch(binary->op) {
            case TK_LT: result->value = (lval < rval); break;
            case TK_LE: result->value = (lval <=rval); break;
            case TK_GT: result->value = (lval > rval); break;
            case TK_GE: result->value = (lval >=rval); break;
            case TK_EQ: result->value = (lval ==rval); break;
            case TK_NE: result->value = (lval !=rval); break;
            default:
              ReportError("type mistmatch,cannot perform match/not-match operation!");
              result->kind = EKIND_ERROR;
              return NULL;
          }
      }
    }
  } else if(result->kind == EKIND_STRING && rhs_result.kind == EKIND_STRING) {
    zone::ZoneString* lval = ToString(*result);
    zone::ZoneString* rval = ToString(rhs_result);
    result->kind = EKIND_BOOLEAN;
    switch(binary->op) {
      case TK_LT: result->value = (*lval < *rval); break;
      case TK_LE: result->value = (*lval <=*rval); break;
      case TK_GT: result->value = (*lval > *rval); break;
      case TK_GE: result->value = (*lval >=*rval); break;
      case TK_EQ: result->value = (*lval ==*rval); break;
      case TK_NE: result->value = (*lval !=*rval); break;
      case TK_MATCH: case TK_NOT_MATCH:
        // Currently don't fold regex matching
        result->kind = EKIND_COMPLEX;
        return binary;
      default:
        ReportError("type mistmatch,cannot perform arithmatic operation on string!");
        result->kind = EKIND_ERROR;
        return NULL;
    }
  } else {
    binary->lhs = lhs ? lhs : GenNode(*result);
    binary->rhs = rhs ? rhs : GenNode(rhs_result);
    result->kind = EKIND_COMPLEX;
    return binary;
  }

  result->location = binary->location;
  return NULL;
}

ast::AST* ConstantFold::Fold( ast::Unary* node , FoldResult* result ) {
  ast::AST* operand = Fold(node->operand,result);
  switch(result->kind) {
    case EKIND_ERROR:
      return NULL;
    case EKIND_COMPLEX:
      DCHECK(operand);
      node->operand = operand;
      return node;
    default:
      break;
  }

  bool flip = false;

  switch(result->kind) {
    case EKIND_BOOLEAN:
    case EKIND_INTEGER: {
      int64_t v;
      if(result->kind == EKIND_INTEGER)
        v = boost::get<int64_t>(result->value);
      else
        v = boost::get<bool>(result->value) ? 1 : 0;

      for( size_t i = 0 ; i < node->ops.size() ; ++i ) {
        switch( node->ops[i] ) {
          case TK_ADD: flip = false; break;
          case TK_SUB: flip = false; v = -v; break;
          case TK_NOT: flip = true ; v = !v; break;
          default: VCL_UNREACHABLE(); break;
        }
      }

      if(flip) {
        result->kind = EKIND_BOOLEAN;
        result->value= static_cast<bool>(v);
      } else {
        result->kind = EKIND_INTEGER;
        result->value= v;
      }
    }
    break;
    case EKIND_REAL: {
      double v = boost::get<double>(result->value);

      for( size_t i = 0 ; i < node->ops.size(); ++i ) {
        switch(node->ops[i]) {
          case TK_ADD: flip = false; break;
          case TK_SUB: flip = false; v = -v; break;
          case TK_NOT: flip = true ; v = !v; break;
          default: VCL_UNREACHABLE(); break;
        }
      }

      if(flip) {
        result->kind = EKIND_BOOLEAN;
        result->value= static_cast<bool>(v);
      } else {
        result->kind = EKIND_INTEGER;
        result->value= v;
      }
    }
    break;
    case EKIND_NULL: {
      if(node->ops[0] != TK_NOT) {
        ReportError("null type's unary operator can only be \"!\"");
        result->kind = EKIND_ERROR;
        return NULL;
      } else {
        int64_t v = 1;
        for( size_t i = 1 ; i < node->ops.size(); ++i ) {
          switch(node->ops[i]) {
            case TK_ADD: flip = false; break;
            case TK_SUB: flip = false; v = -v; break;
            case TK_NOT: flip = true; v = !v; break;
            default: VCL_UNREACHABLE(); break;
          }
        }
        if(flip) {
          result->kind = EKIND_BOOLEAN;
          result->value= static_cast<bool>(v);
        } else {
          result->kind = EKIND_INTEGER;
          result->value= v;
        }
      }
    }
    break;
    default:
      ReportError("string type cannot be work with unary operator!");
      result->kind = EKIND_ERROR;
      return NULL;
  }

  result->location = node->location;
  return NULL;
}

ast::AST* ConstantFold::Fold( ast::StringConcat* node , FoldResult* result ) {
  std::string buffer;
  buffer.reserve(128);
  for( size_t i = 0 ; i < node->list.size(); ++i ) {
    buffer.append( node->list[i]->data() );
  }
  result->location = node->location;
  result->kind = EKIND_STRING;
  result->value = zone::ZoneString::New(m_zone,buffer);
  return NULL;
}

ast::AST* ConstantFold::Fold( ast::Ternary* node , FoldResult* result ) {
  ast::AST* cond = Fold(node->condition,result);
  switch(result->kind) {
    case EKIND_ERROR: return NULL;
    case EKIND_COMPLEX:
      DCHECK(cond);
      node->condition = cond;
      return node;
    default:
      break;
  }
  bool condition = ToBoolean(*result);
  if(condition) {
    FoldResult first_result;
    ast::AST* first = Fold(node->first,&first_result);
    if(first_result.kind == EKIND_ERROR)
      return NULL;
    else if(first_result.kind == EKIND_COMPLEX)
      return first;
    else
      return NULL;
  } else {
    FoldResult second_result;
    ast::AST* second = Fold(node->second,&second_result);
    if(second_result.kind == EKIND_ERROR)
      return NULL;
    else if(second_result.kind == EKIND_COMPLEX)
      return second;
    else
      return NULL;
  }
}

ast::AST* ConstantFold::FoldBinary( ast::Binary* node , zone::Zone* zone ,
                                                        std::string* error ) {
  m_zone = zone;
  m_error= error;
  FoldResult result;
  ast::AST* ret = Fold(node,&result);

  switch(result.kind) {
    case EKIND_ERROR: return NULL;
    case EKIND_REAL:
    case EKIND_INTEGER:
    case EKIND_STRING:
    case EKIND_NULL:
    case EKIND_BOOLEAN:
      return GenNode(result);
    default:
      DCHECK(ret);
      return ret;
  }
}

ast::AST* ConstantFold::FoldTernary( ast::Ternary* node , zone::Zone* zone ,
                                                          std::string* error ) {
  m_zone = zone;
  m_error= error;

  FoldResult result;
  ast::AST* ret = Fold(node,&result);

  switch(result.kind) {
    case EKIND_ERROR: return NULL;
    case EKIND_REAL:
    case EKIND_INTEGER:
    case EKIND_STRING:
    case EKIND_BOOLEAN:
      return GenNode(result);
    default:
      DCHECK(ret);
      return ret;
  }
}

} // namespace


ast::AST*  Fold( ast::Binary* node , zone::Zone* zone , std::string* error ) {
  ConstantFold folder;
  return folder.FoldBinary(node,zone,error);
}

ast::AST*  Fold( ast::Ternary* node , zone::Zone* zone , std::string* error ) {
  ConstantFold folder;
  return folder.FoldTernary(node,zone,error);
}

} // namespace vm
} // namespace vcl

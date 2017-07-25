#include "constant-fold.h"
#include "ast.h"

#include <boost/variant.hpp>

namespace vcl {
namespace vm {

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
  boost::variant<zone::ZoneString*, int32_t, double, bool> value;
  vcl::util::CodeLocation location;
  FoldResult() : kind(EKIND_COMPLEX), value(), location() {}

  zone::ZoneString* string() const {
    return boost::get<zone::ZoneString*>(value);
  }
  int32_t integer() const { return boost::get<int32_t>(value); }
  double real() const { return boost::get<double>(value); }
  bool boolean() const { return boost::get<bool>(value); }
};

class ConstantFolder {
 public:
  ConstantFolder() : m_zone(NULL), m_error(NULL) {}

  ast::AST* Fold(ast::AST*, zone::Zone*, std::string*);

 private:
  void ReportError(const char* format, ...);

  int32_t ToInteger(const FoldResult& value);
  double ToReal(const FoldResult& value);
  zone::ZoneString* ToString(const FoldResult& value);
  bool ToBoolean(const FoldResult& value);

  ast::AST* Fold(ast::Binary*, FoldResult*);
  ast::AST* Fold(ast::Ternary*, FoldResult*);
  ast::AST* Fold(ast::Unary*, FoldResult*);
  ast::AST* Fold(ast::StringConcat*, FoldResult*);
  ast::AST* Fold(ast::StringInterpolation*, FoldResult*);
  ast::AST* Fold(ast::AST*, FoldResult*);
  ast::AST* GenNode(const FoldResult&);
  ExpressionKind ArithPromotion(ExpressionKind, ExpressionKind);

 private:
  zone::Zone* m_zone;
  std::string* m_error;
};

void ConstantFolder::ReportError(const char* format, ...) {
  va_list vl;
  va_start(vl, format);
  vcl::util::FormatV(m_error, format, vl);
}

ast::AST* ConstantFolder::GenNode(const FoldResult& value) {
  switch (value.kind) {
    case EKIND_NULL:
      return new (m_zone) ast::Null(value.location);
    case EKIND_INTEGER:
      return new (m_zone) ast::Integer(value.location, value.integer());
    case EKIND_REAL:
      return new (m_zone) ast::Real(value.location, value.real());
    case EKIND_STRING:
      return new (m_zone) ast::String(value.location, value.string());
    case EKIND_BOOLEAN:
      return new (m_zone) ast::Boolean(value.location, value.boolean());
    default:
      VCL_UNREACHABLE();
      return NULL;
  }
}

int32_t ConstantFolder::ToInteger(const FoldResult& value) {
  if (value.kind == EKIND_INTEGER) {
    return value.integer();
  } else if (value.kind == EKIND_REAL) {
    return static_cast<int32_t>(value.real());
  } else if (value.kind == EKIND_BOOLEAN) {
    return static_cast<int32_t>(value.boolean());
  } else {
    VCL_UNREACHABLE();
    return 0;
  }
}

double ConstantFolder::ToReal(const FoldResult& value) {
  if (value.kind == EKIND_REAL) {
    return value.real();
  } else if (value.kind == EKIND_INTEGER) {
    return static_cast<double>(value.integer());
  } else if (value.kind == EKIND_BOOLEAN) {
    return static_cast<double>(value.boolean());
  } else {
    VCL_UNREACHABLE();
    return 0;
  }
}

bool ConstantFolder::ToBoolean(const FoldResult& value) {
  switch (value.kind) {
    case EKIND_INTEGER:
      return value.integer() != 0;
    case EKIND_REAL:
      return value.real() != 0.0;
    case EKIND_BOOLEAN:
      return value.boolean();
    case EKIND_NULL:
      return false;
    case EKIND_STRING:
      return true;
    default:
      VCL_UNREACHABLE();
      return false;
  }
}

zone::ZoneString* ConstantFolder::ToString(const FoldResult& value) {
  if (value.kind == EKIND_STRING) {
    return value.string();
  } else {
    VCL_UNREACHABLE();
    return NULL;
  }
}

ExpressionKind ConstantFolder::ArithPromotion(ExpressionKind left,
                                              ExpressionKind right) {
  switch (left) {
    case EKIND_INTEGER:
    case EKIND_BOOLEAN:
      if (right == EKIND_BOOLEAN || right == EKIND_INTEGER)
        return EKIND_INTEGER;
      else if (right == EKIND_REAL)
        return EKIND_REAL;
      else
        return EKIND_ERROR;
    case EKIND_REAL:
      if (right == EKIND_BOOLEAN || right == EKIND_INTEGER ||
          right == EKIND_REAL)
        return EKIND_REAL;
      else
        return EKIND_ERROR;
    default:
      VCL_UNREACHABLE();
      return EKIND_ERROR;
  }
}

ast::AST* ConstantFolder::Fold(ast::AST* node, FoldResult* result) {
  switch (node->type) {
    case ast::AST_BINARY:
      return Fold(static_cast<ast::Binary*>(node), result);
    case ast::AST_UNARY:
      return Fold(static_cast<ast::Unary*>(node), result);
    case ast::AST_TERNARY:
      return Fold(static_cast<ast::Ternary*>(node), result);
    case ast::AST_STRING_CONCAT:
      return Fold(static_cast<ast::StringConcat*>(node), result);
    case ast::AST_STRING_INTERPOLATION:
      return Fold(static_cast<ast::StringInterpolation*>(node), result);
    case ast::AST_INTEGER:
      result->kind = EKIND_INTEGER;
      result->value = static_cast<ast::Integer*>(node)->value;
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

ast::AST* ConstantFolder::Fold(ast::Binary* binary, FoldResult* result) {
  FoldResult rhs_result;
  ast::AST* lhs;
  ast::AST* rhs;

  // 1. Folding the left hand side
  lhs = Fold(binary->lhs, result);
  switch (result->kind) {
    case EKIND_ERROR:
      DCHECK(!lhs);
      return NULL;
    case EKIND_COMPLEX:
      DCHECK(lhs);
      binary->lhs = lhs;
      {
        // Here we still try to fold the right hand side value, though we know
        // this node cannot be evaluated to constant value, but at least we can
        // simpify our right hand side operand
        ast::AST* rhs = Fold(binary->rhs, result);
        if (result->kind == EKIND_ERROR) return NULL;
        binary->rhs = rhs ? rhs : GenNode(*result);
      }
      result->kind = EKIND_COMPLEX;
      return binary;
    default:
      break;
  }

  switch (binary->op) {
    case TK_AND: {
      bool lhs = ToBoolean(*result);
      if (!lhs) {
        result->kind = EKIND_BOOLEAN;
        result->value = false;
        result->location = binary->location;
        return NULL;
      }
    } break;
    case TK_OR: {
      bool lhs = ToBoolean(*result);
      if (lhs) {
        result->kind = EKIND_BOOLEAN;
        result->value = true;
        result->location = binary->location;
        return NULL;
      }
    } break;
    default:
      break;
  }

  // 2. Folding the right hand side value
  rhs = Fold(binary->rhs, &rhs_result);

  switch (rhs_result.kind) {
    case EKIND_ERROR:
      DCHECK(!rhs);
      return NULL;
    case EKIND_COMPLEX:
      DCHECK(rhs);
      binary->lhs = lhs ? lhs : GenNode(*result);
      binary->rhs = rhs;
      result->kind = EKIND_COMPLEX;  // Change type for the caller
      return binary;
    default:
      break;
  }

  switch (binary->op) {
    case TK_AND: {
      bool lhs = ToBoolean(*result);
      bool rhs = ToBoolean(rhs_result);
      result->kind = EKIND_BOOLEAN;
      result->value = (lhs && rhs);
      return NULL;
    }
    case TK_OR: {
      bool lhs = ToBoolean(*result);
      bool rhs = ToBoolean(rhs_result);
      result->kind = EKIND_BOOLEAN;
      result->value = (lhs || rhs);
      return NULL;
    }
    default:
      break;
  }

  // 3. If applicable, do the actual binary constant fold
  if ((result->kind == EKIND_NULL || rhs_result.kind == EKIND_NULL) &&
      (binary->op == TK_EQ || binary->op == TK_NE)) {
    result->kind = EKIND_BOOLEAN;
    if (binary->op == TK_EQ) {
      result->value =
          (result->kind == EKIND_NULL && rhs_result.kind == EKIND_NULL);
    } else {
      result->value =
          !(result->kind == EKIND_NULL && rhs_result.kind == EKIND_NULL);
    }
  } else if (result->kind == EKIND_INTEGER || result->kind == EKIND_REAL ||
             result->kind == EKIND_BOOLEAN) {
    ExpressionKind type = ArithPromotion(result->kind, rhs_result.kind);
    if (type == EKIND_ERROR) {
      ReportError(
          "type mismatch,cannot perform arithmatic/comparison operation!");
      result->kind = EKIND_ERROR;
      return NULL;
    }

    if (type == EKIND_INTEGER) {
      int32_t lval = ToInteger(*result);
      int32_t rval = ToInteger(rhs_result);
      result->kind = EKIND_INTEGER;
      switch (binary->op) {
        case TK_ADD:
          result->value = (lval + rval);
          break;
        case TK_SUB:
          result->value = (lval - rval);
          break;
        case TK_MUL:
          result->value = (lval * rval);
          break;
        case TK_DIV:
          if (!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval / rval);
          break;
        case TK_MOD:
          if (!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval % rval);
          break;
        default:
          result->kind = EKIND_BOOLEAN;
          switch (binary->op) {
            case TK_LT:
              result->value = (lval < rval);
              break;
            case TK_LE:
              result->value = (lval <= rval);
              break;
            case TK_GT:
              result->value = (lval > rval);
              break;
            case TK_GE:
              result->value = (lval >= rval);
              break;
            case TK_EQ:
              result->value = (lval == rval);
              break;
            case TK_NE:
              result->value = (lval != rval);
              break;
            default:
              ReportError(
                  "type mistmatch,cannot perform match/not-match operation!");
              result->kind = EKIND_ERROR;
              return NULL;
          }
      }
    } else {
      DCHECK(type == EKIND_REAL);
      double lval = ToReal(*result);
      double rval = ToReal(rhs_result);
      result->kind = EKIND_REAL;
      switch (binary->op) {
        case TK_ADD:
          result->value = (lval + rval);
          break;
        case TK_SUB:
          result->value = (lval - rval);
          break;
        case TK_MUL:
          result->value = (lval * rval);
          break;
        case TK_DIV:
          if (!rval) {
            ReportError("devide zero!");
            result->kind = EKIND_ERROR;
            return NULL;
          }
          result->value = (lval / rval);
          break;
        case TK_MOD:
          ReportError("type mistmatch,real type cannot perform % operation!");
          result->kind = EKIND_ERROR;
          return NULL;
        default:
          result->kind = EKIND_BOOLEAN;
          switch (binary->op) {
            case TK_LT:
              result->value = (lval < rval);
              break;
            case TK_LE:
              result->value = (lval <= rval);
              break;
            case TK_GT:
              result->value = (lval > rval);
              break;
            case TK_GE:
              result->value = (lval >= rval);
              break;
            case TK_EQ:
              result->value = (lval == rval);
              break;
            case TK_NE:
              result->value = (lval != rval);
              break;
            default:
              ReportError(
                  "type mistmatch,cannot perform match/not-match operation!");
              result->kind = EKIND_ERROR;
              return NULL;
          }
      }
    }
  } else if (result->kind == EKIND_STRING && rhs_result.kind == EKIND_STRING) {
    zone::ZoneString* lval = ToString(*result);
    zone::ZoneString* rval = ToString(rhs_result);
    result->kind = EKIND_BOOLEAN;
    switch (binary->op) {
      case TK_ADD: {
        std::string l(lval->data());
        l.append(rval->data());
        result->value = zone::ZoneString::New(m_zone, l);
        result->kind = EKIND_STRING;
      } break;
      case TK_LT:
        result->value = (*lval < *rval);
        break;
      case TK_LE:
        result->value = (*lval <= *rval);
        break;
      case TK_GT:
        result->value = (*lval > *rval);
        break;
      case TK_GE:
        result->value = (*lval >= *rval);
        break;
      case TK_EQ:
        result->value = (*lval == *rval);
        break;
      case TK_NE:
        result->value = (*lval != *rval);
        break;
      case TK_MATCH:
      case TK_NOT_MATCH:
        // Currently don't fold regex matching
        result->kind = EKIND_COMPLEX;
        return binary;
      default:
        ReportError(
            "type mistmatch,cannot perform arithmatic operation on string!");
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

ast::AST* ConstantFolder::Fold(ast::Unary* node, FoldResult* result) {
  ast::AST* operand = Fold(node->operand, result);
  switch (result->kind) {
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

  switch (result->kind) {
    case EKIND_BOOLEAN:
    case EKIND_INTEGER: {
      int32_t v;
      if (result->kind == EKIND_INTEGER)
        v = result->integer();
      else
        v = static_cast<int32_t>(result->boolean());

      for (size_t i = 0; i < node->ops.size(); ++i) {
        switch (node->ops[i]) {
          case TK_ADD:
            flip = false;
            break;
          case TK_SUB:
            flip = false;
            v = -v;
            break;
          case TK_NOT:
            flip = true;
            v = !v;
            break;
          default:
            VCL_UNREACHABLE();
            break;
        }
      }

      if (flip) {
        result->kind = EKIND_BOOLEAN;
        result->value = static_cast<bool>(v);
      } else {
        result->kind = EKIND_INTEGER;
        result->value = v;
      }
    } break;
    case EKIND_REAL: {
      double v = result->real();

      for (size_t i = 0; i < node->ops.size(); ++i) {
        switch (node->ops[i]) {
          case TK_ADD:
            flip = false;
            break;
          case TK_SUB:
            flip = false;
            v = -v;
            break;
          case TK_NOT:
            flip = true;
            v = !v;
            break;
          default:
            VCL_UNREACHABLE();
            break;
        }
      }

      if (flip) {
        result->kind = EKIND_BOOLEAN;
        result->value = static_cast<bool>(v);
      } else {
        result->kind = EKIND_REAL;
        result->value = v;
      }
    } break;
    case EKIND_NULL: {
      if (node->ops[0] != TK_NOT) {
        ReportError("null type's unary operator can only be \"!\"");
        result->kind = EKIND_ERROR;
        return NULL;
      } else {
        int32_t v = 1;
        for (size_t i = 1; i < node->ops.size(); ++i) {
          switch (node->ops[i]) {
            case TK_ADD:
              flip = false;
              break;
            case TK_SUB:
              flip = false;
              v = -v;
              break;
            case TK_NOT:
              flip = true;
              v = !v;
              break;
            default:
              VCL_UNREACHABLE();
              break;
          }
        }
        if (flip) {
          result->kind = EKIND_BOOLEAN;
          result->value = static_cast<bool>(v);
        } else {
          result->kind = EKIND_INTEGER;
          result->value = v;
        }
      }
    } break;
    default:
      ReportError("string type cannot be work with unary operator!");
      result->kind = EKIND_ERROR;
      return NULL;
  }

  result->location = node->location;
  return NULL;
}

ast::AST* ConstantFolder::Fold(ast::StringConcat* node, FoldResult* result) {
  std::string buffer;
  buffer.reserve(128);
  for (size_t i = 0; i < node->list.size(); ++i) {
    buffer.append(node->list[i]->data());
  }
  result->location = node->location;
  result->kind = EKIND_STRING;
  result->value = zone::ZoneString::New(m_zone, buffer);
  return NULL;
}

ast::AST* ConstantFolder::Fold(ast::StringInterpolation* node, FoldResult* result) {
  std::string buffer;
  buffer.reserve( 1024 );
  vcl::util::CodeLocation last_loc = node->location;
  zone::ZoneVector<ast::AST*> rset;

  for( size_t i = 0 ; i < node->list.size() ; ++i ) {
    ast::AST& n = *node->list.Index(i);
    if(n.type == ast::AST_STRING) {
      // Now we append the string literal into our buffer
      buffer.append( static_cast<const ast::String&>(n).value->data() );
      last_loc = n.location;
    } else {
      if(!buffer.empty()) {
        rset.Add(m_zone,
                 new (m_zone) ast::String(last_loc,zone::ZoneString::New(m_zone,buffer)));
        buffer.clear();
      }
      rset.Add(m_zone,&n);
    }
  }

  if(!rset.empty()) {
    if(!buffer.empty()) {
      rset.Add(m_zone,
               new (m_zone) ast::String(last_loc,zone::ZoneString::New(m_zone,buffer)));
    }
    result->location = node->location;
    result->kind = EKIND_COMPLEX;
    node->list.Swap( &rset );
    return node; // Complex result , cannot be folded anymore
  } else {
    DCHECK( !buffer.empty() );
    result->kind = EKIND_STRING;
    result->location = node->location;
    result->value = zone::ZoneString::New(m_zone,buffer);
    return NULL; // Folded result
  }
}

ast::AST* ConstantFolder::Fold(ast::Ternary* node, FoldResult* result) {
  ast::AST* cond = Fold(node->condition, result);
  switch (result->kind) {
    case EKIND_ERROR:
      return NULL;
    case EKIND_COMPLEX:
      DCHECK(cond);
      node->condition = cond;
      {
        ast::AST* first;
        ast::AST* second;
        first = Fold(node->first, result);
        if (result->kind == EKIND_ERROR) return NULL;
        node->first = first ? first : GenNode(*result);
        second = Fold(node->second, result);
        if (result->kind == EKIND_ERROR) return NULL;
        node->second = second ? second : GenNode(*result);
      }
      result->kind = EKIND_COMPLEX;
      return node;
    default:
      break;
  }

  bool condition = ToBoolean(*result);
  if (condition) {
    FoldResult first_result;
    ast::AST* first = Fold(node->first, &first_result);
    if (first_result.kind == EKIND_ERROR)
      return NULL;
    else if (first_result.kind == EKIND_COMPLEX)
      return first;
    else
      return NULL;
  } else {
    FoldResult second_result;
    ast::AST* second = Fold(node->second, &second_result);
    if (second_result.kind == EKIND_ERROR)
      return NULL;
    else if (second_result.kind == EKIND_COMPLEX)
      return second;
    else
      return NULL;
  }
}

ast::AST* ConstantFolder::Fold(ast::AST* node,
                               zone::Zone* zone,
                               std::string* error) {
  m_zone = zone;
  m_error = error;
  FoldResult result;
  ast::AST* ret = Fold(node, &result);

  switch (result.kind) {
    case EKIND_ERROR:
      return NULL;
    case EKIND_REAL:
    case EKIND_INTEGER:
    case EKIND_STRING:
    case EKIND_NULL:
    case EKIND_BOOLEAN:
      DCHECK(!ret);
      return GenNode(result);
    default:
      DCHECK(ret);
      return ret;
  }
}

}  // namespace

ast::AST* ConstantFold(ast::AST* node, zone::Zone* zone, std::string* error) {
  if (node) {
    ConstantFolder folder;
    return folder.Fold(node, zone, error);
  } else
    return NULL;
}

}  // namespace vm
}  // namespace vcl

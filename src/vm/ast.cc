#include "ast.h"

namespace vcl {
namespace vm {
namespace ast {
namespace {

const char* kIndent = "  ";

// Forward
#define __(A, B, C) std::ostream& Serialize(const B&, int, std::ostream*);
VCL_AST_TYPE(__)
#undef __  // __

std::ostream& Serialize(const AST& node, int indent, std::ostream* output) {
  switch (node.type) {
#define __(A, B, C) \
  case AST_##A:     \
    return Serialize(static_cast<const B&>(node), indent, output);

    VCL_AST_TYPE(__)

#undef __  // __
    default:
      VCL_UNREACHABLE();
      return *output;
  }
}

std::ostream& Indent(std::ostream& output, int indent) {
  for (int i = 0; i < indent; ++i) {
    output << kIndent;
  }
  return output;
}

std::ostream& Serialize(const Include& inc, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << "include " << '"' << (*(inc.path)) << "\";\n";
}

std::ostream& Serialize(const Import& import, int indent,
                        std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << "import " << '"' << (*(import.module_name)) << "\";\n";
}

std::ostream& Serialize(const Chunk& chunk, int indent, std::ostream* output) {
  Indent(*output, indent) << "{\n";
  for (size_t i = 0; i < chunk.statement_list.size(); ++i) {
    AST* expr = chunk.statement_list[i];
    Serialize(*expr, indent + 1, output);
  }
  return Indent(*output, indent) << "}\n";
}

std::ostream& Serialize(const LexScope& lscope, int indent,
                        std::ostream* output) {
  return Serialize(*lscope.body, indent, output);
}

std::ostream& Serialize(const Sub& sub, int indent, std::ostream* output) {
  Indent(*output, indent) << "sub " << (*sub.sub_name);
  {  // Argument list
    *output << "(";
    for (size_t i = 0; i < sub.arg_list.size(); ++i) {
      *output << (*sub.arg_list[i]);
      if (i < sub.arg_list.size() - 1) *output << ",";
    }
    *output << ")\n";
  }
  return Serialize(*sub.body, indent, output);
}

std::ostream& Serialize(const ExtensionInitializer& ini, int indent,
                        std::ostream* output) {
  *output << "{\n";
  for (size_t i = 0; i < ini.list.size(); ++i) {
    const ExtensionInitializer::ExtensionField& f = ini.list[i];
    Indent(*output, indent + 1) << '.' << (*f.name) << " = ";
    Serialize(*f.value, indent + 1, output) << ";\n";
  }
  return Indent(*output, indent) << "}";
}

std::ostream& Serialize(const ExtensionLiteral& literal, int indent,
                        std::ostream* output) {
  *output << (*literal.type_name) << ' ';
  return Serialize(*literal.initializer, indent, output);
}

std::ostream& Serialize(const Extension& ext, int indent,
                        std::ostream* output) {
  Indent(*output, indent) << (*ext.type_name) << " " << (*ext.instance_name);
  return Serialize(*ext.initializer, indent, output) << '\n';
}

std::ostream& Serialize(const Dict& obj, int indent, std::ostream* output) {
  *output << '{';
  for (size_t i = 0; i < obj.list.size(); ++i) {
    const Dict::Entry& e = obj.list[i];
    Serialize(*e.key, indent + 1, output) << " : ";
    Serialize(*e.value, indent + 1, output);
    if (i < obj.list.size() - 1) *output << ',';
  }
  *output << '}';
  return *output;
}

std::ostream& Serialize(const ACL& acl, int indent, std::ostream* output) {
  Indent(*output, indent) << "acl " << *(acl.name) << " {\n";
  for (size_t i = 0; i < acl.list.size(); ++i) {
    const ACL::ACLItem& item = acl.list[i];
    Indent(*output, indent + 1);
    if (item.negative) *output << '!';
    *output << '"' << *(acl.name) << '"';
    if (item.mask) *output << '/' << item.mask;
    *output << ";\n";
  }
  return Indent(*output, indent) << "}\n";
}

std::ostream& Serialize(const Global& g, int indent, std::ostream* output) {
  Indent(*output, indent) << "global " << *(g.name) << " = ";
  return Serialize(*(g.value), indent, output) << ";\n";
}

std::ostream& Serialize(const List& l, int indent, std::ostream* output) {
  *output << '[';
  for (size_t i = 0; i < l.list.size(); ++i) {
    Serialize(*l.list[i], indent, output);
    if (i < l.list.size() - 1) *output << ',';
  }
  return *output << ']';
}

std::ostream& Serialize(const Return& r, int indent, std::ostream* output) {
  Indent(*output, indent) << "return ";
  if (r.value) {
    *output << '{';
    Serialize(*r.value, indent, output);
    *output << '}';
  }
  return *output << ";\n";
}

std::ostream& Serialize(const Terminate& t, int indent, std::ostream* output) {
  Indent(*output, indent) << "return (";
  if (t.value) {
    CHECK(t.action == vcl::ACT_EXTENSION);
    Serialize(*t.value, indent, output);
  } else {
    *output << GetActionName(t.action);
  }
  return *output << ");\n";
}

std::ostream& Serialize(const LeftHandSide& lhs, int indent,
                        std::ostream* output) {
  if (lhs.type == LeftHandSide::VARIABLE) {
    *output << *(lhs.variable);
  } else {
    Serialize(*lhs.prefix, indent, output);
  }
  return *output;
}

std::ostream& Serialize(const Set& s, int indent, std::ostream* output) {
  Indent(*output, indent) << "set ";
  Serialize(s.lhs, indent, output);
  *output << GetTokenName(s.op);
  return Serialize(*s.rhs, indent, output) << ";\n";
}

std::ostream& Serialize(const Unset& u, int indent, std::ostream* output) {
  Indent(*output, indent) << "unset ";
  return Serialize(u.lhs, indent, output) << ";\n";
}

std::ostream& Serialize(const Declare& d, int indent, std::ostream* output) {
  Indent(*output, indent) << "declare " << *(d.variable);
  if (d.rhs) {
    *output << " = ";
    return Serialize(*d.rhs, indent, output) << ";\n";
  } else {
    *output << ";\n";
  }
  return *output;
}

std::ostream& Serialize(const char* prefix, const If::Branch& br, int indent,
                        std::ostream* output) {
  Indent(*output, indent) << prefix << '(';
  Serialize(*br.condition, indent, output) << ")\n";
  return Serialize(*br.body, indent, output);
}

std::ostream& Serialize(const If& i, int indent, std::ostream* output) {
  Serialize("if", i.branch_list[0], indent, output);
  for (size_t k = 1; k < i.branch_list.size() - 1; ++k) {
    Serialize("elif", i.branch_list[k], indent, output);
  }
  if (i.branch_list.size() > 1) {
    if (i.branch_list.Last().condition) {
      Serialize("elif", i.branch_list.Last(), indent, output);
    } else {
      Indent(*output, indent) << "else \n";
      Serialize(*i.branch_list.Last().body, indent, output);
    }
  }
  return *output;
}

std::ostream& Serialize(const Break& node, int indent, std::ostream* output) {
  return Indent(*output, indent) << "break;\n";
}

std::ostream& Serialize(const Continue& node, int indent,
                        std::ostream* output) {
  return Indent(*output, indent) << "continue;\n";
}

std::ostream& Serialize(const For& node, int indent, std::ostream* output) {
  Indent(*output, indent) << "for( ";

  if (node.key)
    (*output) << node.key->data();
  else
    (*output) << '_';
  (*output) << ',';

  if (node.val)
    (*output) << node.val->data();
  else
    (*output) << '_';
  (*output) << ':';

  Serialize(*node.iterator, indent, output);
  (*output) << ")\n";
  return Serialize(*node.body, indent, output);
}

std::ostream& Serialize(const Stmt& s, int indent, std::ostream* output) {
  Indent(*output, indent);
  return Serialize(*s.expr, indent, output) << ";\n";
}

std::ostream& Serialize(const Ternary& t, int indent, std::ostream* output) {
  *output << "if(";
  Serialize(*t.condition, indent, output) << ',';
  Serialize(*t.first, indent, output) << ',';
  return Serialize(*t.second, indent, output) << ')';
}

std::ostream& Serialize(const Binary& b, int indent, std::ostream* output) {
  *output << '(';
  Serialize(*b.lhs, indent, output);
  *output << GetTokenName(b.op);
  return Serialize(*b.rhs, indent, output) << ')';
}

std::ostream& Serialize(const Unary& u, int indent, std::ostream* output) {
  for (size_t i = 0; i < u.ops.size(); ++i) {
    *output << GetTokenName(u.ops[i]);
  }
  return Serialize(*u.operand, indent, output);
}

std::ostream& Serialize(const Prefix& p, int indent, std::ostream* output) {
  CHECK(p.list.First().tag == Prefix::Component::DOT);
  *output << *(p.list.First().var);
  for (size_t i = 1; i < p.list.size(); ++i) {
    const Prefix::Component& c = p.list[i];
    switch (c.tag) {
      case Prefix::Component::CALL:
        Serialize(*c.funccall, indent, output);
        break;
      case Prefix::Component::INDEX:
        *output << '[';
        Serialize(*c.expression, indent, output) << ']';
        break;
      case Prefix::Component::DOT:
        *output << '.' << *c.var;
        break;
      case Prefix::Component::ATTRIBUTE:
        *output << ':' << *c.var;
        break;
      default:
        VCL_UNREACHABLE();
        break;
    }
  }
  return *output;
}

std::ostream& Serialize(const FuncCall& fc, int indent, std::ostream* output) {
  if (fc.name) {
    Indent(*output, indent) << "call " << *(fc.name);
  }

  *output << '(';
  for (size_t i = 0; i < fc.argument.size(); ++i) {
    Serialize(*fc.argument[i], indent, output);
    if (i < fc.argument.size() - 1) *output << ',';
  }
  *output << ')';

  if (fc.name) *output << ";\n";
  return *output;
}

std::string Escape(const zone::ZoneString& str) {
  std::string buf;
  buf.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '"') {
      buf.push_back('\\');
    }
    buf.push_back(str[i]);
  }
  return buf;
}

std::ostream& Serialize(const StringConcat& sc, int indent,
                        std::ostream* output) {
  VCL_UNUSED(indent);
  for (size_t i = 0; i < sc.list.size(); ++i) {
    *output << '"' << Escape(*sc.list[i]) << '"' << ' ';
  }
  return *output;
}

std::ostream& Serialize(const Integer& i, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << i.value;
}

std::ostream& Serialize(const Real& r, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << r.value;
}

std::ostream& Serialize(const Boolean& b, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << std::boolalpha << b.value << std::noboolalpha;
}

std::ostream& Serialize(const Null& n, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  VCL_UNUSED(n);
  return *output << "null";
}

std::ostream& Serialize(const String& s, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << '"' << Escape(*s.value) << '"';
}

std::ostream& Serialize(const Variable& v, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << *v.value;
}

std::ostream& Serialize(const Duration& d, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << d.value;
}

std::ostream& Serialize(const Size& s, int indent, std::ostream* output) {
  VCL_UNUSED(indent);
  return *output << s.value;
}

std::ostream& Serialize(const StringInterpolation& node, int indent,
                        std::ostream* output) {
  Indent(*output, indent);
  for (size_t i = 0; i < node.list.size(); ++i) {
    const ast::AST* ele = node.list.Index(i);
    *output << "__to_string(";
    Serialize(*ele, indent, output) << ')';
    if (i < node.list.size() - 1) *output << '+';
  }
  return *output;
}

std::ostream& Serialize(const File& f, int indent, std::ostream* output) {
  for (size_t i = 0; i < f.chunk->statement_list.size(); ++i) {
    Serialize(*(f.chunk->statement_list[i]), indent, output);
  }
  return *output;
}

}  // namespace

#define __(A, B, C)                                               \
  std::ostream& operator<<(std::ostream& output, const B& node) { \
    return Serialize(node, 0, &output);                           \
  }

VCL_AST_TYPE(__)

#undef __  // __

std::ostream& operator<<(std::ostream& output, const AST& node) {
  return Serialize(node, 0, &output);
}

void ASTSerialize(const File& file, std::ostream* output) {
  Serialize(file, 0, output);
}

Declare* NewTempVariableDeclare(zone::Zone* zone, zone::ZoneString* name,
                                Prefix* expr,
                                const vcl::util::CodeLocation& loc) {
  Declare* ret = new (zone) Declare(loc);
  ret->rhs = expr;
  ret->variable = name;
  return ret;
}

std::string Sub::FormatProtocol(const Sub& sub) {
  std::string buffer;
  buffer.reserve(128);
  buffer.append(sub.sub_name->data());
  buffer.push_back('(');
  for (size_t i = 0; i < sub.arg_list.size(); ++i) {
    buffer.append(sub.arg_list[i]->data());
    if (i < sub.arg_list.size() - 1) buffer.push_back(',');
  }
  buffer.push_back(')');
  return buffer;
}

const char* GetASTName(ASTType type) {
#define __(A, B, C) \
  case AST_##A:     \
    return C;
  switch (type) {
    VCL_AST_TYPE(__)
    default:
      return NULL;
  }
#undef __  // __
}

}  // namespace ast
}  // namespace vm
}  // namespace vcl

#include "parser.h"
#include "vcl-pri.h"

#include <vcl/util.h>
#include "constant-fold.h"

namespace vcl {
namespace vm {

using namespace vcl::util;

class Parser::EnterLexicalScope {
 public:
  EnterLexicalScope(Parser* self, ast::Chunk* chunk)
      : m_parser(self), m_prev_scope(self->m_lexical_scope) {
    self->m_lexical_scope = chunk;
  }

  ~EnterLexicalScope() { m_parser->m_lexical_scope = m_prev_scope; }

 private:
  Parser* m_parser;
  ast::Chunk* m_prev_scope;
  VCL_DISALLOW_COPY_AND_ASSIGN(EnterLexicalScope);
};

void Parser::ParserError(const char* format, ...) {
  va_list vl;
  va_start(vl, format);
  if (m_lexer.lexeme().token == TK_ERROR) {
    *m_error = m_lexer.lexeme().string();
  } else {
    *m_error =
        ReportErrorV(m_source, m_lexer.location(), "grammar", format, vl);
  }
}

zone::ZoneString* Parser::GetAnonymousSubName(zone::Zone* zone) const {
  std::stringstream formatter;
  formatter << '@' << "__anonymous_sub__::" << m_rand_name_seed;
  ++m_rand_name_seed;
  return zone::ZoneString::New(zone, formatter.str());
}

zone::ZoneString* Parser::GetTempVariableName(zone::Zone* zone) const {
  std::stringstream formatter;
  formatter << '@' << "__temp_variable__::" << m_rand_name_seed;
  ++m_rand_name_seed;
  return zone::ZoneString::New(zone, formatter.str());
}

ast::File* Parser::DoParse() {
  DCHECK(m_file == NULL);

  m_file = new (m_zone) ast::File(m_lexer.location());
  m_file->chunk = new (m_zone) ast::Chunk(m_lexer.location());
  EnterLexicalScope enter_scope(this, m_file->chunk);

  m_lexer.Next();  // Very first lexing

  // 1. Check VCL version
  if (m_lexer.lexeme().token == TK_VCL) {
    if (!ParseVCLVersion()) return NULL;
  } else {
    ParserError("You don't have VCL version declaration ??");
    return NULL;
  }

  // 2. Do top level statements
  while (m_lexer.lexeme().token != TK_EOF) {
    ast::AST* expr;
    switch (m_lexer.lexeme().token) {
      case TK_SUB_ROUTINE:
        if (!(expr = ParseSub())) return NULL;
        break;
      case TK_INCLUDE:
        if (!(expr = ParseInclude())) return NULL;
        break;
      case TK_IMPORT:
        if (!(expr = ParseImport())) return NULL;
        break;
      case TK_GLOBAL:
        if (!(expr = ParseGlobalVariable())) return NULL;
        break;
      case TK_ACL:
        if (!(expr = ParseACL())) return NULL;
        break;
      case TK_VAR:
        if (!(expr = ParseExtension())) return NULL;
        break;
      case TK_EOF:
        break;
      default:
        ParserError(
            "In global scope, you are only allowed to put statement "
            "like include,import,global variable definition,sub "
            "definition and vcl version definition, the syntax you put "
            "here is unknown to me!");
        return NULL;
    }
    m_file->chunk->statement_list.Add(m_zone, expr);
    m_file->chunk->location_end = m_lexer.location();
  }

  return m_file;
}

// =============================================================
// Top level
// =============================================================
ast::AST* Parser::ParseGlobalVariable() {
  DCHECK(m_lexer.lexeme().token == TK_GLOBAL);
  ast::Global* global = new (m_zone) ast::Global(m_lexer.location());
  if (!m_lexer.Try(TK_VAR)) {
    ParserError(
        "global variable expect a variable name after "
        "\"global\" keyword!");
    return NULL;
  }

  global->name = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (!m_lexer.Try(TK_ASSIGN)) {
    ParserError(
        "global variable assignment expect a \"=\" after variable "
        "name!");
    return NULL;
  }
  m_lexer.Next();  // Skip =

  if (!(global->value = ParseExpression())) {
    return NULL;
  }

  if (!m_lexer.Expect(TK_SEMICOLON)) {
    ParserError("Expect \";\" after global variable statement!");
    return NULL;
  }

  DCHECK(m_file);
  return global;
}

bool Parser::ParseVCLVersion() {
  DCHECK(m_lexer.lexeme().token == TK_VCL);
  if (!m_lexer.Try(TK_REAL)) {
    ParserError("VCL version requires a real/float number!");
    return false;
  }

  if (vcl::CheckVCLVersion(m_lexer.lexeme().real())) {
    if (!m_lexer.Try(TK_SEMICOLON)) {
      ParserError("Expect \";\" after vcl version!");
      return false;
    }
    m_lexer.Next();
  } else {
    ParserError("VCL version mismatch,we don't support version %.2f",
                m_lexer.lexeme().real());
    return false;
  }
  return true;
}

ast::AST* Parser::ParseACL() {
  DCHECK(m_lexer.lexeme().token == TK_ACL);
  if (!m_lexer.Try(TK_VAR)) {
    ParserError("Expect a variable name for ACL");
    return NULL;
  }
  ast::ACL* acl = new (m_zone) ast::ACL(m_lexer.location());
  acl->name = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());

  if (m_lexer.Next().token == TK_LBRA) {
    if (m_lexer.Next().token == TK_RBRA) {
      m_lexer.Next();
    } else {
      // ---------------------------------------
      // ACL item parsing
      // ---------------------------------------
      do {
        ast::ACL::ACLItem item;
        switch (m_lexer.lexeme().token) {
          case TK_STRING:
            item.name =
                zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
            m_lexer.Next();
            break;
          case TK_NOT:
            item.negative = true;
            if (!m_lexer.Try(TK_STRING)) {
              ParserError(
                  "Expect a string literal to serve IP address in ACL!");
              return NULL;
            }
            item.name =
                zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
            m_lexer.Next();
            break;
          default:
            ParserError(
                "In ACL list,you are only allowed to put quoted string "
                "ip address ,optionally prefix with not sign and also "
                "the netmask,however this construct is not unknown to "
                "me!");
            return NULL;
        }
        // Check optional network mask
        if (m_lexer.lexeme().token == TK_DIV) {
          // Optional network mask
          if (!m_lexer.Try(TK_INTEGER)) {
            ParserError("Expect a integer here to serve as network mask!");
            return NULL;
          }
          {
            int32_t m = m_lexer.lexeme().integer();
            if (m < 0 || m > 128) {
              ParserError("Network mask must be in range [0,128]!");
              return NULL;
            }
            item.mask = static_cast<uint8_t>(m);
          }
          m_lexer.Next();
        }
        acl->list.Add(m_zone, item);

        if (!m_lexer.Expect(TK_SEMICOLON)) {
          ParserError("Expect a \";\" here to end VCL's item!");
          return NULL;
        }
      } while (m_lexer.lexeme().token != TK_RBRA &&
               m_lexer.lexeme().token != TK_EOF);

      if (m_lexer.lexeme().token == TK_EOF) {
        ParserError("ACL literal is not closed properly with \"}\"!");
        return NULL;
      }
      m_lexer.Next();
    }
  }

  DCHECK(m_file);
  return acl;
}

ast::AST* Parser::ParseExtension() {
  DCHECK(m_lexer.lexeme().token == TK_VAR);
  zone::ZoneString* type_name =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (!m_lexer.Try(TK_VAR)) {
    ParserError("Expect a variable name after the extension type name!");
    return NULL;
  }
  zone::ZoneString* instance_name =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (!m_lexer.Try(TK_LBRA)) {
    ParserError("Expect a \"{\" to start a extension initializer literal!");
    return NULL;
  }
  ast::Extension* ext = new (m_zone) ast::Extension(m_lexer.location());
  ext->instance_name = instance_name;
  ext->type_name = type_name;
  ext->initializer = ParseExtensionInitializer();
  if (!(ext->initializer)) return NULL;

  // Lastly , check the optional ;
  if (m_lexer.lexeme().token == TK_SEMICOLON) {
    m_lexer.Next();
  }

  DCHECK(m_file);
  return ext;
}

ast::AST* Parser::ParseSub() {
  DCHECK(m_lexer.lexeme().token == TK_SUB_ROUTINE);
  if (!m_lexer.Try(TK_VAR)) {
    ParserError("sub must follow a variable name to indicate function name!");
    return NULL;
  }
  zone::ZoneString* name =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  m_lexer.Next();
  return ParseSubDefinition(name);
}

ast::AST* Parser::ParseSubDefinition(zone::ZoneString* name) {
  ast::Sub* sub = new (m_zone) ast::Sub(m_lexer.location());
  sub->sub_name = name;
  if (m_lexer.lexeme().token == TK_LPAR) {
    if (m_lexer.Next().token == TK_RPAR) {
      m_lexer.Next();
    } else {
      // parse the argument list
      do {
        if (m_lexer.lexeme().token != TK_VAR) {
          ParserError("Expect a variable name to be sub's argument list!");
          return NULL;
        }
        sub->arg_list.Add(
            m_zone, zone::ZoneString::New(m_zone, m_lexer.lexeme().string()));
        m_lexer.Next();
        if (m_lexer.lexeme().token == TK_COMMA) {
          m_lexer.Next();
        } else if (m_lexer.lexeme().token == TK_RPAR) {
          m_lexer.Next();
          break;
        } else {
          ParserError("Expect a \",\" or \")\" in sub's argument list!");
          return NULL;
        }
      } while (true);
    }
  }

  if (m_lexer.lexeme().token == TK_LBRA) {
    if (!(sub->body = ParseChunk())) return NULL;
  } else {
    ParserError(
        "Sub-routine %s's definition doesn't have a function body or "
        "you forget to put a \"{\" to start to define a body.",
        name->data());
    return NULL;
  }

  return sub;
}

ast::AST* Parser::ParseAnonymousSub() {
  DCHECK(m_lexer.lexeme().token == TK_SUB_ROUTINE);
  m_lexer.Next();
  if (m_lexer.lexeme().token != TK_LPAR && m_lexer.lexeme().token != TK_LBRA) {
    ParserError(
        "Anonymouse sub routine requires a optional argument list or "
        "\"{\" to indicate the start of a function body definition!");
    return NULL;
  }
  return ParseSubDefinition(GetAnonymousSubName(m_zone));
}

ast::AST* Parser::ParseInclude() {
  DCHECK(m_lexer.lexeme().token == TK_INCLUDE);
  if (!m_lexer.Try(TK_STRING)) {
    ParserError("include statement must follow a string literal!");
    return NULL;
  }
  zone::ZoneString* path =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (!m_lexer.Try(TK_SEMICOLON)) {
    ParserError("Expect a \";\" at the end of include statement!");
    return NULL;
  }
  m_lexer.Next();
  ast::Include* inc = new (m_zone) ast::Include(m_lexer.location(), path);
  DCHECK(m_file);
  return inc;
}

ast::AST* Parser::ParseImport() {
  DCHECK(m_lexer.lexeme().token == TK_IMPORT);
  if (!m_lexer.Try(TK_VAR)) {
    ParserError("import statement must follow a variable name!");
    return NULL;
  }
  zone::ZoneString* name =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (!m_lexer.Try(TK_SEMICOLON)) {
    ParserError("Expect a \";\" here at the end of import!");
    return NULL;
  }
  m_lexer.Next();

  ast::Import* import = new (m_zone) ast::Import(m_lexer.location(), name);
  DCHECK(m_file);
  return import;
}

// =============================================================
// Statements
// =============================================================
ast::AST* Parser::ParseStatement() {
  switch (m_lexer.lexeme().token) {
    case TK_LBRA:
      return ParseLexScope();
    case TK_IF:
      return ParseIf();
    case TK_FOR:
      if (m_support_loop)
        return ParseFor();
      else
        goto loop_error;
    case TK_BREAK:
      if (m_support_loop)
        return ParseLoopControlStmt<ast::Break>();
      else
        goto loop_error;

    case TK_CONTINUE:
      if (m_support_loop)
        return ParseLoopControlStmt<ast::Continue>();
      else
        goto loop_error;
    case TK_SET:
      return ParseSet();
    case TK_UNSET:
      return ParseUnset();
    case TK_NEW:
      return ParseNew();
    case TK_DECLARE:
      return ParseDeclare();
    case TK_CALL:
      return ParseCall();
    case TK_RETURN:
      return ParseReturnOrTerminate();
    case TK_VAR:
      return ParsePrefixCall();
    default:
      ParserError(
          "expect a valid statement here, a statment can be "
          "return,if,set,unset,new,declare,call,member function call!");
      return NULL;
  }

loop_error:
  ParserError(
      "In this script configuration, we don't support for/break/continue "
      "language constructs!");
  return NULL;
}

ast::AST* Parser::ParseLexScope() {
  DCHECK(m_lexer.lexeme().token == TK_LBRA);
  ast::LexScope* node = new (m_zone) ast::LexScope(m_lexer.location());
  node->body = ParseChunk();
  if (!node->body) return NULL;
  return node;
}

ast::AST* Parser::ParseStatementWithSemicolon() {
  ast::AST* ret = ParseStatement();
  if (!ret) return NULL;
  if ((ret->type != ast::AST_IF && ret->type != ast::AST_LEXSCOPE &&
       ret->type != ast::AST_FOR) &&
      !m_lexer.Expect(TK_SEMICOLON)) {
    ParserError("Expect \";\" here!");
    return NULL;
  }
  return ret;
}

ast::AST* Parser::ParseIf() {
  DCHECK(m_lexer.lexeme().token == TK_IF);
  m_lexer.Next();
  ast::If* node = new (m_zone) ast::If(m_lexer.location());
  // 1. Leading "if"
  {
    ast::If::Branch br;
    if (!ParseBranch(&br)) return NULL;
    node->branch_list.Add(m_zone, br);
  }
  // 2. Optional "elif/elsif/elseif"
  {
    while (m_lexer.lexeme().token == TK_ELIF ||
           m_lexer.lexeme().token == TK_ELSIF ||
           m_lexer.lexeme().token == TK_ELSEIF) {
      ast::If::Branch br;
      m_lexer.Next();
      if (!ParseBranch(&br)) return NULL;
      node->branch_list.Add(m_zone, br);
    }
  }
  // 3. Optional else
  {
    ast::If::Branch br;
    if (m_lexer.lexeme().token == TK_ELSE) {
      m_lexer.Next();
      if (!(br.body = ParseSingleStatementOrChunk())) return NULL;
      node->branch_list.Add(m_zone, br);
    }
  }
  return node;
}

bool Parser::ParseBranch(ast::If::Branch* br) {
  if (m_lexer.lexeme().token != TK_LPAR) {
    ParserError("Expect \"(\" here for if/else if branch condition!");
    return false;
  }
  m_lexer.Next();
  br->condition = ParseExpression();
  if (!(br->condition)) return false;
  if (!m_lexer.Expect(TK_RPAR)) {
    ParserError("Expect \")\" after if/elseif condition!");
    return false;
  }
  if (!(br->body = ParseSingleStatementOrChunk())) return false;
  return true;
}

ast::AST* Parser::ParseFor() {
  DCHECK(m_lexer.lexeme().token == TK_FOR);

  ast::For* node = new (m_zone) ast::For(m_lexer.location());
  m_lexer.Next();
  if (!m_lexer.Expect(TK_LPAR)) {
    ParserError("Expect \"(\" after \"for\"!");
    return NULL;
  }
  if (m_lexer.lexeme().token != TK_VAR) {
    ParserError("Expect a variable name as loop reduction variable");
    return NULL;
  }

  zone::ZoneString* var =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  m_lexer.Next();  // Skip the variable name

  if (m_lexer.lexeme().token == TK_COLON) {
    m_lexer.Next();
    node->key = var;
    if (!(node->iterator = ParseExpression())) return NULL;
    if (!m_lexer.Expect(TK_RPAR)) {
      ParserError("Expect a \")\" to close loop condition");
      return NULL;
    }
  } else {
    DCHECK(m_lexer.lexeme().token == TK_COMMA);
    m_lexer.Next();
    if (m_lexer.lexeme().token != TK_VAR) {
      ParserError("Expect a variable name here");
      return NULL;
    }
    node->key = var;
    node->val = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
    if (!m_lexer.Try(TK_COLON)) {
      ParserError("Expect \":\" here");
      return NULL;
    }
    m_lexer.Next();
    if (!(node->iterator = ParseExpression())) return NULL;
    if (!m_lexer.Expect(TK_RPAR)) {
      ParserError("Expect a \")\" to close loop condition");
    }
  }

  ++m_nested_loop;
  if (!(node->body = ParseSingleStatementOrChunk())) return NULL;
  --m_nested_loop;

  return node;
}

ast::Chunk* Parser::ParseChunk() {
  DCHECK(m_lexer.lexeme().token == TK_LBRA);
  if (m_lexer.Next().token == TK_RBRA) {
    m_lexer.Next();
    return new (m_zone) ast::Chunk(m_lexer.location());
  } else {
    ast::Chunk* ck = new (m_zone) ast::Chunk(m_lexer.location());
    EnterLexicalScope enter_scope(this, ck);

    do {
      ast::AST* statement = ParseStatementWithSemicolon();
      if (!statement) return NULL;
      ck->statement_list.Add(m_zone, statement);
    } while (m_lexer.lexeme().token != TK_RBRA &&
             m_lexer.lexeme().token != TK_EOF);
    if (m_lexer.lexeme().token == TK_EOF) {
      ParserError("Chunk body is not properly closed by \"}\"!");
      return NULL;
    } else {
      ck->location_end = m_lexer.location();
      m_lexer.Next();
      return ck;
    }
  }
}

ast::Chunk* Parser::ParseSingleStatementOrChunk() {
  if (m_lexer.lexeme().token == TK_LBRA) {
    return ParseChunk();
  } else {
    ast::Chunk* ret = new (m_zone) ast::Chunk(m_lexer.location());
    if (m_lexer.lexeme().token == TK_SEMICOLON) {
      // Empty statement sitaution
      m_lexer.Next();
      return ret;
    } else {
      EnterLexicalScope enter_scope(this, ret);
      ast::AST* code = ParseStatementWithSemicolon();
      if (code) {
        ret->statement_list.Add(m_zone, code);
        return ret;
      } else {
        return NULL;
      }
    }
  }
}

template <int TK>
ast::AST* Parser::ParseDeclareImpl() {
  if (m_lexer.lexeme().token != TK_VAR) {
    ParserError("In new/declare statement, expect a variable name!");
    return NULL;
  }
  ast::Declare* dec = new (m_zone) ast::Declare(m_lexer.location());
  dec->variable = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  m_lexer.Next();

  if (TK == TK_DECLARE) {
    if (m_lexer.lexeme().token == TK_ASSIGN) {
      m_lexer.Next();
      dec->rhs = ParseExpression();
      if (!dec->rhs) return NULL;
      return dec;
    } else {
      // Do not eat the token here, just treat it unknown
      // and we end up with default local variable declaration
      return dec;
    }
  } else {
    if (!m_lexer.Expect(TK_ASSIGN)) {
      ParserError("In new statement, expect \"=\" after the variable name!");
      return NULL;
    }
    if (!(dec->rhs = ParseExpression())) return NULL;
    return dec;
  }
}

ast::AST* Parser::ParseUnset() {
  DCHECK(m_lexer.lexeme().token == TK_UNSET);
  m_lexer.Next();
  ast::Unset* unset = new (m_zone) ast::Unset(m_lexer.location());
  if (!ParseLHS(&(unset->lhs))) return NULL;
  return unset;
}

ast::AST* Parser::ParseSet() {
  DCHECK(m_lexer.lexeme().token == TK_SET);
  m_lexer.Next();
  ast::Set* set = new (m_zone) ast::Set(m_lexer.location());
  if (!ParseLHS(&(set->lhs))) return NULL;
  switch (m_lexer.lexeme().token) {
    case TK_ASSIGN:
    case TK_SELF_ADD:
    case TK_SELF_SUB:
    case TK_SELF_MUL:
    case TK_SELF_DIV:
    case TK_SELF_MOD:
      break;
    default:
      ParserError(
          "In set statement, expected operators are \"=\",\"+=\""
          ",\"-=\",\"*=\",\"/=\",\"%-\"!");
      return NULL;
  }
  set->op = m_lexer.lexeme().token;
  m_lexer.Next();
  set->rhs = ParseExpression();
  if (!(set->rhs)) return NULL;
  return set;
}

bool Parser::ParseLHS(ast::LeftHandSide* output) {
  if (m_lexer.lexeme().token != TK_VAR) {
    ParserError("Left hand side value expect a variable name!");
    return false;
  } else {
    zone::ZoneString* prefix =
        zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
    m_lexer.Next();  // Skip the first variable
    int last_component = ast::Prefix::Component::DOT;
    ast::Prefix* ret;
    if (IsPrefixOperator(m_lexer.lexeme().token)) {
      ret = ParsePrefix(prefix, &last_component);
      if (!ret) return false;
      if (last_component == ast::Prefix::Component::CALL ||
          last_component == ast::Prefix::Component::MCALL) {
        ParserError("Left hand side value cannot be a function call!");
        return false;
      }
      *output = ast::LeftHandSide(ret);
    } else {
      *output = ast::LeftHandSide(prefix);
    }
    return true;
  }
}

ast::AST* Parser::ParsePrefixCall() {
  DCHECK(m_lexer.lexeme().token == TK_VAR);
  zone::ZoneString* prefix =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  m_lexer.Next();
  int last_component = ast::Prefix::Component::DOT;
  ast::Prefix* ret;
  if (IsPrefixOperator(m_lexer.lexeme().token)) {
    ret = ParsePrefix(prefix, &last_component);
    if (!ret) return NULL;
    if (last_component == ast::Prefix::Component::CALL ||
        last_component == ast::Prefix::Component::MCALL) {
      return new (m_zone) ast::Stmt(m_lexer.location(), ret);
    } else {
      ParserError("Expect a valid function call or othre statement here!");
      return NULL;
    }
  } else if (m_lexer.lexeme().token == TK_SEMICOLON) {
    // Short for calling void argument function
    // Fastly supports it
    ast::FuncCall* fc = new (m_zone) ast::FuncCall(m_lexer.location());
    fc->name = prefix;
    return fc;
  } else {
    ParserError(
        "Maybe forget to write a \";\" here?Or your statement "
        "here cannot be understood by parser!Parser only expects "
        "syntax express function call here or keyword prefixed statement.");
  }
  return NULL;
}

ast::AST* Parser::ParseCall() {
  DCHECK(m_lexer.lexeme().token == TK_CALL);
  if (m_lexer.Next().token != TK_VAR) {
    ParserError(
        "Call statement must follow a variable name indicate "
        "sub/function to call!");
    return NULL;
  }
  ast::FuncCall* fc = new (m_zone) ast::FuncCall(m_lexer.location());
  fc->name = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (m_lexer.Next().token == TK_LPAR) {
    return ParseFuncCallArgument(fc);
  } else {
    return fc;
  }
}

ast::AST* Parser::ParseReturnOrTerminate() {
  DCHECK(m_lexer.lexeme().token == TK_RETURN);
  m_lexer.Next();  // Skip 'return'
  switch (m_lexer.lexeme().token) {
    case TK_LBRA:
      return ParseReturn();
    case TK_LPAR:
      return ParseTerminate();
    default:
      // empty return
      return new (m_zone) ast::Return(m_lexer.location());
  }
}

ast::AST* Parser::ParseReturn() {
  DCHECK(m_lexer.lexeme().token == TK_LBRA);
  if (m_lexer.Next().token == TK_RBRA) {
    return new (m_zone) ast::Return(m_lexer.location());
  } else {
    ast::AST* value = ParseExpression();
    if (!m_lexer.Expect(TK_RBRA)) {
      ParserError("Expect \"}\" in return statement for returning value!");
      return NULL;
    }
    return new (m_zone) ast::Return(m_lexer.location(), value);
  }
}

ast::AST* Parser::ParseTerminate() {
  DCHECK(m_lexer.lexeme().token == TK_LPAR);
  if (m_lexer.Next().token == TK_RPAR) {
    return new (m_zone) ast::Terminate(m_lexer.location());
  } else {
    // Now we need to handle the return statement grammar specifically.
    // When we reach here, any expression in return is invalid except
    // certain keywords and certain function call. We *allow* function
    // call and keyword here. That is everything we allow here
    zone::ZoneString* prefix;
    ast::Terminate* terminate = new (m_zone) ast::Terminate(m_lexer.location());
    if (m_lexer.lexeme().token == TK_VAR) {
      prefix = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
      m_lexer.Next();  // Skip the variable
    } else {
      ParserError(
          "Expect special *variable name* in return statement with"
          " terminate semantic!");
      return NULL;
    }
    if ((terminate->action = vcl::GetActionNameEnum(prefix->data())) ==
        vcl::ACT_EXTENSION) {
      if (IsPrefixOperator(m_lexer.lexeme().token)) {
        int op = ast::Prefix::Component::DOT;
        if (!(terminate->value = ParsePrefix(prefix, &op))) return NULL;
        if (op != ast::Prefix::Component::CALL &&
            op != ast::Prefix::Component::MCALL) {
          goto fail;
        }
      } else {
      fail:
        ParserError(
            "In terminated return statement, you can only put "
            "action name or function call!");
        return NULL;
      }
    }
    if (!m_lexer.Expect(TK_RPAR)) {
      ParserError(
          "Expect \")\" to close return statement with "
          "terminate semantic!");
      return NULL;
    }
    return terminate;
  }
}

ast::AST* Parser::ParseTernary() {
  DCHECK(m_lexer.lexeme().token == TK_IF);
  ast::Ternary* ternary = new (m_zone) ast::Ternary(m_lexer.location());
  m_lexer.Next();  // Skip if
  if (!m_lexer.Expect(TK_LPAR)) {
    ParserError("Expect a \"(\" after ternary \"if\"!");
    return NULL;
  }
  ast::AST* condition;
  ast::AST* first;
  ast::AST* second;

  if (!(condition = ParseExpression())) return NULL;
  if (!m_lexer.Expect(TK_COMMA)) {
    ParserError("Expect a \",\" after condition in ternary \"if\"!");
    return NULL;
  }

  if (!(first = ParseExpression())) return NULL;
  if (!m_lexer.Expect(TK_COMMA)) {
    ParserError("Expect a \",\" after first branch in ternary \"if\"!");
    return NULL;
  }

  if (!(second = ParseExpression())) return NULL;
  if (!m_lexer.Expect(TK_RPAR)) {
    ParserError("Expect a \")\" after second branch in bernary \"if\"!");
    return NULL;
  }
  ternary->condition = condition;
  ternary->first = first;
  ternary->second = second;
  return ternary;
}

ast::AST* Parser::ParseExpression() {
  ast::AST* expr = ParseBinary();
  if(!expr) return NULL;
  else {
    std::string error;
    ast::AST* ret = ConstantFold(expr,m_zone,&error);
    if(!ret) ParserError("%s",error.c_str());
    return ret;
  }
}

const int kPrecedence[] = {
    1,  // TK_ADD
    1,  // TK_SUB
    0,  // TK_MUL
    0,  // TK_DIV
    0,  // TK_MOD
    3,  // TK_MATCH
    3,  // TK_NOT_MATCH
    3,  // TK_EQ
    3,  // TK_NE
    2,  // TK_LT
    2,  // TK_LE
    2,  // TK_GT
    2,  // TK_GE
    4,  // TK_AND
    5   // TK_OR
};

// Maximum precedence
static const int kMaxPrecendence = 5;

ast::AST* Parser::ParseBinaryPrecedence(int precedence) {
  if (precedence < 0)
    return ParseUnary();
  else {
    DCHECK(static_cast<size_t>(precedence) < boost::size(kPrecedence));
    ast::AST* left;
    if (!(left = ParseBinaryPrecedence(precedence - 1))) return NULL;

    do {
      if (TokenIsBinaryOperator(m_lexer.lexeme().token)) {
        DCHECK(m_lexer.lexeme().token < boost::size(kPrecedence));
        int cur_precedence = kPrecedence[m_lexer.lexeme().token];

        // We shuold already consume all operators that has smaller
        // precedence already. The only siutation left is following
        DCHECK(cur_precedence >= precedence);

        if (cur_precedence == precedence) {
          // OK we can handle it here since the precedence matches
          Token op = m_lexer.lexeme().token;
          m_lexer.Next();
          ast::AST* right = ParseBinaryPrecedence(precedence - 1);
          if (!right) return NULL;
          left = new (m_zone) ast::Binary(m_lexer.location(), left, right, op);
        } else {
          break;
        }
      } else {
        break;
      }
    } while (true);

    return left;
  }
}

ast::AST* Parser::ParseBinary() {
  return ParseBinaryPrecedence(kMaxPrecendence);
}

ast::AST* Parser::ParseUnary() {
  if (m_lexer.lexeme().token != TK_ADD && m_lexer.lexeme().token != TK_SUB &&
      m_lexer.lexeme().token != TK_NOT) {
    return ParsePrimary();
  } else {
    ast::Unary* ret = new (m_zone) ast::Unary(m_lexer.location());
    Token token;
    do {
      ret->ops.Add(m_zone, m_lexer.lexeme().token);
      token = m_lexer.Next().token;
    } while (token == TK_ADD || token == TK_SUB || token == TK_NOT);
    ret->operand = ParsePrimary();
    if (!ret->operand) return NULL;
    return ret;
  }
}

ast::ExtensionLiteral* Parser::ParseExtensionLiteral(zone::ZoneString* prefix) {
  ast::ExtensionLiteral* ret =
      new (m_zone) ast::ExtensionLiteral(m_lexer.location());
  ret->type_name = prefix;
  ret->initializer = ParseExtensionInitializer();
  if (!ret->initializer) return NULL;
  return ret;
}

ast::ExtensionInitializer* Parser::ParseExtensionInitializer() {
  DCHECK(m_lexer.lexeme().token == TK_LBRA);
  ast::ExtensionInitializer* ret =
      new (m_zone) ast::ExtensionInitializer(m_lexer.location());
  m_lexer.Next();
  while (m_lexer.lexeme().token != TK_RBRA &&
         m_lexer.lexeme().token != TK_EOF) {
    if (!m_lexer.Expect(TK_DOT) || m_lexer.lexeme().token != TK_VAR) {
      ParserError(
          "Field inside of extension must have leading \".\" followed "
          "by a valid variable name!");
      return NULL;
    }
    ast::ExtensionInitializer::ExtensionField field;
    field.name = zone::ZoneString::New(m_zone, m_lexer.lexeme().string());

    if (!m_lexer.Try(TK_ASSIGN)) {
      ParserError(
          "Expect a \"=\" for field assignment in extension initializer!");
      return NULL;
    }
    m_lexer.Next();

    field.value = ParseExpression();
    if (!field.value) return NULL;
    ret->list.Add(m_zone, field);

    if (m_lexer.lexeme().token != TK_SEMICOLON &&
        m_lexer.lexeme().token != TK_COMMA) {
      ParserError("Extension field needs to end with \";\" or \",\"");
      return NULL;
    }
    m_lexer.Next();
  }
  if (m_lexer.lexeme().token == TK_EOF) {
    ParserError("Extension is not closed by \"}\"!");
    return NULL;
  }
  m_lexer.Next();  // Skip last }
  return ret;
}

ast::FuncCall* Parser::ParseFuncCallArgument(ast::FuncCall* fc) {
  DCHECK(m_lexer.lexeme().token == TK_LPAR);
  if (m_lexer.Next().token == TK_RPAR) {
    m_lexer.Next();
    return fc;
  } else {
    do {
      ast::AST* expr = ParseExpression();
      if (!expr) return NULL;
      fc->argument.Add(m_zone, expr);
      if (m_lexer.lexeme().token == TK_COMMA) {
        m_lexer.Next();
      } else if (m_lexer.lexeme().token == TK_RPAR) {
        m_lexer.Next();
        break;
      } else {
        ParserError(
            "Expect a \",\" or \")\" here in function call argument list ",
            "the current token is unrecognized!");
        return NULL;
      }
    } while (true);
    return fc;
  }
}

ast::FuncCall* Parser::ParseFuncCall() {
  return ParseFuncCallArgument(new (m_zone) ast::FuncCall(m_lexer.location()));
}

ast::FuncCall* Parser::ParseMethodCall(ast::AST* arg) {
  ast::FuncCall* fc = new (m_zone) ast::FuncCall(m_lexer.location());
  fc->argument.Add(m_zone, arg);
  return ParseFuncCallArgument(fc);
}

ast::AST* Parser::ParseStringConcat() {
  DCHECK(m_lexer.lexeme().token == TK_STRING);
  zone::ZoneString* string =
      zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
  if (m_lexer.Next().token == TK_STRING) {
    ast::StringConcat* cc = new (m_zone) ast::StringConcat(m_lexer.location());
    cc->list.Add(m_zone, string);
    do {
      cc->list.Add(m_zone,
                   zone::ZoneString::New(m_zone, m_lexer.lexeme().string()));
    } while (m_lexer.Next().token == TK_STRING);
    return cc;
  } else {
    return new (m_zone) ast::String(m_lexer.location(), string);
  }
}

ast::Prefix* Parser::ParsePrefix(zone::ZoneString* prefix,
                                 int* last_component) {
  DCHECK(IsPrefixOperator(m_lexer.lexeme().token));

  ast::Prefix* ret = new (m_zone) ast::Prefix(m_lexer.location());
  ret->list.Add(m_zone, ast::Prefix::Component(prefix));
  do {
    switch (m_lexer.lexeme().token) {
      case TK_DOT:
      case TK_COLON: {
        int op = m_lexer.lexeme().token;
        if (!m_lexer.TryTokenAsExtendedVar()) {
          ParserError("Expect variable after a \".\" operator!");
          return NULL;
        }

        ret->list.Add(
            m_zone,
            ast::Prefix::Component(
                zone::ZoneString::New(m_zone, m_lexer.lexeme().string()),
                op == TK_DOT ? ast::Prefix::Component::DOT
                             : ast::Prefix::Component::ATTRIBUTE));

        m_lexer.Next();
        if (last_component) *last_component = ast::Prefix::Component::DOT;
        break;
      }
      case TK_LSQR:
        m_lexer.Next();
        {
          ast::AST* expr = ParseExpression();
          if (!expr) return NULL;
          if (!m_lexer.Expect(TK_RSQR)) {
            ParserError("Expect a \"]\" in a index operation!");
            return NULL;
          }
          ret->list.Add(m_zone, ast::Prefix::Component(expr));
        }
        if (last_component) *last_component = ast::Prefix::Component::INDEX;
        break;
      case TK_LPAR: {
        ast::FuncCall* fc = ParseFuncCall();
        if (!fc) return NULL;
        ret->list.Add(m_zone, ast::Prefix::Component(fc));
      }
        if (last_component) *last_component = ast::Prefix::Component::CALL;
        break;
      case TK_FIELD: {
        if(m_support_desugar) {
          ast::AST* self;
          // The grammar a::b(c) is actually a syntax sugar , however to
          // make this syntax sugar has good semantic is kind of tricky.
          // The core here is the evaluation a::b can be very nasty due
          // to user could chain as much prefix expression as they like.
          // Grammar like , a.b.c.d.e.f::g is allowed.
          //
          // What we actually do here is we setup a temporary assignment
          // AST in current lexical scope and assign a.b.c.d.e.f to that
          // temporary variable, so an expression a.b.c.d.e.f::g() is same
          // as :
          //  __temp = a.b.c.d.e.f;
          //  __temp.g( __temp );

          if (ret->list.size() == 1) {
            // This is slightly a optimization to avoid too much local variables
            // when user just do one level of lookup. Basically for cases as :
            // a::b() , we will not create a temporary variable to avoid vm
            // overhead.
            DCHECK(ret->list[0].tag == ast::Prefix::Component::DOT);
            self = new (m_zone)
                ast::Variable(m_lexer.location(), ret->list.First().var);
          } else {
            zone::ZoneString* temp_name = GetTempVariableName(m_zone);

            // 1. New temproary variable
            DCHECK(m_lexical_scope);
            m_lexical_scope->statement_list.Add(
                m_zone,
                ast::NewTempVariableDeclare(
                    m_zone, temp_name, ret, ret->location));

            // 2. Create a new Prefix expression here to override the existed
            // prefix expression
            ret = new (m_zone) ast::Prefix(ret->location);
            ret->list.Add(m_zone, ast::Prefix::Component(temp_name));

            // 3. Create AST reference the temporary variable
            self = new (m_zone) ast::Variable(m_lexer.location(), temp_name);
          }

          // Generate rest of the method call AST
          if (!m_lexer.Try(TK_VAR)) {
            ParserError("Expect a variable name after \"::\" operation!");
            return NULL;
          }
          ret->list.Add(
              m_zone,
              ast::Prefix::Component(
                  zone::ZoneString::New(m_zone, m_lexer.lexeme().string()),
                  ast::Prefix::Component::DOT));
          if (!m_lexer.Try(TK_LPAR)) {
            ParserError(
                "Expect function call argument list in method call "
                "operation!");
            return NULL;
          }

          ast::FuncCall* mc = ParseMethodCall(self);
          ret->list.Add(m_zone, mc);

          if (last_component) *last_component = ast::Prefix::Component::CALL;
        } else {
          if(!m_lexer.Try(TK_VAR)) {
            ParserError("Expect a variable name after \"::\" operation!");
            return NULL;
          }
          ast::FuncCall* fc = new (m_zone) ast::FuncCall(m_lexer.location());
          fc->name = zone::ZoneString::New(m_zone,m_lexer.lexeme().string());

          if(!m_lexer.Try(TK_LPAR)) {
            ParserError(
                "Expect a function call agrument list in method call "
                "operation!");
            return NULL;
          }

          if(!ParseFuncCallArgument(fc)) return NULL;

          ret->list.Add(m_zone, ast::Prefix::Component(
                fc, ast::Prefix::Component::MCALL));

          if(last_component) *last_component = ast::Prefix::Component::MCALL;
        }
      } break;
      default:
        return ret;
    }
  } while (true);
  return NULL;
}

ast::List* Parser::ParseList() {
  DCHECK(m_lexer.lexeme().token == TK_LSQR);
  ast::List* ret = new (m_zone) ast::List(m_lexer.location());
  if (m_lexer.Next().token == TK_RSQR) {
    m_lexer.Next();
  } else {
    ret->list.Reserve(m_zone, 4);
    do {
      ast::AST* expr = ParseExpression();
      if (!expr) return NULL;
      ret->list.Add(m_zone, expr);

      if (m_lexer.lexeme().token == TK_COMMA) {
        m_lexer.Next();
      } else if (m_lexer.lexeme().token == TK_RSQR) {
        m_lexer.Next();
        break;
      } else {
        ParserError("Expect \"]\" or list literal!");
        return NULL;
      }
    } while (true);
  }
  return ret;
}

ast::Dict* Parser::ParseDict() {
  DCHECK(m_lexer.lexeme().token == TK_LBRA);
  ast::Dict* ret = new (m_zone) ast::Dict(m_lexer.location());
  if (m_lexer.Next().token == TK_RBRA) {
    m_lexer.Next();
  } else {
    ast::AST* key;
    ret->list.Reserve(m_zone, 4);
    do {
      // Key parsing for Dictionary. The dictionary's key can be of these
      // 2 types.
      // 1. String Literal
      // 2. Variable name
      // 3. An expression that is quoted by square bracket.
      //
      // The reason for why we cannot use a expression here is simply because
      // Fastly's sub-component accessing grammar is using ":" to do job. So
      // an expression a:bc is valid which leads to ambigious.
      switch (m_lexer.lexeme().token) {
        case TK_STRING:
          key = new (m_zone) ast::String(
              m_lexer.location(),
              zone::ZoneString::New(m_zone, m_lexer.lexeme().string()));
          m_lexer.Next();
          break;
        case TK_VAR:
          key = new (m_zone) ast::String(
              m_lexer.location(),
              zone::ZoneString::New(m_zone, m_lexer.lexeme().string()));
          m_lexer.Next();
          break;
        case TK_LSQR:
          if (m_lexer.Next().token == TK_RSQR) {
            ParserError(
                "Empty dictionary key here, you need to specify an "
                "expression to indicate the key!");
            return NULL;
          }
          key = ParseExpression();
          if (!m_lexer.Expect(TK_RSQR)) {
            ParserError("Dictionary's key is not closed by the \"]\"!");
            return NULL;
          }
          break;
        default:
          ParserError(
              "Dictionary's key can only be 1) string literal or 2) "
              "an expression wrapped by \"[\" and \"]\",eg: "
              "[val1+val2]!");
          return NULL;
      }

      if (!key) return NULL;
      if (!m_lexer.Expect(TK_COLON)) {
        ParserError("Expect a \":\" here in object literal!");
        return NULL;
      }
      ast::AST* val = ParseExpression();
      if (!val) return NULL;
      ret->list.Add(m_zone, ast::Dict::Entry(key, val));
      if (m_lexer.lexeme().token == TK_COMMA) {
        m_lexer.Next();
      } else if (m_lexer.lexeme().token == TK_RBRA) {
        m_lexer.Next();
        break;
      } else {
        ParserError("Expect a \",\" or \"}\" here in object literal!");
        return NULL;
      }
    } while (true);
  }
  return ret;
}

ast::AST* Parser::ParsePrimary() {
  ast::AST* ret;
  switch (m_lexer.lexeme().token) {
    case TK_INTEGER:
      ret = new (m_zone)
          ast::Integer(m_lexer.location(), m_lexer.lexeme().integer());
      m_lexer.Next();
      return ret;
    case TK_REAL:
      ret = new (m_zone) ast::Real(m_lexer.location(), m_lexer.lexeme().real());
      m_lexer.Next();
      return ret;
    case TK_TRUE:
      ret = new (m_zone) ast::Boolean(m_lexer.location(), true);
      m_lexer.Next();
      return ret;
    case TK_FALSE:
      ret = new (m_zone) ast::Boolean(m_lexer.location(), false);
      m_lexer.Next();
      return ret;
    case TK_NULL:
      ret = new (m_zone) ast::Null(m_lexer.location());
      m_lexer.Next();
      return ret;
    case TK_DURATION:
      ret = new (m_zone)
          ast::Duration(m_lexer.location(), m_lexer.lexeme().duration());
      m_lexer.Next();
      return ret;
    case TK_SIZE:
      ret = new (m_zone) ast::Size(m_lexer.location(), m_lexer.lexeme().size());
      m_lexer.Next();
      return ret;
    case TK_STRING:
      return ParseStringConcat();
    case TK_IF:
      return ParseTernary();
    case TK_SUB_ROUTINE:
      return ParseAnonymousSub();
    case TK_INTERP_START:
      return ParseStringInterpolation();
    case TK_VAR: {
      zone::ZoneString* prefix =
          zone::ZoneString::New(m_zone, m_lexer.lexeme().string());
      Token token;
      const vcl::util::CodeLocation loc = m_lexer.location();
      token = m_lexer.Next().token;
      if (IsPrefixOperator(token)) {
        return ParsePrefix(prefix);
      } else if (token == TK_LBRA) {
        return ParseExtensionLiteral(prefix);
      } else {
        return new (m_zone) ast::Variable(loc, prefix);
      }
    }
    case TK_LSQR:
      return ParseList();
    case TK_LBRA:
      return ParseDict();
    case TK_LPAR: {
      // Subexpression
      m_lexer.Next();
      ast::AST* ret = ParseExpression();
      if (!ret) return NULL;
      if (!m_lexer.Expect(TK_RPAR)) {
        ParserError("Expect \")\" to close a subexpression!");
        return NULL;
      }
      return ret;
    }
    default:
      ParserError(
          "unrecognize expression, expect a primary expression or a "
          "variable prefixed expression!");
      return NULL;
  }
}

ast::AST* Parser::ParseStringInterpolation() {
  DCHECK(m_lexer.lexeme().token == TK_INTERP_START);
  m_lexer.Next();
  ast::StringInterpolation* interp =
      new (m_zone) ast::StringInterpolation(m_lexer.location());
  do {
    switch (m_lexer.lexeme().token) {
      case TK_INTERP_END:
        if(m_lexer.Next().token == TK_INTERP_START) {
          m_lexer.Next(); // Yet another string interpolation section starts
          interp->list.Add(m_zone,
              new (m_zone) ast::String(m_lexer.location(),zone::ZoneString::New(m_zone,"\n")));
        } else {
          goto done;
        }
        break;
      case TK_CODE_START: {
        m_lexer.Next();  // Skip the ${

        ast::AST* expr = ParseExpression();
        if (!expr) return NULL;

        interp->list.Add(m_zone, expr);
        // Check if we have a } serve as the *end* of code segment
        if (m_lexer.lexeme().token != TK_RBRA) {
          ParserError(
              "Expect a \"}\" to close a code segment inside of string "
              "interpolation!");
          return NULL;
        }

        // NOTES: ==========================================================
        // The following code order is very subtle. We should always set the
        // lexer to mark it has CodeEnd *before* call Next since otherwise
        // another code path will be taken.
        m_lexer.SetCodeEnd();
        m_lexer.Next();
        break;
      }
      case TK_SEGMENT:
        interp->list.Add(
            m_zone,
            new (m_zone) ast::String(
                m_lexer.location(),
                zone::ZoneString::New(m_zone, m_lexer.lexeme().string())));
        m_lexer.Next();
        break;
      default:  // We should never see any other tokens
        VCL_UNREACHABLE();
        break;
    }
  } while (true);

done:
  return interp;
}

bool IsGeneratedVariableName( zone::ZoneString* name ) {
  return (name->size() >0) && ((*name)[0] == '@');
}

}  // namespace vm
}  // namespace vcl

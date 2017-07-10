#ifndef PARSER_H_
#define PARSER_H_
#include <string>
#include <gtest/gtest_prod.h>
#include "ast.h"

namespace vcl {
class Engine;
namespace vm  {

namespace test {
bool _test( const char* );
}

// Here is a simple BNF for VCL grammar. The formal definition of
// VCL is hard to find out. The grammar is based on my reading of
// the source code and examples/samples . Regardless of whether the
// real VCL has such grammar, our parser is based on the following
// grammar to compose.
//
// The grammar here is somehow extended for our own purpose but it
// mostly supports all VCL feature plus most of Fastly VCL feature,
// except the *goto* statement , which is kind of ugly to have it.
//
// We have some extension , including argument for sub routine and
// also return from sub routine , global variables , array support, etc ...
//
//
// We don't always use token name but sometimes prefer the literal
// of that token to make BNF easier to read.
// The % sign is a simple shortcuts for following situations :
// a%(DELIMITER) == a?(DEIMILTER a)*
//
// 1. Top level constructs
//
// top ::= include    |
//         import     |
//         extension  |
//         sub        |
//         acl        |
//         global     |
//         vcl_version
//
// include ::= INCLUDE STRING SEMICOLON.
// import  ::= IMPORT expression SEMICOLON.
// extension-prefix ::= VARIABLE VARIABLE
// extension  ::= VARIABLE VARIABLE extension-initializer SEMICOLON;
// extension-initializer ::= '{' extension-field%(';') '}'.
// extension-literal ::= VARIABLE extension-initializer.
// extension-field ::= DOT VARNAME '=' expr.
// sub ::= 'sub' VARNAME chunk
// acl ::= 'acl' VARNAME '{' (ip-pattern SEMICOLON)* '}'.
// numeric_ip ::= (STRING | STRING '/' INTEGER).
// ip-pattern ::= ( numeric_ip | '!' numeric_ip ).
// global     ::= 'global' VARNAME '=' expression;
// vcl_version ::= 'vcl' REAL SEMICOLON.
//
// 2. Statements
//
// chunk ::= '{' (statement SEMICOLON)* '}'
//
// statement ::= return |
//               call   |
//               prefix-call |
//               set    |
//               unset  |
//               new    |
//               declare|
//               if     |
//               for    |
//               break  |
//               continue
//
// return ::= 'return' (EMPTY | return_value | terminate).
// return_value ::= '{' expr? '}'.
// terminate    ::= '(' actlist | prefix ')'.
// actlist ::= ('ok' | 'fail' | 'pipe' | 'hash' | 'purge' |
//              'lookup' | 'restart' | 'fetch' | 'miss' | 'deliver' |
//              'retry'  | 'abandon' ).
// call ::= 'call' VARNAME (EMPTY | arglist).
// arglist ::= '(' expr%(',') ')'.
// set ::= 'set' left-hand-side ('='|'+='|'-='|'*='|'/='|'%=') right-hand-side.
// unset::= 'unset' left-hand-side.
// new ::= 'new' VARNAME '=' right-hand-side.
// declare ::= 'declare' VARNAME ( '=' right-hand-side | VARNAME ).
// if ::= 'if' '(' expr ')' single-statement-or-chunk else-list* else+.
// single-statement-or-chunk ::= statement SEMICOLON | chunk
// else-list ::= (ELIF|ELSIF|ELSEIF) '(' expr ')' single-statement-or-chunk.
// else ::= ELSE single-statement-or-chunk.
// left-hand-side ::= VARNAME ( DOT VARNAME |'[' expr ']'|arglist )* ( DOT VARNAME | '[' expr ']' ).
// right-hand-side ::= expr.
//
// for ::= 'for' '(' VARNAME (for-each-rest | for-rest)
// for-each-rest ::= ':' expression ')' single-statement-or-chunk
// for-rest      ::= ASSIGN expressino ';' expression ';' expression ';' ')' single-statement-or-chunk
//
// 3. Expression
//
// primary ::= INTEGER |
//          REAL    |
//          TRUE    |
//          FALSE   |
//          NULL    |
//          STRING  |
//          SIZE    |
//          DURATION|
//          VARIABLE|
//          extension-literal |
//          array-literal |
//          '(' expr ')'
//
// prefix ::= primary | VARIABLE ( DOT VARIABLE | '[' expr ']' | arglist )+.
// unary  ::= prefix  | ('-' | '!' | '+') prefix.
// factor ::= unary   | unary ('*' | '/' | '%') unary.
// term   ::= factor  | factor('+' | '-') factor.
// ineq   ::= term    | term ('>' | '<' | '>=' | '<=') term.
// eq     ::= ineq    | ineq  ('=='| '!=') ineq.
// and    ::= eq      | eq '&&' eq
// or     ::= and     | and '||' and
// ternary ::= or     | 'if' '('expr','expr','expr')'. // Fastly fashion
// array-literal ::=  '[' expr%(',') ']'
// expr   ::= ternary | extension-literal.
// ----------------------------------------------------------------------------
// The core parser of VCL grammar. User are *not* supposed to use this class
// directly since it is very low level. User just need to be aware of Engine
// class which already contains all the important APIs that allow user to do
// high level compilation.


class Parser {
 public:
  Parser( zone::Zone* zone ,
          const std::string& file_path ,
          const std::string& source    ,
          std::string* error ,
          int rand_name_ref = 0 ,
          bool support_loop = true ):
    m_zone(zone),
    m_file_path(file_path),
    m_source(source),
    m_lexer(source,file_path),
    m_error(error),
    m_file(NULL),
    m_lexical_scope(NULL),
    m_rand_name_seed(rand_name_ref),
    m_nested_loop(0),
    m_support_loop(support_loop)
  {}

  // The main function for parsing the target file and soruce
  ast::File* DoParse();

  // Used to generate random anonymous function name cross different
  // source files
  int rand_name_seed() const { return m_rand_name_seed; }

 private:
  void ParserError( const char* format , ... );
  zone::ZoneString* GetAnonymousSubName( zone::Zone* ) const;
  zone::ZoneString* GetTempVariableName( zone::Zone* ) const;

 private:
  bool IsPrefixOperator( Token tk ) {
    switch(tk) {
      case TK_COLON: case TK_DOT: case TK_LSQR:
      case TK_LPAR : case TK_FIELD:
        return true;
      default:
        return false;
    }
  }

  bool is_in_loop() const { return m_nested_loop > 0; }

 private: // Expression
  ast::AST* ParsePrimary();
  ast::List* ParseList();
  ast::Prefix* ParsePrefix( zone::ZoneString* prefix , int* last_component = NULL );
  ast::FuncCall* ParseFuncCall();
  ast::FuncCall* ParseMethodCall( ast::AST* );
  ast::FuncCall* ParseFuncCallArgument( ast::FuncCall* );
  ast::AST* ParseStringConcat();
  ast::AST* ParseAtomic();
  ast::ExtensionInitializer* ParseExtensionInitializer();
  ast::ExtensionLiteral* ParseExtensionLiteral( zone::ZoneString* );
  ast::Dict*ParseDict();
  ast::AST* ParseUnary();
  ast::AST* ParseBinaryPrecedence( int precedence );
  ast::AST* ParseBinary();
  ast::AST* ParseTernary();
  ast::AST* ParseExpression();
  ast::AST* ParseStringInterpolation();

 private: // Statement
  ast::AST* ParseReturnOrTerminate();
  ast::AST* ParseReturn();
  ast::AST* ParseTerminate();
  ast::AST* ParseCall();
  ast::AST* ParseLexScope();
  // Call a certain function with prefix or not , it should end up with
  // a function call regardless of what
  ast::AST* ParsePrefixCall();
  bool ParseLHS( ast::LeftHandSide* output );
  ast::AST* ParseSet();
  ast::AST* ParseUnset();
  template< int TK >
  ast::AST* ParseDeclareImpl();
  ast::AST* ParseDeclare() {
    CHECK(m_lexer.lexeme().token == TK_DECLARE);
    m_lexer.Next();
    return ParseDeclareImpl<TK_DECLARE>();
  }
  ast::AST* ParseNew() {
    CHECK(m_lexer.lexeme().token == TK_NEW);
    m_lexer.Next();
    return ParseDeclareImpl<TK_NEW>();
  }
  ast::AST* ParseIf();
  bool ParseBranch( ast::If::Branch* );

  // loop
  ast::AST* ParseFor();

  template< typename T >
  ast::AST* ParseLoopControlStmt() {
    if(!is_in_loop()) {
      ParserError("\"break\" or \"continue\" statement can only write inside "
                  "a loop body");
      return NULL;
    }
    T* ret = new (m_zone) T(m_lexer.location());
    m_lexer.Next();
    return ret;
  }

  // statement and body
  ast::AST* ParseStatementWithSemicolon();
  ast::AST* ParseStatement();
  ast::Chunk* ParseChunk();
  ast::Chunk* ParseSingleStatementOrChunk();
 private: // top level
  ast::AST* ParseInclude();
  ast::AST* ParseImport();
  ast::AST* ParseAnonymousSub();
  ast::AST* ParseSub();
  ast::AST* ParseSubDefinition( zone::ZoneString* );
  ast::AST* ParseExtension();
  ast::AST* ParseACL();
  bool ParseVCLVersion();
  ast::AST* ParseGlobalVariable();

 private:
  zone::Zone* m_zone;
  const std::string& m_file_path;
  const std::string& m_source;
  Lexer m_lexer;
  std::string* m_error;
  ast::File* m_file;
  ast::Chunk* m_lexical_scope;
  mutable int m_rand_name_seed;
  int m_nested_loop;      // how many level of loop we are in
  bool m_support_loop;    // whether we support loop as a valid grammar

  class EnterLexicalScope;
  friend class EnterLexicalScope;

 private: // For unittest
  FRIEND_TEST(Parser,Basic);
  FRIEND_TEST(Parser,Expression);
  FRIEND_TEST(Parser,Statement);
  FRIEND_TEST(Parser,File);
  friend bool test::_test(const char*);
};

} // namespace vm
} // namespace vcl

#endif // PARSER_H_

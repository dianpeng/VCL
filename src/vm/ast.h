#ifndef AST_H_
#define AST_H_
#include <iostream>
#include <vcl/util.h>

#include "vcl-pri.h"
#include "lexer.h"
#include "zone.h"

namespace vcl {
namespace vm  {
namespace ast {

#define VCL_AST_TYPE(__) \
  /* Top level */ \
  __(FILE,File,"file") \
  __(INCLUDE,Include,"include") \
  __(IMPORT,Import,"import") \
  __(SUB,Sub,"sub") \
  __(EXTENSION_INITIALIZER,ExtensionInitializer,"extension_initializer") \
  __(EXTENSION,Extension,"extension") \
  __(ACL,ACL,"acl") \
  __(GLOBAL,Global,"global") \
  /* Block level */ \
  __(CHUNK,Chunk,"chunk") \
  __(LEXSCOPE,LexScope,"lex_scope") \
  /* In subroutine */ \
  __(TERMINATE,Terminate,"terminate") \
  __(RETURN,Return,"return") \
  __(SET,Set,"set") \
  __(UNSET,Unset,"unset") \
  __(DECLARE,Declare,"declare") \
  __(IF,If,"if") \
  __(FOR,For,"for") \
  __(BREAK,Break,"break") \
  __(CONTINUE,Continue,"continue") \
  __(STMT,Stmt,"stmt") \
  /* Expression level */ \
  __(TERNARY,Ternary,"ternary") \
  __(BINARY,Binary,"binary") \
  __(UNARY,Unary,"unary") \
  __(PREFIX,Prefix,"prefix") \
  __(FUNCCALL,FuncCall,"funccall") \
  __(EXTENSION_LITERAL,ExtensionLiteral,"extension_literal") \
  __(DICT,Dict,"dict") \
  /* Primary */ \
  __(STRING_CONCAT,StringConcat,"string_concat") \
  __(INTEGER,Integer,"integer") \
  __(REAL,Real,"real") \
  __(BOOLEAN,Boolean,"boolean") \
  __(NULL,Null,"null") \
  __(STRING,String,"string") \
  __(VARIABLE,Variable,"variable") \
  __(DURATION,Duration,"duration") \
  __(LIST,List,"list") \
  __(SIZE,Size,"size") \
  __(STRING_INTERPOLATION,StringInterpolation,"string_interpolation")

enum ASTType {
#define __(A,B,C) AST_##A,
  VCL_AST_TYPE(__)
  SIZE_OF_AST
#undef __ // __
};

// Forward class declaration
#define __(A,B,C) struct B;
VCL_AST_TYPE(__)
#undef __ // __

const char* GetASTName( ASTType );

// Nothing special. A basic Abstract Syntax Tree C++ version implementation.
// The only thing here is that we prefer using POD for all these AST node.
// And also all AST a ZoneObject which means its memory should live on a zone.
// Additionally, make sure all AST have trivial destructor otherwise it won't
// compile.

struct AST : public zone::ZoneObject {
  ASTType type;
  vcl::util::CodeLocation location;

  AST( const vcl::util::CodeLocation& loc , ASTType t ):
    type(t),
    location(loc)
  {}

  void* operator new ( size_t size , zone::Zone* zone ) {
    return zone->Malloc<void*>(size);
  }

 private:
  // Forbid
  void* operator new (size_t);
  VCL_DISALLOW_COPY_AND_ASSIGN(AST);
};

struct Include : public AST {
  Include(const vcl::util::CodeLocation& loc , zone::ZoneString* path ):
    AST(loc,AST_INCLUDE),
    path(path)
  {}

  zone::ZoneString* path;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Include);
};

struct Import : public AST {
  Import(const vcl::util::CodeLocation& loc , zone::ZoneString* name ):
    AST(loc,AST_IMPORT),
    module_name(name)
  {}

  zone::ZoneString* module_name;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Import);
};

struct Chunk : public AST {
  Chunk(const vcl::util::CodeLocation& loc):
    AST(loc,AST_CHUNK),
    statement_list(),
    location_end  (loc)
  {}

  zone::ZoneVector<AST*> statement_list;
  // Where is the *end* of this Chunk
  vcl::util::CodeLocation  location_end;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Chunk);
};

struct LexScope : public AST {
  LexScope(const vcl::util::CodeLocation& loc):
    AST(loc,AST_LEXSCOPE),
    body(NULL)
  {}

  Chunk* body;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(LexScope);
};

struct Sub : public AST {
  Sub(const vcl::util::CodeLocation& loc):
    AST(loc,AST_SUB),
    arg_list(),
    sub_name(NULL),
    body(NULL)
  {}

  static std::string FormatProtocol( const Sub& );

  zone::ZoneVector<zone::ZoneString*> arg_list;
  zone::ZoneString* sub_name;
  Chunk* body;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Sub);
};

struct ExtensionLiteral : public AST {
  ExtensionLiteral( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_EXTENSION_LITERAL),
    type_name(NULL),
    initializer(NULL)
  {}

  zone::ZoneString* type_name;
  ExtensionInitializer* initializer;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(ExtensionLiteral);
};


struct ExtensionInitializer: public AST {
  struct ExtensionField {
    zone::ZoneString* name;
    AST* value;
  };
  zone::ZoneVector<ExtensionField> list;

  ExtensionInitializer( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_EXTENSION_INITIALIZER),
    list()
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(ExtensionInitializer);
};

struct Extension : public AST {
  Extension(const vcl::util::CodeLocation& loc):
    AST(loc,AST_EXTENSION),
    type_name(NULL),
    instance_name(NULL),
    initializer(NULL)
  {}

  zone::ZoneString* type_name;
  zone::ZoneString* instance_name;
  ExtensionInitializer* initializer;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Extension);
};

struct Dict : public AST {
  Dict(const vcl::util::CodeLocation& loc):
    AST(loc,AST_DICT),
    list()
  {}

  struct Entry {
    AST* key;
    AST* value;
    Entry():
      key(NULL),
      value(NULL)
    {}
    Entry(AST* k ,AST* v):
      key(k),
      value(v)
    {}
  };

  zone::ZoneVector<Entry> list;

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Dict);
};

struct ACL : public AST {
  ACL(const vcl::util::CodeLocation& loc):
    AST(loc,AST_ACL),
    name(NULL),
    list()
  {}

  struct ACLItem {
    zone::ZoneString* name;
    uint8_t mask;
    bool negative;
    ACLItem():
      name(NULL),
      mask(0),
      negative(false)
    {}
  };

  zone::ZoneString* name;
  zone::ZoneVector<ACLItem> list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(ACL);
};

struct Global : public AST {
  Global(const vcl::util::CodeLocation& loc):
    AST(loc,AST_GLOBAL),
    name(NULL),
    value(NULL)
  {}
  zone::ZoneString* name;
  AST* value;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Global);
};

struct List : public AST {
  List(const vcl::util::CodeLocation& loc):
    AST(loc,AST_LIST),
    list()
  {}

  zone::ZoneVector<AST*> list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(List);
};

struct Return : public AST {
  Return(const vcl::util::CodeLocation& loc):
    AST(loc,AST_RETURN),
    value(NULL)
  {}

  Return(const vcl::util::CodeLocation& loc , AST* v):
    AST(loc,AST_RETURN),
    value(v)
  {}

  AST* value;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Return);
};

struct Terminate : public AST {
  Terminate( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_TERMINATE),
    value(NULL),
    action()
  {}

  AST* value;
  ActionType action;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Terminate);
};

struct LeftHandSide {
  int type;
  enum { VARIABLE , PREFIX };
  union {
    zone::ZoneString* variable;
    ast::Prefix* prefix;
  };

  explicit LeftHandSide( zone::ZoneString* v ):
    type(VARIABLE),
    variable(v)
  {}

  explicit LeftHandSide( ast::Prefix* p ):
    type(PREFIX),
    prefix(p)
  {}

  LeftHandSide():
    type(0)
  {}
};

struct Set : public AST {
  Set(const vcl::util::CodeLocation& loc):
    AST(loc,AST_SET),
    lhs(),
    rhs(NULL),
    op(TK_ERROR)
  {}

  LeftHandSide lhs;
  AST* rhs;
  Token op;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Set);
};

struct Unset: public AST {
  Unset(const vcl::util::CodeLocation& loc):
    AST(loc,AST_UNSET),
    lhs()
  {}

  LeftHandSide lhs;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Unset);
};

struct Declare : public AST {
  Declare(const vcl::util::CodeLocation& loc):
    AST(loc,AST_DECLARE),
    variable(NULL),
    rhs(NULL)
  {}

  zone::ZoneString* variable;
  AST* rhs;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Declare);
};

struct If : public AST {
  struct Branch {
    AST* condition;
    Chunk* body;
    Branch():condition(NULL),body(NULL){}
  };
  If( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_IF),
    branch_list()
  {}
  zone::ZoneVector<Branch> branch_list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(If);
};

struct For : public AST {
  AST* iterator;
  zone::ZoneString* key;
  zone::ZoneString* val;
  Chunk* body;
  For( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_FOR),
    iterator(NULL),
    key(NULL),
    val(NULL),
    body(NULL)
  {}
};

struct Continue : public AST {
  Continue( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_CONTINUE)
  {}
};

struct Break : public AST {
  Break( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_BREAK)
  {}
};

struct Ternary : public AST {
  Ternary( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_TERNARY),
    condition(NULL),
    first(NULL),
    second(NULL)
  {}

  AST* condition;
  AST* first;
  AST* second;

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Ternary);
};

struct Binary : public AST {
  Binary( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_BINARY),
    lhs(NULL),
    rhs(NULL),
    op(TK_ERROR)
  {}

  Binary( const vcl::util::CodeLocation& loc ,
          AST* l,
          AST* r,
          Token o):
    AST(loc,AST_BINARY),
    lhs(l),
    rhs(r),
    op(o)
  {}

  AST* lhs;
  AST* rhs;
  Token op;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Binary);
};

struct Unary : public AST {
  Unary( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_UNARY),
    ops(),
    operand(NULL)
  {}

  zone::ZoneVector<Token> ops;
  AST* operand;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Unary);
};

struct Prefix : public AST {
  Prefix( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_PREFIX),
    list()
  {}

  struct Component {
    enum { CALL = 0 , INDEX , DOT , ATTRIBUTE };
    int tag;
    union {
      FuncCall* funccall;
      AST* expression;
      zone::ZoneString* var;
    };

    Component( FuncCall* fc ):
      tag(CALL),
      funccall(fc)
    {}

    Component( AST* expr ):
      tag(INDEX),
      expression(expr)
    {}

    Component( zone::ZoneString* v , int tag = DOT ):
      tag(tag),
      var(v)
    { DCHECK(tag == DOT || tag == ATTRIBUTE); }
  };

  zone::ZoneVector<Component> list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Prefix);
};

struct Stmt : public AST {
  Stmt( const vcl::util::CodeLocation& loc , AST* e ):
    AST(loc,AST_STMT),
    expr(e)
  {}

  AST* expr;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Stmt);
};

struct FuncCall : public AST {
  FuncCall( const vcl::util::CodeLocation& loc , bool m = false ):
    AST(loc,AST_FUNCCALL),
    name(NULL),
    argument()
  {}

  zone::ZoneString* name;
  zone::ZoneVector<AST*> argument;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(FuncCall);
};

struct StringConcat : public AST {
  StringConcat( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_STRING_CONCAT),
    list()
  {}

  zone::ZoneVector<zone::ZoneString*> list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(StringConcat);
};

struct Integer : public AST {
  int32_t value;
  Integer( const vcl::util::CodeLocation& loc , int32_t v ):
    AST(loc,AST_INTEGER),
    value(v)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Integer);
};

struct Real : public AST {
  double value;
  Real( const vcl::util::CodeLocation& loc , double v ):
    AST(loc,AST_REAL),
    value(v)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Real);
};

struct Boolean : public AST {
  bool value;
  Boolean( const vcl::util::CodeLocation& loc , bool v ):
    AST(loc,AST_BOOLEAN),
    value(v)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Boolean);
};

struct Null : public AST {
  Null( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_NULL)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Null);
};

struct String : public AST {
  zone::ZoneString* value;
  String( const vcl::util::CodeLocation& loc , zone::ZoneString* v ):
    AST(loc,AST_STRING),
    value(v)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(String);
};

struct Variable : public AST {
  zone::ZoneString* value;

  Variable( const vcl::util::CodeLocation& loc , zone::ZoneString* v ):
    AST(loc,AST_VARIABLE),
    value(v)
  {}

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Variable);
};

struct Duration : public AST {
  vcl::util::Duration value;
  Duration( const vcl::util::CodeLocation& loc , const vcl::util::Duration& v ):
    AST(loc,AST_DURATION),
    value(v)
  {}
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Duration);
};

struct Size : public AST {
  vcl::util::Size value;
  Size( const vcl::util::CodeLocation& loc , const vcl::util::Size& v ):
    AST(loc,AST_SIZE),
    value(v)
  {}
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(Size);
};

struct StringInterpolation : public AST {
  StringInterpolation( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_STRING_INTERPOLATION),
    list()
  {}
  zone::ZoneVector<ast::AST*> list;
 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(StringInterpolation);
};

struct File : public AST {
  File( const vcl::util::CodeLocation& loc ):
    AST(loc,AST_FILE),
    chunk(NULL)
  {}

  Chunk* chunk;

 private:
  VCL_DISALLOW_COPY_AND_ASSIGN(File);
};

// Helper function to create specialized AST object
Declare* NewTempVariableDeclare( zone::Zone* , zone::ZoneString* , Prefix* ,
    const vcl::util::CodeLocation& );

// Serialize to s-expression to ease pain of future testing
void ASTSerialize( const File& file , std::ostream* output );

#define __(A,B,C) std::ostream& operator << ( std::ostream& , const B& );
VCL_AST_TYPE(__)
#undef __ // __
std::ostream& operator << ( std::ostream& , const AST& );


} // namespace ast
} // namespace vm
} // namespace vcl

#endif // AST_H_

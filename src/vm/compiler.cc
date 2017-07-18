#include "compiler.h"
#include "zone.h"
#include "bytecode.h"
#include "compilation-unit.h"
#include "procedure.h"
#include "ip-address.h"
#include "vcl-pri.h"

// A macro to explicit mark where we emit the bytecode
#define __ m_procedure->code_buffer().

namespace {
using namespace vcl;
using namespace vcl::vm;
using namespace vcl::vm::zone;

static const size_t kMaxLocalVarSize = kMaxArg;
class LexicalScope;

// Core compiler constructs. Normal code generator by visiting AST
// one by one
class Compiler {
 public:
  Compiler( CompiledCode* cc , zone::Zone* zone , const CompilationUnit& cu ,
      std::string* error ):
    m_unit(cu),
    m_cc(cc),
    m_procedure(NULL),
    m_error(error),
    m_lex_scope(NULL),
    m_cur_source_index(0),
    m_zone(zone)
  {}

  bool DoCompile();

 private:
  bool Compile( const CompilationUnit& cu );
  bool Compile( const ast::AST& );
  bool CompileChunk( const ast::Chunk& body );
  bool CompileChunkNoLexicalScope( const ast::Chunk& );

  // Global
  bool Compile( const ast::Import& );
  bool Compile( const ast::Global& );
  bool Compile( const ast::Extension& );
  bool Compile( const ast::Sub& );
  bool CompileAnonymousSub( const ast::Sub& );
  bool Compile( const CompilationUnit::SubList& );
  bool Compile( const ast::ACL& );

  // Stmt
  bool Compile( const ast::Stmt& );
  bool Compile( const ast::Declare& );
  bool Compile( const ast::Set& );
  bool Compile( const ast::Unset& );
  bool Compile( const ast::Return& );
  bool Compile( const ast::Terminate& );
  bool Compile( const ast::If& );
  bool Compile( const ast::LexScope& );
  bool Compile( const ast::Break& );
  bool Compile( const ast::Continue& );
  bool Compile( const ast::For& );

  // Expression
  bool Compile( const ast::ExtensionInitializer& );
  bool Compile( const ast::ExtensionLiteral& );
  bool Compile( const ast::List& );
  bool Compile( const ast::Dict& );
  bool Compile( const ast::Size& );
  bool Compile( const ast::Duration& );
  bool Compile( const ast::String& );
  bool Compile( const ast::Null& null ) {
    __ lnull(null.location);
    return true;
  }
  bool Compile( const ast::Boolean& boolean ) {
    if(boolean.value) {
      __ ltrue(boolean.location);
    } else {
      __ lfalse(boolean.location);
    }
    return true;
  }
  bool Compile( const ast::Integer& );
  bool Compile( const ast::Real& );
  bool Compile( const ast::StringInterpolation& );
  bool Compile( const ast::StringConcat& );
  bool Compile( const ast::Unary& );
  bool Compile( const ast::Binary& );
  bool CompileLogic( const ast::Binary& );
  bool Compile( const ast::Ternary& );
  bool CompileLHSPrefix( const ast::Prefix& );
  bool Compile( const ast::Prefix& );
  bool CompilePrefixList( const vcl::util::CodeLocation& loc ,
                          const zone::ZoneVector< ast::Prefix::Component>& ,
                          size_t );

  enum {
    CATEGORY_PROP,
    CATEGORY_ATTR,
    CATEGORY_INDEX,
    SIZE_OF_CATEGORY
  };

  enum {
    OP_SET,
    OP_SADD,
    OP_SSUB,
    OP_SMUL,
    OP_SDIV,
    OP_SMOD,
    OP_UNSET,
    SIZE_OF_OP
  };

  template< int CATE , int OP >
  Bytecode BuildBytecode() {
    static const Bytecode kInstrTable[SIZE_OF_CATEGORY][SIZE_OF_OP] = {
      { BC_PSET, BC_PSADD , BC_PSSUB, BC_PSMUL, BC_PSDIV, BC_PSMOD, BC_PUNSET },
      { BC_ASET, BC_ASADD , BC_ASSUB, BC_ASMUL, BC_ASDIV, BC_ASMOD, BC_AUNSET },
      { BC_ISET, BC_ISADD , BC_ISSUB, BC_ISMUL, BC_ISDIV, BC_ISMOD, BC_IUNSET }
    };
    return kInstrTable[CATE][OP];
  }

  template< int OP >
  bool CompileComponent( const vcl::util::CodeLocation& loc ,
      const ast::Prefix::Component& comp );

  bool Compile( const ast::Variable& var ) {
    return CompileVariable(var.location,var.value);
  }

  bool CompileFuncCall( const ast::FuncCall& );

  template< typename T >
  int CompileString( const vcl::util::CodeLocation& loc ,
      const T& value ) {
    int index = m_procedure->Add(m_cc->gc(),value);
    if(!BytecodeBuffer::CheckOperand(index)) {
      ReportError(loc,"too many string literals!");
      return -1;
    }
    return index;
  }

  template< typename T >
  int CompileLiteral( const vcl::util::CodeLocation& loc ,
      const T& value ) {
    int index = m_procedure->Add(value);
    if(!BytecodeBuffer::CheckOperand(index)) {
      ReportError(loc,"too many literals!");
      return -1;
    }
    return index;
  }

  bool CompileVariable( const vcl::util::CodeLocation& loc ,
      zone::ZoneString* );

 private:
  void ReportError( const vcl::util::CodeLocation& loc ,
                    const char* format , ... );

 private:
  const CompilationUnit& m_unit;
  CompiledCode* m_cc;
  Procedure* m_procedure;
  std::string* m_error;
  LexicalScope* m_lex_scope;
  uint32_t m_cur_source_index;
  zone::Zone* m_zone;

  friend class LexicalScope;
  VCL_DISALLOW_COPY_AND_ASSIGN(Compiler);
};

// Represent Lexical Scope formed in the source code. It is used to
// manage all the local variable and stack status
class LexicalScope {
 public:
  LexicalScope( Compiler* comp , Procedure* procedure,
      const vcl::util::CodeLocation& location ,
      bool no_pop = false ,
      bool is_loop = false,
      bool is_function = false):
    m_base(0),
    m_var (),
    m_parent(comp->m_lex_scope),
    m_procedure(procedure),
    m_location(location),
    m_compiler(comp),
    m_no_pop  (no_pop),
    m_is_direct_loop(is_loop),
    m_is_in_loop    (false),
    m_breaks(),
    m_continues(),
    m_iter_prefix(0)
  {
    if(!is_function) m_base = m_parent ? m_parent->base() : 0;
    comp->m_lex_scope = this;
    if(m_parent)
      m_is_in_loop = ( m_parent->m_is_direct_loop || m_parent->m_is_in_loop );
  }

  LexicalScope( Compiler* comp , Procedure* procedure ):
    m_base(0),
    m_var () ,
    m_parent(comp->m_lex_scope),
    m_procedure( procedure )   ,
    m_location(),
    m_compiler(comp),
    m_no_pop  (true),
    m_is_direct_loop(false),
    m_is_in_loop(false),
    m_breaks(),
    m_continues(),
    m_iter_prefix(0)
  {
    comp->m_lex_scope = this;
  }

  ~LexicalScope();
 public:

  LexicalScope* parent() const {
    return m_parent;
  }
  bool is_direct_loop() const { return m_is_direct_loop; }
  bool is_in_loop() const     { return m_is_in_loop; }
  bool is_loop_scope() const  { return is_direct_loop() || is_in_loop(); }
  int base() const {
    return m_base + static_cast<int>(m_var.size());
  }

 public:
  bool DefineIterator( Zone* );
  enum {
    LOCAL_TOO_MUCH = -1 ,
    LOCAL_DUPLICATE= -2
  };

  int DefineLocal ( ZoneString* );
  int LookupLocal ( ZoneString* );
  int Lookup      ( ZoneString* );

 public: // Loop control related

  BytecodeBuffer::Label AddBreak  ( BytecodeBuffer* bb , const vcl::util::CodeLocation& loc ) {
    BytecodeBuffer::Label ret = bb->brk(loc);
    m_breaks.push_back(ret);
    return ret;
  }

  BytecodeBuffer::Label AddContinue(BytecodeBuffer* bb , const vcl::util::CodeLocation& loc ) {
    BytecodeBuffer::Label ret = bb->cont(loc);
    m_continues.push_back(ret);
    return ret;
  }

  void FixBreak( size_t position ) {
    for( size_t i = 0 ; i < m_breaks.size(); ++i ) {
      m_breaks[i].Patch(static_cast<uint32_t>(position));
    }
  }

  void FixContinues( size_t position ) {
    for( size_t i = 0 ; i < m_continues.size() ; ++i ) {
      m_continues[i].Patch(static_cast<uint32_t>(position));
    }
  }

 public:
  // Manually generate bytecode for poping all variables defined inside of this
  // lexical scope. It should only be used in specific cases and cautiouos since
  // the destructor of the LexicalScope can generate pop instruction for all
  // local variables and it is mostly good enough for many cases.
  void PopAllVariablesInScope() {
    __ spop(m_location,static_cast<int>(m_var.size()));
  }

 public:
  // This function will walk the lexical scope upwards and accumulate *all*
  // variable size defined in each scope until it reaches the outer loop
  size_t GetVariableSizeUntilLoopScope() const;
  LexicalScope* GetLoopScope();

 private:
  int m_base;
  std::vector<ZoneString*> m_var;
  LexicalScope* m_parent;
  Procedure* m_procedure;
  vcl::util::CodeLocation m_location;
  Compiler* m_compiler;
  bool m_no_pop;

  // Loop related stuff
  bool m_is_direct_loop; // Whether this lexical scope is the direct scope
  bool m_is_in_loop;     // Whether this lexical scope is nested inside of loop
  std::vector<BytecodeBuffer::Label> m_breaks;
  std::vector<BytecodeBuffer::Label> m_continues;
  size_t m_iter_prefix;

  VCL_DISALLOW_COPY_AND_ASSIGN(LexicalScope);
};

// Define a random variable name to hold the internal iterator object place
bool LexicalScope::DefineIterator( Zone* zone ) {
  ++m_iter_prefix;
  int ret = DefineLocal(
      ZoneString::New(zone,vcl::util::Format("@_%d_iter",m_iter_prefix)));
  DCHECK( ret != LOCAL_DUPLICATE );
  return ret >= 0;
}

int LexicalScope::DefineLocal( ZoneString* zone_string ) {
  int ret;
  if((ret = LookupLocal(zone_string)) >=0) return LOCAL_DUPLICATE;
  if(m_var.size() == kMaxLocalVarSize)
    return LOCAL_TOO_MUCH; // Cannot handle, just too much local variables
  m_var.push_back( zone_string );
  return m_base + static_cast<int>(m_var.size());
}

int LexicalScope::LookupLocal( ZoneString* zone_string ) {
  for( size_t i = 0 ; i < m_var.size() ; ++i ) {
    if( (*m_var[i]) == *zone_string )
      return static_cast<int>( i ) + m_base;
  }
  return -1;
}

int LexicalScope::Lookup( ZoneString* zone_string ) {
  LexicalScope* scope = this;
  do {
    int ret = scope->LookupLocal(zone_string);
    if(ret >= 0) return ret;
    scope = scope->m_parent;
  } while(scope);
  return -1;
}

size_t LexicalScope::GetVariableSizeUntilLoopScope() const {
  const LexicalScope* scope = this;
  size_t count = 0;
  do {
    count += scope->m_var.size();
    scope = scope->m_parent;
    DCHECK(scope);
    if(scope->is_direct_loop()) {
      count += scope->m_var.size();
      break;
    }
  } while(true);
  return count;
}

LexicalScope* LexicalScope::GetLoopScope() {
  LexicalScope* scope = this;
  while( scope ) {
    if(scope->is_direct_loop()) break;
    scope = scope->m_parent;
  }
  DCHECK(scope);
  return scope;
}

LexicalScope::~LexicalScope() {
  if(m_var.size() && !m_no_pop )
    PopAllVariablesInScope();
  m_compiler->m_lex_scope = m_parent;
}

void Compiler::ReportError( const vcl::util::CodeLocation& loc ,
                            const char* format ,
                            ... ) {
  va_list vlist;
  va_start(vlist,format);
  *m_error = vcl::util::ReportErrorV(
      m_cc->IndexSourceCodeInfo(m_cur_source_index)->source_code,
      loc,
      "[compiler]",
      format,
      vlist);
}

bool Compiler::Compile( const ast::LexScope& body ) {
  return CompileChunk(*body.body);
}

bool Compiler::CompileChunk( const ast::Chunk& body ) {
  LexicalScope scope(this,m_procedure,body.location_end);
  return CompileChunkNoLexicalScope(body);
}

bool Compiler::Compile( const ast::Size& size ) {
  int index = CompileLiteral(size.location,size.value);
  if(index <0) return false;
  __ lsize(size.location,index);
  return true;
}

bool Compiler::Compile( const ast::Duration& duration ) {
  int index = CompileLiteral(duration.location,duration.value);
  if(index <0) return false;
  __ lduration(duration.location,index);
  return true;
}

bool Compiler::Compile( const ast::String& string ) {
  int index = CompileString(string.location,string.value);
  if(index <0) return false;
  __ lstr(string.location,index);
  return true;
}

bool Compiler::Compile( const ast::Integer& integer ) {
  int index = CompileLiteral(integer.location,integer.value);
  if(index < 0) return false;
  __ lint(integer.location,index);
  return true;
}

bool Compiler::Compile( const ast::Real& real ) {
  int index = CompileLiteral(real.location,real.value);
  if(index < 0) return false;
  __ lreal(real.location,index);
  return true;
}

bool Compiler::Compile( const ast::List& list ) {
  // Generate List's literal
  for( size_t i = 0 ; i < list.list.size() ; ++i ) {
    if(!Compile(*list.list[i])) return false;
  }
  __ llist(list.location,static_cast<uint32_t>(list.list.size()));
  return true;
}

bool Compiler::Compile( const ast::Dict& dict ) {
  // Generate dict's literal
  for( size_t i = 0 ; i < dict.list.size() ; ++i ) {
    const ast::Dict::Entry& e = dict.list[i];
    if(!Compile(*e.key)) return false;
    if(!Compile(*e.value)) return false;
  }
  __ ldict(dict.location,static_cast<uint32_t>(dict.list.size()));
  return true;
}

bool Compiler::Compile( const ast::ExtensionInitializer& initializer ) {
  for( size_t i = 0 ; i < initializer.list.size() ; ++i ) {
    const ast::ExtensionInitializer::ExtensionField& field =
      initializer.list[i];
    // Key
    {
      int id = CompileString(field.value->location,field.name);
      if(id <0) return false;
      __ lstr(field.value->location,id);
    }
    // Value
    if(!Compile(*field.value))
      return false;
  }
  __ lext(initializer.location,static_cast<uint32_t>(
        initializer.list.size()));
  return true;
}

bool Compiler::Compile( const ast::ExtensionLiteral& literal ) {
  int id = CompileString(literal.location,literal.type_name);
  if(id <0) return false;
  __ lstr(literal.location,id);
  if(!Compile(*literal.initializer)) return false;
  return true;
}

bool Compiler::Compile( const ast::StringConcat& sc ) {
  std::string buffer;
  for( size_t i = 0 ; i < sc.list.size() ; ++i ) {
    zone::ZoneString* str = sc.list[i];
    buffer.append( str->data() );
  }
  int index = CompileString(sc.location,buffer);
  if(index <0) return false;
  __ lstr(sc.location,index);
  return true;
}

bool Compiler::Compile( const ast::StringInterpolation& node ) {
  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    const ast::AST* e = node.list.Index(i);
    if(e->type == ast::AST_STRING) {
      int index = CompileString(e->location,
                                static_cast<const ast::String*>(e)->value);
      if(index <0) return false;
      __ lstr(e->location,index);
    } else {
      if(!Compile(*e)) return false;
      __ cstr(e->location);
    }
  }
  __ scat( node.location , static_cast<uint32_t>(node.list.size()) );
  return true;
}

bool Compiler::Compile( const ast::Unary& unary ) {
  if(!Compile(*unary.operand)) return false;
  for( size_t i = 0 ; i < unary.ops.size() ; ++i ) {
    Token tk = unary.ops[i];
    switch(tk) {
      case TK_ADD: break;
      case TK_SUB: __ negate(unary.location); break;
      case TK_NOT: __ flip(unary.location); break;
      default: VCL_UNREACHABLE(); break;
    }
  }
  return true;
}

bool Compiler::Compile( const ast::Binary& binary ) {
  if(TokenIsLogicOperator(binary.op)) {
    return CompileLogic(binary);
  } else {
    // Check whether at least one of the operand is a constant integer value
    // which has specialized instruction to fasten the operations later on
    // during the interpreter generation
    if(binary.lhs->type == ast::AST_INTEGER) {
      if(!Compile(*binary.rhs)) return false;
      int index = CompileLiteral(binary.lhs->location,
          static_cast<const ast::Integer*>(binary.lhs)->value);
      if(index <0) return false;
      switch(binary.op) {
        case TK_ADD: __ addiv(binary.location,index); break;
        case TK_SUB: __ subiv(binary.location,index); break;
        case TK_MUL: __ muliv(binary.location,index); break;
        case TK_DIV: __ diviv(binary.location,index); break;
        case TK_MOD: __ modiv(binary.location,index); break;
        case TK_LT : __ ltiv (binary.location,index); break;
        case TK_LE : __ leiv (binary.location,index); break;
        case TK_GT : __ gtiv (binary.location,index); break;
        case TK_GE : __ geiv (binary.location,index); break;
        case TK_EQ : __ eqiv (binary.location,index); break;
        case TK_NE : __ neiv (binary.location,index); break;
        default:
          ReportError(binary.location,"match/unmatch operator used with integer type");
          return false;
      }
    } else if(binary.rhs->type == ast::AST_INTEGER) {
      if(!Compile(*binary.lhs)) return false;
      int index = CompileLiteral(binary.rhs->location,
          static_cast<const ast::Integer*>(binary.rhs)->value);
      if(index <0) return false;
      switch(binary.op) {
        case TK_ADD: __ addvi(binary.location,index); break;
        case TK_SUB: __ subvi(binary.location,index); break;
        case TK_MUL: __ mulvi(binary.location,index); break;
        case TK_DIV: __ divvi(binary.location,index); break;
        case TK_MOD: __ modvi(binary.location,index); break;
        case TK_LT : __ ltvi (binary.location,index); break;
        case TK_LE : __ levi (binary.location,index); break;
        case TK_GT : __ gtvi (binary.location,index); break;
        case TK_GE : __ gevi (binary.location,index); break;
        case TK_EQ : __ eqvi (binary.location,index); break;
        case TK_NE : __ nevi (binary.location,index); break;
        default:
          ReportError(binary.location,"match/unmatch operator used with integer type");
          return false;
      }
    } else {
      if(!Compile(*binary.lhs)) return false;
      if(!Compile(*binary.rhs)) return false;
      switch( binary.op ) {
        case TK_ADD :
          __ add(binary.location); break;
        case TK_SUB:
          __ sub(binary.location); break;
        case TK_MUL:
          __ mul(binary.location); break;
        case TK_DIV:
          __ div(binary.location); break;
        case TK_MOD:
          __ mod(binary.location); break;
        case TK_LT:
          __ lt(binary.location); break;
        case TK_LE:
          __ le(binary.location); break;
        case TK_GT:
          __ gt(binary.location); break;
        case TK_GE:
          __ ge(binary.location); break;
        case TK_EQ:
          __ eq(binary.location); break;
        case TK_NE:
          __ ne(binary.location); break;
        case TK_MATCH:
          __ match(binary.location); break;
        case TK_NOT_MATCH:
          __ nmatch(binary.location); break;
        default:
          VCL_UNREACHABLE();
          return false;
      }
    }
  }
  return true;
}

bool Compiler::CompileLogic( const ast::Binary& logic ) {
  DCHECK( logic.op == TK_AND || logic.op == TK_OR );
  // 1. Generate first operand /lhs
  if(!Compile(*logic.lhs)) return false;
  BytecodeBuffer::Label forward_label;

  if( logic.op == TK_AND ) {
    forward_label = __ brf( logic.location );
  } else {
    forward_label = __ brt( logic.location );
  }

  // 2. Generate second operand /rhs
  if(!Compile(*logic.rhs)) return false;
  __ test(logic.location);

  // 3. Set the jump out label position
  forward_label.Patch( m_procedure->code_buffer().position() );
  return true;
}

bool Compiler::Compile( const ast::Ternary& ternary ) {
  if(!Compile(*ternary.condition)) return false;

  // 1. Jump if condition is false
  BytecodeBuffer::Label forward_label = __ brf( ternary.location );

  // 2. Fall througth to condition is true
  if(!Compile(*ternary.first)) return false;
  BytecodeBuffer::Label exit_label = __ jmp(ternary.location);

  // 3. Here we have false condition value
  forward_label.Patch( m_procedure->code_buffer().position() );
  if(!Compile(*ternary.second)) return false;

  // 4. Fall through to exit position
  exit_label.Patch(m_procedure->code_buffer().position());

  return true;
}

bool Compiler::Compile( const ast::Stmt& stmt ) {
  if(!Compile(*stmt.expr))
    return false;
  __ spop(stmt.location,1);
  return true;
}

bool Compiler::Compile( const ast::Declare& dec ) {
  if(dec.rhs) {
    if(!Compile(*dec.rhs)) return false;
  } else {
    __ lnull(dec.location);
  }
  DCHECK(m_lex_scope);
  int idx = m_lex_scope->DefineLocal( dec.variable );
  if(idx == LexicalScope::LOCAL_DUPLICATE) {
    ReportError(dec.location,"Variable %s has been defined before!",
        dec.variable->data());
    return false;
  } else if(idx == LexicalScope::LOCAL_TOO_MUCH) {
    ReportError(dec.location,"Too much local variables!");
    return false;
  }
  return true;
}

bool Compiler::Compile( const ast::Prefix& pref ) {
  return CompilePrefixList(pref.location,
      pref.list,pref.list.size());
}

bool Compiler::CompilePrefixList( const vcl::util::CodeLocation& loc ,
    const zone::ZoneVector<ast::Prefix::Component>& list, size_t end ) {
  const ast::Prefix::Component& first = list.First();
  CHECK(first.tag == ast::Prefix::Component::DOT);
  if(!CompileVariable(loc,first.var)) return false;
  DCHECK(end <= list.size());
  for( size_t i = 1 ; i < end ; ++i ) {
    const ast::Prefix::Component& comp = list[i];
    switch(comp.tag) {
      case ast::Prefix::Component::CALL:
        if(!CompileFuncCall(*comp.funccall)) return false;
        break;
      case ast::Prefix::Component::INDEX:
        {
          if(!Compile(*comp.expression)) return false;
          __ iget(comp.expression->location);
          break;
        }
      case ast::Prefix::Component::DOT:
        {
          int id = CompileString(loc,comp.var);
          if(id <0) return false;
          __ pget(loc,id);
          break;
        }
      case ast::Prefix::Component::ATTRIBUTE:
        {
          int id = CompileString(loc,comp.var);
          if(id <0) return false;
          __ aget(loc,id);
          break;
        }
      default:
        VCL_UNREACHABLE(); break;
    }
  }
  return true;
}

bool Compiler::CompileLHSPrefix( const ast::Prefix& prefix ) {
  return CompilePrefixList(prefix.location,prefix.list,
      prefix.list.size() - 1);
}

template< int OP >
bool Compiler::CompileComponent( const vcl::util::CodeLocation& loc ,
    const ast::Prefix::Component& comp ) {
  switch(comp.tag) {
    case ast::Prefix::Component::DOT:
      {
        int id = CompileString(loc,comp.var);
        if(id <0) return false;
        m_procedure->code_buffer().Emit( loc ,
                                         BuildBytecode<CATEGORY_PROP,OP>(),
                                         id );
        break;
      }
    case ast::Prefix::Component::INDEX:
      {
        if(!Compile(*comp.expression)) return false;
        m_procedure->code_buffer().Emit( loc ,
                                         BuildBytecode<CATEGORY_INDEX,OP>());
        break;
      }
    case ast::Prefix::Component::ATTRIBUTE:
      {
        int id = CompileString(loc,comp.var);
        if(id <0) return false;
        m_procedure->code_buffer().Emit( loc ,
                                         BuildBytecode<CATEGORY_ATTR,OP>(),
                                         id );
        break;
      }
    default:
      VCL_UNREACHABLE();
      return false;
  }
  return true;
}

bool Compiler::Compile( const ast::Unset& unset ) {
  const ast::LeftHandSide& lhs = unset.lhs;
  if(lhs.type == ast::LeftHandSide::VARIABLE) {
    DCHECK(m_lex_scope);
    int id = m_lex_scope->Lookup(lhs.variable);
    if(id <0) {
      // Global varialbes
      id = CompileString(unset.location,lhs.variable);
      if(id <0)
        return false;
      __ gunset(unset.location,id);
    } else {
      __ unset(unset.location,id);
    }
  } else {
    if(!CompileLHSPrefix(*lhs.prefix))
      return false;
    const ast::Prefix::Component& comp = lhs.prefix->list.Last();
    if(!CompileComponent<OP_UNSET>(unset.location,comp))
      return false;
  }
  return true;
}

bool Compiler::Compile( const ast::Set& set ) {
  const ast::LeftHandSide& lhs = set.lhs;
  if(lhs.type == ast::LeftHandSide::VARIABLE) {
    DCHECK(m_lex_scope);
    int id = m_lex_scope->Lookup(lhs.variable);
    if(id >= 0) {
      // Compile the right hand side value
      if(!Compile(*set.rhs)) return false;

      switch(set.op) {
        case TK_ASSIGN:
          __ sstore(set.location,id);
          break;
        case TK_SELF_ADD:
          __ sadd(set.location,id);
          break;
        case TK_SELF_SUB:
          __ ssub(set.location,id);
          break;
        case TK_SELF_MUL:
          __ smul(set.location,id);
          break;
        case TK_SELF_DIV:
          __ sdiv(set.location,id);
          break;
        case TK_SELF_MOD:
          __ smod(set.location,id);
          break;
        default:
          VCL_UNREACHABLE();
          break;
      }
    } else {
      // Global variables
      id = CompileString(set.location,lhs.variable);
      if(id <0) return false;

      // Compile the right hand side value
      if(!Compile(*set.rhs)) return false;

      switch(set.op) {
        case TK_ASSIGN:
          __ gset(set.location,id);
          break;
        case TK_SELF_ADD:
          __ gsadd(set.location,id);
          break;
        case TK_SELF_SUB:
          __ gssub(set.location,id);
          break;
        case TK_SELF_MUL:
          __ gsmul(set.location,id);
          break;
        case TK_SELF_DIV:
          __ gsdiv(set.location,id);
          break;
        case TK_SELF_MOD:
          __ gsmod(set.location,id);
          break;
        default:
          VCL_UNREACHABLE();
          break;
      }
    }
  } else {
    // Right hand side
    if(!Compile(*set.rhs)) return false;

    // Left hand side
    if(!CompileLHSPrefix(*lhs.prefix)) return false;

    const ast::Prefix::Component& last = lhs.prefix->list.Last();
    switch(set.op) {
      case TK_ASSIGN:
        if(!CompileComponent<OP_SET>(set.location,last))
          return false;
        break;
      case TK_SELF_ADD:
        if(!CompileComponent<OP_SADD>(set.location,last))
          return false;
        break;
      case TK_SELF_SUB:
        if(!CompileComponent<OP_SSUB>(set.location,last))
          return false;
        break;
      case TK_SELF_MUL:
        if(!CompileComponent<OP_SMUL>(set.location,last))
          return false;
        break;
      case TK_SELF_DIV:
        if(!CompileComponent<OP_SDIV>(set.location,last))
          return false;
        break;
      case TK_SELF_MOD:
        if(!CompileComponent<OP_SMOD>(set.location,last))
          return false;
        break;
      default:
        return false;
    }
  }
  return true;
}

bool Compiler::CompileVariable( const vcl::util::CodeLocation& loc ,
    zone::ZoneString* var ) {
  int index = -1;
  if(m_lex_scope) index = m_lex_scope->Lookup(var);
  if(index <0) {
    int id = CompileString(loc,var);
    if(id <0) return false;
    __ gload(loc,id);
  } else {
    __ sload(loc,index);
  }
  return true;
}


bool Compiler::CompileFuncCall( const ast::FuncCall& fc ) {
  IntrinsicFunctionIndex intrinsic = INTRINSIC_FUNCTION_UNKNOWN;
  if(fc.name) {
    intrinsic = GetIntrinsicFunctionIndex(fc.name->data());
  }

  if(intrinsic == INTRINSIC_FUNCTION_UNKNOWN) {
    if(fc.name&&!CompileVariable(fc.location,fc.name))
      return false;
  }

  // Generate function call argument
  for( size_t i = 0 ; i < fc.argument.size() ; ++i ) {
    if(!Compile(*fc.argument[i])) return false;
  }

  // Generate code for intrinsic function via special bytecode
#define XX(A,B,C,D) case INTRINSIC_FUNCTION_##A : __ D(fc.location);

  switch(intrinsic) {
    INTRINSIC_FUNCTION_LIST(XX)
    default:
      __ call(fc.location,static_cast<uint32_t>(fc.argument.size()));
  }

#undef XX // XX

  if( fc.name ) {
    // It is a statment call, then we have to pop the return
    // value since there're no assignment happened here.
    __ spop(fc.location,1);
  }

  return true;
}

bool Compiler::Compile( const ast::Return& ret ) {
  if(!ret.value) {
    __ lnull(ret.location);
  } else {
    if(!Compile(*ret.value)) return false;
  }
  __ ret(ret.location);
  return true;
}

bool Compiler::Compile( const ast::Terminate& term ) {
  if(term.value) {
    if(!Compile(*term.value)) return false;
    __ term( term.location , static_cast<uint32_t>( ACT_EXTENSION ));
  } else {
    CHECK(term.action < ACT_EXTENSION);
    __ term( term.location , static_cast<uint32_t>( term.action ));
  }
  return true;
}

bool Compiler::Compile( const ast::If& node ) {
  std::vector<BytecodeBuffer::Label> exit_list;
  BytecodeBuffer::Label jump_label;

  // 1. Generate the first If
  {
    const ast::If::Branch& br = node.branch_list[0];
    // Generate the condition
    CHECK(br.condition);
    if(!Compile(*br.condition)) return false;
    jump_label = __ jf( node.location );
    if(!CompileChunk(*br.body)) return false;
    if( node.branch_list.size() > 1 )
      exit_list.push_back( __ jmp(node.location) );
  }

  // 2. Generate rest elif or else
  {
    for( size_t i = 1 ; i < node.branch_list.size() ; ++i ) {
      const ast::If::Branch& br = node.branch_list[i];
      // Patch the previous jump to jump here , before the previous
      // condition checking for this branch
      jump_label.Patch( m_procedure->code_buffer().position() );

      if(br.condition) {
        if(!Compile(*br.condition)) return false;
        jump_label = __ jf( node.location );
      } else {
        // else branch must be the last branch
        DCHECK( i == node.branch_list.size() - 1 );
      }

      // Compile the body of the if else branch
      if(!CompileChunk(*br.body)) return false;

      if(i < node.branch_list.size()-1)
        exit_list.push_back( __ jmp(node.location) );
    }

    if(!jump_label.IsPatched()) {
      jump_label.Patch( m_procedure->code_buffer().position() );
    }
  }

  // 3. Patch all exit jump branch
  for( size_t i = 0 ; i < exit_list.size() ; ++i ) {
    exit_list[i].Patch( m_procedure->code_buffer().position() );
  }
  return true;
}

bool Compiler::Compile( const ast::Break& node ) {
  VCL_UNUSED(node);
  LexicalScope* scope = m_lex_scope->GetLoopScope();
  // Pop all local variables along with all the nested lexical scope
  // until we hit the inner most loop direct scope
  __ spop( node.location , m_lex_scope->GetVariableSizeUntilLoopScope() );
  scope->AddBreak( &(m_procedure->code_buffer()) , node.location );
  return true;
}

bool Compiler::Compile( const ast::Continue& node ) {
  VCL_UNUSED(node);
  LexicalScope* scope = m_lex_scope->GetLoopScope();
  // Pop all local variables along with all the nested lexical scope
  // until we hit the inner most loop direct scope
  __ spop( node.location , m_lex_scope->GetVariableSizeUntilLoopScope() );
  scope->AddContinue( &(m_procedure->code_buffer()) , node.location );
  return true;
}

bool Compiler::Compile( const ast::For& node ) {
  size_t loop_hdr = 0;

  // 1. Evaluate the iterator code
  if(!Compile(*node.iterator)) return false;

  if(!m_lex_scope->DefineIterator(m_zone)) {
    ReportError(node.location,"Too much local variables!");
    return false;
  }

  // 2. Loop code generation
  {
    BytecodeBuffer::Label forprep_label; // Label for forprep instructions
                                         // this instruction will perform a jump
                                         // if very first test of iterator fails

    // Enter into a new lexical scope
    LexicalScope scope(this,m_procedure,node.body->location_end,true,true);

    // Generate a loop prepare instructions
    forprep_label = __ forprep(node.location);

    // Assign loop header position
    loop_hdr = m_procedure->code_buffer().position();

    // Define local variable and derefence from the iterator objects
    {
      int ret;
      if((ret=scope.DefineLocal( node.key )) == LexicalScope::LOCAL_TOO_MUCH) {
        ReportError(node.location,"Too much local variables!");
        return false;
      }
      DCHECK( ret >= 0);
      __ iterk(node.location);
    }

    if(node.val) {
      int ret = scope.DefineLocal( node.val );
      if(ret == LexicalScope::LOCAL_TOO_MUCH) {
        ReportError(node.location,"Too much local variables!");
        return false;
      } else if(ret == LexicalScope::LOCAL_DUPLICATE) {
        ReportError(node.location,"Variable %s has been defined before!",node.val->data());
        return false;
      }
      __ iterv(node.location);
    }

    // Now generate the body of for loop directly , pay attention, up to
    // now we don't need to generate code for testing the iterator because
    // we do a simple loop inverse
    if(!CompileChunkNoLexicalScope(*node.body)) return false;

    // 3. Manually generate pop instructions for all local variables defined
    // inside of the scope
    scope.PopAllVariablesInScope();

    // 4. Continue jumping point , jump to the position before doing test
    if(!BytecodeBuffer::CheckOperand(static_cast<uint32_t>(
            m_procedure->code_buffer().position()))) {
      ReportError(node.location,"The loop body is *too* large, the generated byte "
          "code has more than 2^24 bytes long!");
      return false;
    }
    scope.FixContinues( m_procedure->code_buffer().position() );

    // 5. Now generate code for testing the iterator on the stack
    if(!BytecodeBuffer::CheckOperand(static_cast<uint32_t>(loop_hdr))) {
      ReportError(node.location,"The loop body is *too* large, the generated byte "
          "code has more than 2^24 bytes long!");
      return false;
    }
    __ forend(node.body->location_end, static_cast<uint32_t>(loop_hdr));

    // 6. Generate code for breaks jumpping point
    if(!BytecodeBuffer::CheckOperand(static_cast<uint32_t>(
            m_procedure->code_buffer().position()))) {
      ReportError(node.location,"The loop body is *too* large, the generated byte "
          "code has more than 2^24 bytes long!");
      return false;
    }
    scope.FixBreak( m_procedure->code_buffer().position() );

    // We don't need to generate code to pop the iterator object since it is
    // internal local variable. All local variable will be automatically GCed
    // by the scopes rules. So we simply do not need to anything to that iterator
    // object at all.

    // 7. Patch the forprep jump position
    if(!BytecodeBuffer::CheckOperand(static_cast<uint32_t>(
            m_procedure->code_buffer().position()))) {
      ReportError(node.location,"The loop body is *too* large, the generated byte "
          "code has more than 2^24 bytes long!");
      return false;
    }
    forprep_label.Patch(static_cast<uint32_t>(m_procedure->code_buffer().position()));
  }

  return true;
}

bool Compiler::Compile( const ast::Import& import ) {
  int index = CompileString(import.location,import.module_name);
  if(index <0) return false;
  __ import(import.location,index);
  return true;
}

bool Compiler::Compile( const ast::Global& global ) {
  if(!Compile(*global.value)) return false;
  int id = CompileString( global.location , global.name );
  if(id <0) return false;
  __ gset(global.location,id);
  return true;
}

bool Compiler::Compile( const ast::Extension& ext ) {
  int id = CompileString(ext.location,ext.type_name);
  if(id <0) return false;
  __ lstr(ext.location,id);
  if(!Compile(*ext.initializer)) return false;

  {
    int name_id = CompileString(ext.location,ext.instance_name);
    if(name_id <0) return false;
    __ gset(ext.location,name_id);
  }
  return true;
}

bool Compiler::Compile( const ast::Sub& sub ) {
  return CompileChunkNoLexicalScope(*sub.body);
}

bool Compiler::CompileAnonymousSub( const ast::Sub& sub ) {
  Procedure* prev_procedure = m_procedure;
  uint32_t index;
  m_procedure = CompiledCodeBuilder(m_cc).CreateSubRoutine(sub,&index);
  {
    // Setup a function lexical scope since it is a new function/subroutine
    LexicalScope scope(this,m_procedure,sub.body->location_end,true,false,true);

    // Arguments
    for( size_t i = 0 ; i < sub.arg_list.size(); ++i ) {
      CHECK(scope.DefineLocal(sub.arg_list[i])>=0);
    }
    if(!Compile(sub)) return false;
  }
  __ lnull( sub.body->location_end );
  __ ret  ( sub.body->location_end );

  m_procedure = prev_procedure;

  __ lsub ( sub.body->location_end , index );

  return true;
}

bool Compiler::Compile( const CompilationUnit::SubList& sub_list ) {
  // Save the old procedure from previous compilation
  Procedure* prev_procedure = m_procedure;

  uint32_t index;
  const ast::Sub& sub = *sub_list[0];
  m_procedure = CompiledCodeBuilder(m_cc).CreateSubRoutine(sub,&index);

  {
    // Setup a function lexical scope
    LexicalScope scope(this,m_procedure,sub.body->location_end,true,false,true);

    // Generate argument list as local variables
    for( size_t i = 0 ; i < sub.arg_list.size() ; ++i ) {
      CHECK(scope.DefineLocal( sub.arg_list[i] ) >=0);
    }

    // Generate all sub list's body
    for( size_t i = 0 ; i < sub_list.size() ; ++i ) {
      const ast::Sub& s = *sub_list[i];
      DCHECK(*s.sub_name == *sub.sub_name);
      DCHECK(s.arg_list.size() == sub.arg_list.size());
      if(!Compile(s)) return false;
    }
  }

  // Finally , generate a fallback return instructions. Please be
  // aware which location we are using for this instruction. We
  // use the location that points to the last Sub in the SubList
  // which really means the location that points to the last statement
  // of last sub. Because we generate CompilationUnit as with the
  // same order when we meet each sub
  const vcl::util::CodeLocation& loc = sub_list.back()->body->location_end;
  __ lnull(loc);
  __ ret(loc);

  // Restore previous procedure , which is global scope procedure
  m_procedure = prev_procedure;

  __ gsub(loc,index);

  return true;
}

bool Compiler::Compile( const ast::ACL& acl ) {
  IPPattern* pattern = IPPattern::Compile(acl);
  if(pattern) {
    int acl_index = m_procedure->Add( m_cc->gc() , pattern );
    __ lacl(acl.location,acl_index);
    int var_index = CompileString(acl.location,acl.name);
    if(var_index <0) return false;
    __ gset(acl.location,var_index);
    return true;
  } else {
    ReportError(acl.location,"invalid acl, please check acl grammar!");
    return false;
  }
}

bool Compiler::Compile( const ast::AST& node ) {
  switch(node.type) {
    // Global
    case ast::AST_IMPORT:
      return Compile(static_cast<const ast::Import&>(node));
    case ast::AST_EXTENSION_LITERAL:
      return Compile(static_cast<const ast::ExtensionLiteral&>(node));
    case ast::AST_EXTENSION:
      return Compile(static_cast<const ast::Extension&>(node));
    case ast::AST_ACL:
      return Compile(static_cast<const ast::ACL&>(node));
    case ast::AST_GLOBAL:
      return Compile(static_cast<const ast::Global&>(node));
    // Stmt
    case ast::AST_TERMINATE:
      return Compile(static_cast<const ast::Terminate&>(node));
    case ast::AST_RETURN:
      return Compile(static_cast<const ast::Return&>(node));
    case ast::AST_SET:
      return Compile(static_cast<const ast::Set&>(node));
    case ast::AST_UNSET:
      return Compile(static_cast<const ast::Unset&>(node));
    case ast::AST_DECLARE:
      return Compile(static_cast<const ast::Declare&>(node));
    case ast::AST_IF:
      return Compile(static_cast<const ast::If&>(node));
    case ast::AST_STMT:
      return Compile(static_cast<const ast::Stmt&>(node));
    case ast::AST_LEXSCOPE:
      return Compile(static_cast<const ast::LexScope&>(node));
    case ast::AST_FOR:
      return Compile(static_cast<const ast::For&>(node));
    case ast::AST_BREAK:
      return Compile(static_cast<const ast::Break&>(node));
    case ast::AST_CONTINUE:
      return Compile(static_cast<const ast::Continue&>(node));
    // Expression
    case ast::AST_STRING_INTERPOLATION:
      return Compile(static_cast<const ast::StringInterpolation&>(node));
    case ast::AST_TERNARY:
      return Compile(static_cast<const ast::Ternary&>(node));
    case ast::AST_BINARY:
      return Compile(static_cast<const ast::Binary&>(node));
    case ast::AST_UNARY:
      return Compile(static_cast<const ast::Unary&>(node));
    case ast::AST_PREFIX:
      return Compile(static_cast<const ast::Prefix&>(node));
    case ast::AST_FUNCCALL:
      return CompileFuncCall(static_cast<const ast::FuncCall&>(node));
    case ast::AST_DICT:
      return Compile(static_cast<const ast::Dict&>(node));
    case ast::AST_STRING_CONCAT:
      return Compile(static_cast<const ast::StringConcat&>(node));
    case ast::AST_INTEGER:
      return Compile(static_cast<const ast::Integer&>(node));
    case ast::AST_REAL:
      return Compile(static_cast<const ast::Real&>(node));
    case ast::AST_BOOLEAN:
      return Compile(static_cast<const ast::Boolean&>(node));
    case ast::AST_NULL:
      return Compile(static_cast<const ast::Null&>(node));
    case ast::AST_STRING:
      return Compile(static_cast<const ast::String&>(node));
    case ast::AST_VARIABLE:
      return Compile(static_cast<const ast::Variable&>(node));
    case ast::AST_DURATION:
      return Compile(static_cast<const ast::Duration&>(node));
    case ast::AST_LIST:
      return Compile(static_cast<const ast::List&>(node));
    case ast::AST_SIZE:
      return Compile(static_cast<const ast::Size&>(node));
    case ast::AST_SUB:
      return CompileAnonymousSub(static_cast<const ast::Sub&>(node));
    default:
      VCL_UNREACHABLE()<<"node_type:"<<GetASTName( node.type );
      return false;
  }
}

bool Compiler::CompileChunkNoLexicalScope( const ast::Chunk& chunk ) {
  for( size_t i = 0 ; i < chunk.statement_list.size() ; ++i ) {
    if(!Compile(*chunk.statement_list[i])) return false;
  }
  return true;
}

bool Compiler::Compile( const CompilationUnit& cu ) {
  m_cur_source_index = cu.Index(0).source_index;
  // Generate very first debug information
  __ debug(m_cur_source_index);

  // Start the code generation
  for( size_t i = 0 ; i < cu.size() ; ++i ) {
    const CompilationUnit::Statement& stmt = cu.Index(i);
    if( m_cur_source_index != stmt.source_index ) {
      // Generate a debug information whenever our debug context changed
      __ debug(stmt.source_index);
      m_cur_source_index = stmt.source_index;
    }
    if(stmt.code.which() == CompilationUnit::STMT_AST) {
      if(!Compile(*boost::get<const ast::AST*>(stmt.code)))
        return false;
    } else {
      if(!Compile(*boost::get<CompilationUnit::SubListPtr>(stmt.code)))
        return false;
    }
  }

  __ lnull();
  __ ret();

  return true;
}

bool Compiler::DoCompile() {
  m_procedure = m_cc->entry();
  {
    LexicalScope scope(this,m_procedure);
    return Compile(m_unit);
  }
}

} // namespace

namespace vcl {
namespace vm  {

bool Compile( CompiledCode* cc, zone::Zone* zone , const CompilationUnit& cu ,
    std::string* error ) {
  ::Compiler compiler(cc,zone,cu,error);
  return compiler.DoCompile();
}

} // namespace vm
} // namespace vcl

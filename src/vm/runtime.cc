#include "runtime.h"

namespace vcl{
namespace vm {

int Runtime::EnterFunction( const Value& callable , size_t argument_size ,
    MethodStatus* status ) {
  // 1. Check whether we are calling a callable type
  if(!callable.IsFunction() && !callable.IsSubRoutine()) {
    status->set_fail("type %s cannot be called",callable.type_name());
    return FUNC_FAILED;
  }

  // 2. Check whether the too many calls
  if( m_frame.size() == static_cast<size_t>(m_max_calling_stack_size) ) {
    status->set_fail("too deep function call, we allow %d recursive function call",
        m_max_calling_stack_size);
    return FUNC_FAILED;
  }

  // 3. Check argument size for sub routine
  if(callable.IsSubRoutine()) {
    SubRoutine* sub_routine = callable.GetSubRoutine();
    if(sub_routine->argument_size() != argument_size) {
      status->set_fail("sub %s accept %zu argument , but got %zu",
          sub_routine->name().c_str(),
          sub_routine->argument_size(),
          argument_size);
      return FUNC_FAILED;
    }

    // Now generate the frame into the frame status and return caller
    // that we need to continue executing with *new* frame status
    m_frame.push_back( Frame( m_stack.size() - argument_size ,
          argument_size, callable ));

    return FUNC_SCRIPT;
  } else {
    m_frame.push_back( Frame( m_stack.size() - argument_size ,
          argument_size, callable));

    Function* function = callable.GetFunction();

    // Invoke the function
    *status = function->Invoke( context() , &m_v0 );

    // Pops the current function status when it is terminated or failed
    if(status->is_ok() || status->is_yield()) {
      ExitFunction(m_v0);
    }

    return FUNC_CPP;
  }
}

bool Runtime::ExitFunction( const Value& output ) {
  DCHECK(!m_frame.empty());
  size_t rsp_position = CurrentFrame()->base - 1;

  m_frame.pop_back(); // Pop the current calling frame

  if(!m_frame.empty()) {
    DCHECK( rsp_position < m_stack.size() );
    m_stack.resize( rsp_position );
    Push(output); // Push the return value onto the stack
    return true;
  } else {
    m_stack.clear();
  }

  return false;
}

#define verify(XX) \
  do { \
    switch((XX).status()) { \
      case MethodStatus::METHOD_OK: break; \
      case MethodStatus::METHOD_FAIL: goto fail; \
      case MethodStatus::METHOD_YIELD: \
      case MethodStatus::METHOD_TERMINATE: \
        result.set_fail("invalid method return status %s in operator function!", \
            (XX).status_name()); \
        goto fail; \
      case MethodStatus::METHOD_UNIMPLEMENTED: goto fail; \
    } \
  } while(false)

// A typical threaded interpreter in C/C++ code. To implement it I will
// have to use non-portable feature computed goto in GCC , and this is
// also supported by clang. To my best knowledge, MSVC doesn't support
// it but no one will port it to Windows.
MethodStatus Runtime::Main( Value* output , int64_t instr_count ) {
  DCHECK( instr_count > 0 );
  detail::VMGuard guard(this);

  uint32_t arg;
  MethodStatus result = MethodStatus::kOk;
  CompiledCode* cc = context()->compiled_code();
  SubRoutine* sub_routine = CurrentFrame()->sub_routine();
  Procedure* procedure = sub_routine->procedure();
  BytecodeBuffer::Iterator code =
    procedure->code_buffer().BeginAt(CurrentFrame()->pc);

  // Jump table for threading interpretation
  static const void* kLabels[] = {
#define __(A,B,C) &&LABEL_##A,
    VCL_BYTECODE_LIST(__)
    NULL
  };
#undef __ // __

#define dispatch(BYTECODE) \
  do { \
    --instr_count; \
    DCHECK( code != procedure->code_buffer().End() ); \
    /* Checking if user set us to be yieled , this typically happened when */ \
    /* user want to do some sort of preemptive scheduling for execution and signal */ \
    /* us that we should be yielded inside of signal handler */ \
    if(instr_count <0 || m_yield) { goto yield; } \
    const void* target = kLabels[ static_cast<size_t>(BYTECODE) ]; \
    goto *target; \
  } while(false)

#define next() \
  do { \
    ++code; \
    dispatch(*code); \
  } while(false)

  dispatch( *code );

#define vm_instr(BYTECODE) LABEL_##BYTECODE:

  vm_instr(BC_ADD) {
    verify((result = Top(1).Add( context() , Top(0) , &m_v0 )));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_SUB) {
    verify((result = Top(1).Sub( context() , Top(0) , &m_v0 )));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_MUL) {
    verify((result = Top(1).Mul( context() , Top(0) , &m_v0 )));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_DIV) {
    verify((result = Top(1).Div( context() , Top(0), &m_v0 )));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_MOD) {
    verify((result = Top(1).Mod( context() , Top(0), &m_v0 )));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_SADD) {
    verify((result = Back(code.arg()).SelfAdd( context(), Top(0))));
    Pop(1);
    next();
  }

  vm_instr(BC_SSUB) {
    verify((result = Back(code.arg()).SelfSub( context(), Top(0))));
    Pop(1);
    next();
  }

  vm_instr(BC_SMUL) {
    verify((result = Back(code.arg()).SelfMul( context(), Top(0))));
    Pop(1);
    next();
  }

  vm_instr(BC_SDIV) {
    verify((result = Back(code.arg()).SelfDiv( context(), Top(0))));
    Pop(1);
    next();
  }

  vm_instr(BC_SMOD) {
    verify((result = Back(code.arg()).SelfMod( context() , Top(0))));
    Pop(1);
    next();
  }

  vm_instr(BC_UNSET) {
    verify((result = Back(code.arg()).Unset(context())));
    next();
  }

  vm_instr(BC_LT) {
    bool v;
    verify((result = Top(1).Less(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_LE) {
    bool v;
    verify((result = Top(1).LessEqual(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_GT) {
    bool v;
    verify((result = Top(1).Greater(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_GE) {
    bool v;
    verify((result = Top(1).GreaterEqual(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_EQ) {
    bool v;
    verify((result = Top(1).Equal(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_NE) {
    bool v;
    verify((result = Top(1).NotEqual(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_MATCH) {
    bool v;
    verify((result = Top(1).Match(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_NOT_MATCH) {
    bool v;
    verify((result = Top(1).NotMatch(context(),Top(0),&v)));
    Pop(2);Push(Value(v));
    next();
  }

  vm_instr(BC_NEGATE) {
    Value& top = Top(0);
    if(top.IsInteger()) {
      Replace(Value(-top.GetInteger()));
    } else if(top.IsReal()) {
      Replace(Value(-top.GetReal()));
    } else {
      result.set_fail("type %s doesn't support unary operator \"-\".",top.type_name());
      goto fail;
    }
    next();
  }

  vm_instr(BC_TEST) {
    Value& top = Top(0);
    bool b;
    verify((result = top.ToBoolean(context(),&b)));
    Replace( Value(b) );
    next();
  }

  vm_instr(BC_FLIP) {
    Value& top = Top(0);
    bool b;
    verify((result = top.ToBoolean(context(),&b)));
    Replace( Value(!b) );
    next();
  }

  vm_instr(BC_LINT) {
    Push(Value(procedure->IndexInteger(code.arg())));
    next();
  }

  vm_instr(BC_LREAL) {
    Push(Value(procedure->IndexReal(code.arg())));
    next();
  }

  vm_instr(BC_LTRUE) {
    Push(Value(true));
    next();
  }

  vm_instr(BC_LFALSE) {
    Push(Value(false));
    next();
  }

  vm_instr(BC_LNULL) {
    Push(Value());
    next();
  }

  vm_instr(BC_LSTR) {
    Push(Value(procedure->IndexString(code.arg())));
    next();
  }

  vm_instr(BC_LSIZE) {
    Push(Value(procedure->IndexSize(code.arg())));
    next();
  }

  vm_instr(BC_LDURATION) {
    Push(Value(procedure->IndexDuration(code.arg())));
    next();
  }

  vm_instr(BC_LDICT) {
    int len = static_cast<int>(code.arg());
    Dict* dict = gc()->NewDict();
    m_v0.SetDict( dict );

    // Load all key value pair into the dictionary
    for( int i = len - 1 ; i >= 0 ; --i ) {
      Value& k = Top( static_cast<size_t>( 2*i+1 ) );
      Value& v = Top( static_cast<size_t>( 2*i ) );
      if(k.IsString()) {
        dict->InsertOrUpdate( *k.GetString() , v );
      } else {
        result.set_fail("dictionary's key must be string!");
        goto fail;
      }
    }
    Pop( len * 2 ); Push(m_v0);
    next();
  }

  vm_instr(BC_LLIST) {
    int len = static_cast<int>(code.arg());
    List* list = gc()->NewList(len);
    m_v0.SetList(list);

    for( int i = len - 1 ; i >= 0 ; --i ) {
      Value& v = Top( static_cast<size_t>(i) );
      list->Push(v);
    }
    Pop(len);Push(m_v0);
    next();
  }

  vm_instr(BC_LEXT) {
    arg = code.arg();
    Value& ext_name = Top(arg*2);
    DCHECK(ext_name.IsString());
    ExtensionFactory* factory = GetExtensionFactory(*ext_name.GetString());
    if(!factory) {
      result.set_fail("cannot find extension type %s!",
          ext_name.GetString()->data());
      goto fail;
    }

    {
      // Create a new extension from the ExtesnionFactory
      Extension* ext = factory->NewExtension(context());
      if(!ext) {
        result.set_fail("cannot new extension with type %s!",
            ext_name.GetString()->data());
        goto fail;
      }
      m_v0.SetExtension(ext);
      int len = static_cast<int>(arg);
      for( int i = len - 1 ; i >= 0 ; --i ) {
        Value& k = Top( static_cast<size_t>( 2*i + 1 ) );
        Value& v = Top( static_cast<size_t>( 2*i ) );
        DCHECK(k.IsString());
        verify((result = ext->SetProperty(context(),*k.GetString(),v)));
      }
    }
    Pop(arg*2+1); Push(m_v0);
    next();
  }

  vm_instr(BC_LACL) {
    Push(Value(procedure->IndexACL(code.arg())));
    next();
  }

  vm_instr(BC_SLOAD) {
    Push(Back(code.arg()));
    next();
  }

  vm_instr(BC_SSTORE) {
    Back(code.arg()) = Top(0);
    Pop(1);
    next();
  }

  vm_instr(BC_SPOP) {
    Pop(code.arg());
    next();
  }

  vm_instr(BC_JMP) {
    code = procedure->code_buffer().BeginAt( code.arg() );
    dispatch(*code);
  }

  vm_instr(BC_JF) {
    arg = code.arg();
    bool b;
    verify((result = Top(0).ToBoolean(context(),&b)));

    if(!b) {
      code = procedure->code_buffer().BeginAt(arg);
      Pop(1); dispatch(*code);
    } else {
      Pop(1); next();
    }
  }

  vm_instr(BC_JT) {
    arg = code.arg();
    bool b;
    verify((result = Top(0).ToBoolean(context(),&b)));

    if(b) {
      code = procedure->code_buffer().BeginAt(arg);
      Pop(1); dispatch(*code);
    } else {
      Pop(1); next();
    }
  }

  vm_instr(BC_BRT) {
    arg = code.arg();
    bool b;
    verify((result = Top(0).ToBoolean(context(),&b)));

    if(b) {
      code = procedure->code_buffer().BeginAt(arg);
      Replace(Value(true));
      dispatch(*code);
    } else {
      Pop(1); next();
    }
  }

  vm_instr(BC_BRF) {
    arg = code.arg();
    bool b;
    verify((result = Top(0).ToBoolean(context(),&b)));

    if(!b) {
      code = procedure->code_buffer().BeginAt(arg);
      Replace(Value(false));
      dispatch(*code);
    } else {
      Pop(1); next();
    }
  }

  vm_instr(BC_PGET) {
    arg = code.arg();
    String* key = procedure->IndexString( arg );
    verify((result = Top(0).GetProperty(context(),*key,&m_v0)));

    Replace(m_v0);
    next();
  }

  vm_instr(BC_PSET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    Value& v = Top(1);
    Value& obj=Top(0);
    verify((result = obj.SetProperty(context(),*key,v)));

    Pop(2);
    next();
  }

#define XX(BC,METHOD) \
  vm_instr(BC) { \
    arg = code.arg(); \
    String* key = procedure->IndexString(arg); \
    Value& v = Top(1); \
    Value& obj=Top(0); \
    verify((result = obj.GetProperty(context(),*key,&m_v0))); \
    verify((result = m_v0.METHOD(context(),v))); \
    if(!m_v0.IsObject()) { \
      verify((result = obj.SetProperty(context(),*key,m_v0))); \
    } \
    Pop(2); next(); \
  }

  XX(BC_PSADD,SelfAdd)
  XX(BC_PSSUB,SelfSub)
  XX(BC_PSMUL,SelfMul)
  XX(BC_PSDIV,SelfDiv)
  XX(BC_PSMOD,SelfMod)

#undef XX // XX

  vm_instr(BC_PUNSET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    Value& obj=Top(0);
    verify((result = obj.GetProperty(context(),*key,&m_v0)));
    verify((result = m_v0.Unset(context())));
    if(!m_v0.IsObject()) {
      verify((result = obj.SetProperty(context(),*key,m_v0)));
    }
    Pop(2); next();
  }

  vm_instr(BC_AGET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    verify((result = Top(0).GetAttribute(context(),*key,&m_v0)));
    Replace(m_v0); next();
  }

  vm_instr(BC_ASET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    Value& v = Top(1);
    Value& obj=Top(0);
    verify((result = obj.SetAttribute(context(),*key,v)));
    Pop(2); next();
  }

#define XX(BC,METHOD) \
  vm_instr(BC) { \
    arg = code.arg(); \
    String* key = procedure->IndexString(arg); \
    Value& v = Top(1); \
    Value& obj=Top(0); \
    verify((result = obj.GetAttribute(context(),*key,&m_v0))); \
    verify((result = m_v0.METHOD(context(),v))); \
    if(!m_v0.IsObject()) { \
      verify((result = obj.SetAttribute(context(),*key,m_v0))); \
    } \
    Pop(2); next(); \
  }

  XX(BC_ASADD,SelfAdd)
  XX(BC_ASSUB,SelfSub)
  XX(BC_ASMUL,SelfMul)
  XX(BC_ASDIV,SelfDiv)
  XX(BC_ASMOD,SelfMod)

#undef XX // XX

  vm_instr(BC_AUNSET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    Value& obj=Top(0);
    verify((result = obj.GetAttribute(context(),*key,&m_v0)));
    verify((result = m_v0.Unset(context())));
    if(!m_v0.IsObject()) {
      verify((result = obj.SetAttribute(context(),*key,m_v0)));
    }
    Pop(2); next();
  }

  vm_instr(BC_IGET) {
    Value& obj = Top(1);
    Value& key = Top(0);
    verify((result = obj.GetIndex(context(),key,&m_v0)));
    Pop(2);Push(m_v0);
    next();
  }

  vm_instr(BC_ISET) {
    Value& val = Top(2);
    Value& obj = Top(1);
    Value& key = Top(0);
    verify((result = obj.SetIndex(context(),key,val)));
    Pop(3); next();
  }

#define XX(BC,METHOD) \
  vm_instr(BC) { \
    Value& val = Top(2); \
    Value& obj = Top(1); \
    Value& key = Top(0); \
    verify((result = obj.GetIndex(context(),key,&m_v0))); \
    verify((result = m_v0.METHOD(context(),val))); \
    if(!m_v0.IsObject()) { \
      verify((result = obj.SetIndex(context(),key,m_v0))); \
    } \
    Pop(3); next(); \
  }

  XX(BC_ISADD,SelfAdd)
  XX(BC_ISSUB,SelfSub)
  XX(BC_ISMUL,SelfMul)
  XX(BC_ISDIV,SelfDiv)
  XX(BC_ISMOD,SelfMod)

#undef XX // XX

  vm_instr(BC_IUNSET) {
    Value& obj = Top(1);
    Value& key = Top(0);
    verify((result = obj.GetIndex(context(),key,&m_v0)));
    verify((result = m_v0.Unset(context())));
    if(!m_v0.IsObject()) {
      verify((result = obj.SetIndex(context(),key,m_v0)));
    }
    Pop(2); next();
  }

  vm_instr(BC_GLOAD) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    if(!GetGlobalVariable(*key,&m_v0)) {
      result.set_fail("global variable \"%s\" not found",key->data());
      goto fail;
    }
    Push(m_v0); next();
  }

  vm_instr(BC_GSET) {
    arg = code.arg();
    String* key = procedure->IndexString(arg);
    context()->AddOrUpdateGlobalVariable(*key,Top(0));
    Pop(1); next();
  }

#define XX(BC,METHOD) \
  vm_instr(BC) { \
    String* key = procedure->IndexString(code.arg()); \
    Value& val = Top(0); \
    if(!GetGlobalVariable(*key,&m_v0)) { \
      result.set_fail("global variable \"%s\" not found",key->data()); \
      goto fail; \
    } \
    m_v0.METHOD(context(),val); \
    context()->AddOrUpdateGlobalVariable(*key,m_v0); \
    Pop(1); next(); \
  }

  XX(BC_GSADD,SelfAdd)
  XX(BC_GSSUB,SelfSub)
  XX(BC_GSMUL,SelfMul)
  XX(BC_GSDIV,SelfDiv)
  XX(BC_GSMOD,SelfMod)

#undef XX // XX

  vm_instr(BC_GUNSET) {
    String* key = procedure->IndexString(code.arg());
    if(!GetGlobalVariable(*key,&m_v0)) {
      result.set_fail("global variable \"%s\" not found",key->data());
      goto fail;
    }
    verify((result=m_v0.Unset(context())));

    context()->AddOrUpdateGlobalVariable(*key,m_v0);
    next();
  }

  vm_instr(BC_DEBUG) {
    CurrentFrame()->source_index = code.arg();
    next();
  }

  vm_instr(BC_IMPORT) {
    String* key = procedure->IndexString(code.arg());
    Module* module = GetModule(*key);
    if(!module) {
      result.set_fail("module \"%s\" not found",key->data());
      goto fail;
    }
    context()->AddOrUpdateGlobalVariable(*key,Value(module));
    next();
  }

  vm_instr(BC_GSUB) {
    SubRoutine* sub_routine = InternalAllocator(gc()).NewSubRoutine(
        CompiledCodeBuilder(cc).IndexSubRoutine(code.arg()));
    m_v1.SetSubRoutine(sub_routine);

    String* sub_name = gc()->NewString(sub_routine->name());
    m_v0.SetString(sub_name);

    context()->AddOrUpdateGlobalVariable(*sub_name, Value(sub_routine));
    next();
  }

  vm_instr(BC_LSUB) {
    SubRoutine* sub_routine = InternalAllocator(gc()).NewSubRoutine(
        CompiledCodeBuilder(cc).IndexSubRoutine(code.arg()));
    m_v0.SetSubRoutine(sub_routine);
    Push(m_v0);
    next();
  }

  // Function call
  vm_instr(BC_CALL) {
    arg = code.arg();
    Value& callable = Top( arg );

    // Write the instruction position back to the Frame object
    CurrentFrame()->pc = code.next_available();

    // Start to interpreting the function call
    int status = EnterFunction( callable, arg , &result );
    switch(status) {
      case FUNC_FAILED: goto fail;
      case FUNC_CPP:
        switch( result.status() ) {
          case MethodStatus::METHOD_FAIL: goto fail;
          case MethodStatus::METHOD_YIELD: ++code; m_yield = true; goto yield;
          case MethodStatus::METHOD_OK: next();
          case MethodStatus::METHOD_UNIMPLEMENTED: goto fail;
          case MethodStatus::METHOD_TERMINATE: goto terminate;
          default: VCL_UNREACHABLE();
        }
      default:
        sub_routine = CurrentFrame()->sub_routine();
        procedure = sub_routine->procedure();
        code = procedure->code_buffer().BeginAt(CurrentFrame()->pc);
        dispatch(*code);
        break;
    }
  }

  vm_instr(BC_RET) {
    m_v0 = Top(0);
    if(!ExitFunction(m_v0)) {
      goto done;
    }
    sub_routine = CurrentFrame()->sub_routine();
    procedure = sub_routine->procedure();
    code = procedure->code_buffer().BeginAt(CurrentFrame()->pc);
    dispatch(*code);
  }

  vm_instr(BC_TERM) {
    arg = code.arg();
    if( static_cast<ActionType>(arg) == ACT_EXTENSION ) {
      m_v0 = Top(0);
    } else {
      m_v0.SetAction(gc()->NewAction(static_cast<ActionType>(arg)));
    }
    goto terminate;
  }

  vm_instr(BC_FORPREP) {
    Iterator* iterator;
    arg = code.arg();
    m_v0 = Top(0);
    if( m_v0.IsIterator() ) {
      iterator = m_v0.GetIterator();
    } else {
      verify((result = m_v0.NewIterator(context(),&iterator)));
      DCHECK(iterator);
    }
    m_v1.SetIterator(iterator);

    if(!iterator->Has(context())) {
      // Now the iterator doesn't have any value here, then just do a directly
      // jump to the point where we can directly skip the whole freaking body
      code = procedure->code_buffer().BeginAt(arg);
      dispatch(*code);
    } else {
      Replace(m_v1);
      next();
    }
  }

  vm_instr(BC_FOREND ) {
    arg = code.arg();
    m_v0 = Top(0);
    DCHECK( m_v0.IsIterator() );
    Iterator* iterator = m_v0.GetIterator();
    if(iterator->Next(context())) {
      code = procedure->code_buffer().BeginAt(arg);
      dispatch(*code);
    } else {
      next();
    }
  }

  vm_instr(BC_ITERK  ) {
    m_v0 = Top(0);
    DCHECK(m_v0.IsIterator());
    Iterator* iterator = m_v0.GetIterator();
    iterator->GetKey(context(),&m_v1);
    Push(m_v1);
    next();
  }

  vm_instr(BC_ITERV  ) {
    m_v0 = Top(1);
    DCHECK(m_v0.IsIterator());
    Iterator* iterator = m_v0.GetIterator();
    iterator->GetValue(context(),&m_v1);
    Push(m_v1);
    next();
  }

  vm_instr(BC_BRK    ) {
    arg = code.arg();
    code = procedure->code_buffer().BeginAt(arg);
    dispatch(*code);
  }

  vm_instr(BC_CONT   ) {
    arg = code.arg();
    code = procedure->code_buffer().BeginAt(arg);
    dispatch(*code);
  }

  vm_instr(BC_CSTR) {
    m_v0 = Top(0);
    String* pstring;
    if(!Value::ConvertToString(context(),m_v0,&pstring)) {
      result = MethodStatus::NewFail("type %s cannot be convert to string",
                                     m_v0.type_name());
      goto fail;
    }
    m_v1.SetString(pstring);
    Replace(m_v1);
    next();
  }

  vm_instr(BC_SCAT) {
    arg = code.arg();
    {
      // Forms a lexical scope to avoid memory leak inside of std::string due
      // to the *next* call which will jumps over the std::string's destructor
      std::string buf; buf.reserve(128);
      int i = static_cast<int>(arg) - 1;
      for( ; i >= 0 ; --i ) {
        m_v0 = Top(i);
        String* pstring = m_v0.GetString();
        buf.append( pstring->data() );
      }
      Handle<String> str(context()->gc()->NewString(buf),context()->gc());
      Pop(arg);
      Push(Value(str));
    }
    next();
  }

done:
  // When we reach here, it means we have succesfully finish all function
  // execution and m_frame.size() == 0.
  *output = m_v0;
  m_v0.SetNull();

  DCHECK(!m_yield);
  DCHECK(result);
  DCHECK(m_frame.empty());
  DCHECK(m_stack.empty());

  return result;

yield:
  DCHECK(!m_frame.empty());
  CurrentFrame()->pc = code.index();
  m_v0.SetNull();
  m_yield = true;
  return MethodStatus::kYield;

terminate:
  *output = m_v0;

  DCHECK(!m_yield);
  DCHECK(result);

  Reset();
  result.set_terminate();
  return result;

fail:
  m_v0.SetNull();

  DCHECK(result.is_fail() || result.is_unimplemented());
  CurrentFrame()->pc = code.index();
  result = ReportError(result.is_fail() ? result.fail() : result.unimplemented());
  Reset();
  return result;
}

#undef dispatch // dispatch
#undef next     // next
#undef vm_instr // vm_instr
#undef verify   // verify

void Runtime::UnwindStack( std::ostringstream* output ) const {
  int count = 0;

  for( std::vector<Frame>::const_reverse_iterator itr =
      m_frame.rbegin() ; itr != m_frame.rend() ; ++itr ) {
    const Frame& frame = *itr;
    (*output)<<count<<". ";

    if(frame.IsCppFunction()) {
      (*output)<<"<cpp>:"<<frame.function()->name();
      (*output)<<" argument-size:"<<frame.arg_size;
      (*output)<<" frame-base:"<<frame.base;
      (*output)<<" pc:"<<frame.pc<<'\n';
    } else {
      vcl::util::CodeLocation location =
        frame.sub_routine()->procedure()->code_buffer().code_location(frame.pc);

      boost::shared_ptr<SourceCodeInfo> source_code =
        m_context->compiled_code()->IndexSourceCodeInfo(frame.source_index);

      (*output)<<"<sub>:"<<frame.sub_routine()->protocol()<<
        " around line "<<location.line<<" and position "<<location.ccount
        <<" in file "<<source_code->file_path;

      (*output)<<" argument-size:"<<frame.arg_size;
      (*output)<<" frame-base:"<<frame.base;
      (*output)<<" pc:"<<frame.pc;

      if(count == 0) {
        (*output)<<"\naround source code:\n"
                 <<vcl::util::GetCodeSnippetHighlight(source_code->source_code,location)
                 <<'\n';
      } else {
        (*output)<<'\n';
      }
    }

    ++count;
  }
}

MethodStatus Runtime::ReportError( const std::string& error ) const {
  std::ostringstream formatter;
  UnwindStack(&formatter);
  return MethodStatus::NewFail("[runtime]: %s \n%s",error.c_str(),
      formatter.str().c_str());
}

void Runtime::Mark() {
  // Mark the frame stack
  for( size_t i = 0; i < m_frame.size(); ++i ) {
    m_frame[i].Mark();
  }

  // Mark the value stack
  for( size_t i = 0 ; i < m_stack.size(); ++i ) {
    m_stack[i].Mark();
  }

  // Lastly scratch register
  m_v0.Mark();
  m_v1.Mark();
}

// Resume
MethodStatus Runtime::Resume( Value* output ) {
  if(!is_yield()) {
    return MethodStatus::NewFail("Context is not interrupted ,but you try to resume it!");
  } else {
    m_yield = false;
    // We *MUST* have a none empty frame stack
    DCHECK( !m_frame.empty() );
    return Main(output);
  }
}

MethodStatus Runtime::BeginRun( SubRoutine* procedure ) {
  if(is_yield()) {
    return MethodStatus::NewFail("Context is interrupted,call Resume() first!");
  } else {
    Push(Value(procedure));
    return MethodStatus::kOk;
  }
}

void Runtime::AddArgument( const Value& value ) {
  Push(value);
}

MethodStatus Runtime::FinishRun( SubRoutine* routine , Value* value ) {
  DCHECK( Top(routine->argument_size()).IsSubRoutine() );
  DCHECK( Top(routine->argument_size()).GetSubRoutine() == routine );
  MethodStatus rstatus;
  int r = EnterFunction( Value(routine) , routine->argument_size() , &rstatus );
  if(r == FUNC_FAILED) {
    Reset();
    return rstatus;
  } else {
    DCHECK( r == FUNC_SCRIPT );
    return Main(value);
  }
}

} // namespace vm
} // namespace vcl

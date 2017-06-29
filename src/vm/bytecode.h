#ifndef BYTECODE_H_
#define BYTECODE_H_
#include <iostream>
#include <map>
#include <vcl/util.h>

namespace vcl {
namespace vm  {

// We will have a pretty simple bytecode implementation here, all the bytecode
// will have polymorphic semantic and all bytecode are not typpped. We don't
// care performance here too much.
//
// The bytecode is a very classical stack based bytecode and this is mostly
// because it is simpler for me to implement.
//
// Bytecode has variable length, ranging from 1 - 4 bytes. The 1 byte version
// simply means the bytecode *doesn't* have a operand and 4 bytes version means
// it has an operand.
//
// -------------------
// | OP | Operand    |
// -------------------
// | 1  |    3       |  --> Form A
// -------------------
// | 1  |               --> Form OP
// ------

#define VCL_BYTECODE_LIST(__) \
  /* Arithmatic bytecode */ \
  __(BC_ADD,0,add) \
  __(BC_SUB,0,sub) \
  __(BC_MUL,0,mul) \
  __(BC_DIV,0,div) \
  __(BC_MOD,0,mod) \
  __(BC_SADD,1,sadd) \
  __(BC_SSUB,1,ssub) \
  __(BC_SMUL,1,smul) \
  __(BC_SDIV,1,sdiv) \
  __(BC_SMOD,1,smod) \
  __(BC_UNSET,1,unset) \
  /* Comparison */ \
  __(BC_LT,0,lt) \
  __(BC_LE,0,le) \
  __(BC_GT,0,gt) \
  __(BC_GE,0,ge) \
  __(BC_EQ,0,eq) \
  __(BC_NE,0,ne) \
  __(BC_MATCH,0,match) \
  __(BC_NOT_MATCH,0,nmatch) \
  /* Unary */ \
  __(BC_NEGATE,0,negate) \
  __(BC_TEST,0,test) \
  __(BC_FLIP,0,flip) \
  /* Primitive Load */ \
  __(BC_LINT,1,lint) \
  __(BC_LREAL,1,lreal) \
  __(BC_LTRUE,0,ltrue) \
  __(BC_LFALSE,0,lfalse) \
  __(BC_LNULL,0,lnull) \
  __(BC_LSTR,1,lstr) \
  __(BC_LSIZE,1,lsize) \
  __(BC_LDURATION,1,lduration) \
  __(BC_LDICT,1,ldict) \
  __(BC_LLIST,1,llist) \
  __(BC_LEXT ,1,lext ) \
  __(BC_LACL ,1,lacl ) \
  /* Stack load store */ \
  __(BC_SLOAD,1,sload) \
  __(BC_SSTORE,1,sstore) \
  __(BC_SPOP,1,spop) \
  /* Jump */ \
  __(BC_JMP,1,jmp) \
  __(BC_JT ,1,jt) \
  __(BC_JF ,1,jf) \
  __(BC_BRT,1,brt) \
  __(BC_BRF,1,brf) \
  /* Property ( via DOT ) */ \
  __(BC_PGET,1,pget) \
  __(BC_PSET,1,pset) \
  __(BC_PSADD,1,psadd) \
  __(BC_PSSUB,1,pssub) \
  __(BC_PSMUL,1,psmul) \
  __(BC_PSDIV,1,psdiv) \
  __(BC_PSMOD,1,psmod) \
  __(BC_PUNSET,1,punset) \
  /* Attribute ( via COLON ) */ \
  __(BC_AGET,1,aget) \
  __(BC_ASET,1,aset) \
  __(BC_ASADD,1,asadd) \
  __(BC_ASSUB,1,assub) \
  __(BC_ASMUL,1,asmul) \
  __(BC_ASDIV,1,asdiv) \
  __(BC_ASMOD,1,asmod) \
  __(BC_AUNSET,1,aunset) \
  /* Index ( via [] ) */ \
  __(BC_IGET,0,iget) \
  __(BC_ISET,0,iset) \
  __(BC_ISADD,0,isadd) \
  __(BC_ISSUB,0,issub) \
  __(BC_ISMUL,0,ismul) \
  __(BC_ISDIV,0,isdiv) \
  __(BC_ISMOD,0,ismod) \
  __(BC_IUNSET,0,iunset) \
  /* Global variables */ \
  __(BC_GLOAD,1,gload) \
  __(BC_GSET ,1,gset) \
  __(BC_GSADD,1,gsadd) \
  __(BC_GSSUB,1,gssub) \
  __(BC_GSMUL,1,gsmul) \
  __(BC_GSDIV,1,gsdiv) \
  __(BC_GSMOD,1,gsmod) \
  __(BC_GUNSET,1,gunset) \
  /* Forloop */ \
  __(BC_FORPREP,1,forprep) \
  __(BC_FOREND ,1,forend ) \
  __(BC_ITERK  ,0,iterk  ) \
  __(BC_ITERV  ,0,iterv  ) \
  __(BC_BRK    ,1,brk    ) \
  __(BC_CONT   ,1,cont   ) \
  /* Debug */ \
  __(BC_DEBUG,1,debug) \
  /* Import */ \
  __(BC_IMPORT,1,import) \
  /* Call/Return */ \
  __(BC_CALL,1,call) \
  __(BC_TERM,1,term) \
  __(BC_RET,0,ret) \
  /* Sub */ \
  __(BC_GSUB,1,gsub) \
  __(BC_LSUB,1,lsub) \
  /* Speical intrinsic */ \
  __(BC_CSTR,0,cstr) \
  __(BC_CINT,0,cint) \
  __(BC_CREAL,0,creal) \
  __(BC_CBOOL,0,cbool) \
  __(BC_TYPE,0,type) \
  /* For string interpolation */ \
  __(BC_SCAT,1,scat)

enum Bytecode {
#define __(A,B,C) A,
  VCL_BYTECODE_LIST(__)
  SIZE_OF_BYTECODE
#undef __ // __
};

const char* BytecodeGetName( Bytecode );
bool BytecodeHasOperand( Bytecode );

static const uint32_t kMaxArg = (~0xff000000 - 1);

// A buffer object to hold the bytecode and also provides method to encode
// and decode the bytecode objects
class BytecodeBuffer {
 typedef std::map<size_t,vcl::util::CodeLocation> SourceInfoArray;
 public:
  static const size_t kInitSize = 1024;

  BytecodeBuffer( size_t init_size = kInitSize ):
    m_buffer(0),
    m_capacity(0),
    m_size(0)
  { Reserve( init_size , 0 ); }

  ~BytecodeBuffer() { free(m_buffer); }

  void Serialize( std::ostream& output ) const;

 public:
  static void Emit( uint32_t arg , uint8_t* pos )  {
    uint8_t cp1 = (arg & 0xff);
    uint8_t cp2 = (arg & 0xff00)>>8;
    uint8_t cp3 = (arg & 0xff0000)>>16;
    pos[0]= cp1;
    pos[1]= cp2;
    pos[2]= cp3;
  }

  inline void Emit( const vcl::util::CodeLocation& loc , Bytecode op ) {
    m_source_info[m_size] = loc;

    if((m_size + 1) > m_capacity)
      Reserve(m_capacity*2,1);
    m_buffer[m_size++] = static_cast<uint8_t>(op);
  }

  inline void Emit( const vcl::util::CodeLocation& loc , Bytecode op ,
      uint32_t oper ) {
    m_source_info[m_size] = loc;

    if((m_size + 4) > m_capacity)
      Reserve(m_capacity*2,4);
    m_buffer[m_size] = static_cast<uint8_t>(op);
    Emit(oper,m_buffer + m_size+1);
    m_size += 4;
  }

  static bool CheckOperand( uint32_t operand ) {
    return operand < kMaxArg;
  }

#define IMPL0(TYPE,BC) \
  void TYPE( const vcl::util::CodeLocation& loc ) { \
    Emit(loc,BC); \
  } \
  void TYPE() { \
    Emit(vcl::util::CodeLocation(),BC); \
  }

#define IMPL1(TYPE,BC) \
  void TYPE( const vcl::util::CodeLocation& loc , uint32_t operand ) { \
    DCHECK(CheckOperand(operand)); \
    Emit(loc,BC,operand); \
  } \
  void TYPE( uint32_t operand ) { \
    DCHECK(CheckOperand(operand)); \
    Emit(vcl::util::CodeLocation(),BC,operand); \
  }

#define __(A,B,C) IMPL##B(C,A)
  VCL_BYTECODE_LIST(__)
#undef __ // __

#undef IMPL0 // IMPL0
#undef IMPL1 // IMPL1

 public: // Jump and label handling

  struct Label {
    Label( uint32_t position , BytecodeBuffer* bb ):
      m_position(position),
      m_patched (false),
      m_bbuffer (bb)
    {}

    Label():
      m_position(0),
      m_patched (false),
      m_bbuffer (NULL)
    {}

    void Patch( int target ) {
      DCHECK(m_position);
      DCHECK(!m_patched);
      Emit(target,m_bbuffer->m_buffer + m_position);
      m_patched = true;
    }

    bool IsPatched() {
      return m_patched;
    }

   private:
    uint32_t m_position;
    bool m_patched;
    BytecodeBuffer* m_bbuffer;
  };

  Label jmp( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_JMP) + 1 , this );
  }

  Label jt ( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_JT) + 1 , this );
  }

  Label jf ( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_JF) + 1 , this );
  }

  Label brt( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_BRT) + 1 , this );
  }

  Label brf( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_BRF) + 1 , this );
  }

  Label brk( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_BRK) + 1 , this );
  }

  Label cont( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_CONT) + 1 , this );
  }

  Label forprep( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_FORPREP) + 1 , this );
  }

  Label forend( const vcl::util::CodeLocation& loc ) {
    return Label( Put(loc,BC_FOREND) + 1 , this );
  }

 public: // Accessors
  size_t size() const {
    return m_size;
  }

  int position() const {
    return static_cast<int>(m_size);
  }

  size_t capacity() const {
    return m_capacity;
  }

  const vcl::util::CodeLocation& code_location( size_t index ) const {
    SourceInfoArray::const_iterator itr = m_source_info.find(index);
    if(itr == m_source_info.end())
      return kNullCodeLocation;
    else
      return itr->second;
  }

  // Iterators for BytecodeBuffer object
  class Iterator {
   public:
    Iterator( const Iterator& itr ):
      m_bb(itr.m_bb),
      m_index(itr.m_index),
      m_arg(itr.m_arg)
    {}

    bool operator == ( const Iterator& itr ) const {
      DCHECK(m_bb == itr.m_bb);
      return m_index == itr.m_index;
    }

    bool operator != ( const Iterator& itr ) const {
      DCHECK(m_bb == itr.m_bb);
      return m_index != itr.m_index;
    }

    Iterator& operator = ( const Iterator& itr ) {
      if(this == &itr) return *this;
      else {
        m_bb = itr.m_bb;
        m_index = itr.m_index;
        m_arg = itr.m_arg;
        return *this;
      }
    }

    Iterator& operator ++ () {
      if(m_arg) DCHECK( BytecodeHasOperand(bytecode()) );
      m_index += 1 + m_arg;
      m_arg = 0;
      return *this;
    }

    Iterator operator ++ ( int idx )  {
      VCL_UNUSED(idx);
      Iterator itr(*this);
      m_index += 1 + m_arg;
      m_arg = 0;
      return itr;
    }

    Bytecode operator * () const {
      return bytecode();
    }

    uint32_t arg() const {
      DCHECK( BytecodeHasOperand(bytecode()) );
      uint8_t cp1 = m_bb->m_buffer[m_index+1];
      uint8_t cp2 = m_bb->m_buffer[m_index+2];
      uint8_t cp3 = m_bb->m_buffer[m_index+3];
      m_arg = 3;
      uint32_t ret = static_cast<uint32_t>( (cp3 << 16) |
                                            (cp2 <<  8) |
                                            (cp1) );
      DCHECK(ret != 0x00ffffff);
      return ret;
    }

    size_t index() const { return m_index; }
    size_t next_available() const { return m_index + 1 + m_arg; }
   private:
    Bytecode bytecode() const {
      return static_cast<Bytecode>(m_bb->m_buffer[m_index]);
    }

    Iterator( const BytecodeBuffer* bb ) :
      m_bb(bb),
      m_index(0),
      m_arg(0)
    {}

    Iterator( const BytecodeBuffer* bb , size_t idx ):
      m_bb(bb),
      m_index(idx),
      m_arg(0)
    {}

    const BytecodeBuffer* m_bb;
    size_t m_index;
    mutable size_t m_arg;

    friend class BytecodeBuffer;
  };

  typedef Iterator Iterator;

  Iterator Begin() const {
    return Iterator(this);
  }

  Iterator End() const {
    return Iterator(this,m_size);
  }

  Iterator BeginAt( size_t idx ) const {
    idx = idx > m_size ? m_size : idx;
    return Iterator(this,idx);
  }

 private:
  void Reserve( size_t cap , size_t guarantee ) {
    if(cap < guarantee) cap += guarantee;
    m_buffer = static_cast<uint8_t*>(realloc(m_buffer,cap));
    m_capacity = cap;
  }

  size_t Put  ( const vcl::util::CodeLocation& loc , Bytecode op ) {
    m_source_info[m_size] = loc;

    if(m_size + 4 > m_capacity)
      Reserve(2*m_capacity,4);
    m_buffer[m_size] = static_cast<uint8_t>(op);

    m_buffer[m_size+1] = 0xff;
    m_buffer[m_size+2] = 0xff;
    m_buffer[m_size+3] = 0xff;

    m_size += 4;
    return m_size - 4;
  }

  // Instruction buffer
  uint8_t* m_buffer;
  size_t m_capacity;
  size_t m_size;

  // We need an sparse array to store the CodeLocation debug information
  // and it should be addressable sololy with the code/instruction. Here
  // we similuate the sparse array just with an std::map object which is
  // kind of bad in terms of memory usage.
  SourceInfoArray m_source_info;

  // Null Code Location
  static vcl::util::CodeLocation kNullCodeLocation;

  friend class Iterator;
  friend class Label;
};


} // namespace vm
} // namespace vcl

#endif // BYTECODE_H_

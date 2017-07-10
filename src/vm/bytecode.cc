#include "bytecode.h"

namespace vcl {
namespace vm  {
namespace {

bool kOperandTable[] = {
#define __(A,B,C) static_cast<bool>((B)),
  VCL_BYTECODE_LIST(__)
  false
#undef __ // __
};

} // namespace

const char* BytecodeGetName( Bytecode bc ) {
  switch(bc) {
#define __(A,B,C) case A: return #C;
    VCL_BYTECODE_LIST(__)
    default: return NULL;
#undef __ // __
  }
}

bool BytecodeHasOperand( Bytecode bc ) {
  DCHECK(bc >=0 && bc < SIZE_OF_BYTECODE);
  return kOperandTable[static_cast<uint8_t>(bc)];
}

IntrinsicFunctionIndex GetIntrinsicFunctionIndex( const char* data ) {
#define __(A,B,C,D) if(strcmp(B,data) ==0) return INTRINSIC_FUNCTION_##A;
  INTRINSIC_FUNCTION_LIST(__)
#undef __ // __
  return INTRINSIC_FUNCTION_UNKNOWN;
}

void BytecodeBuffer::Serialize( std::ostream& output ) const {
  Iterator beg = Begin();
  Iterator end = End();
  size_t count = 1;

#define DISPLAY0() \
  do { \
    output<<beg.index()<<"  "<<count<<". "<<BytecodeGetName(*beg)<<'\n'; \
  } while(false)

#define DISPLAY1() \
  do { \
    output<<beg.index()<<"  "<<count<<". "<<BytecodeGetName(*beg); \
    output<<"  "<<beg.arg()<<'\n'; \
  } while(false)

  for( ; beg != end ; ++beg , ++count ) {
    switch(*beg) {
#define __(A,B,C) case A: DISPLAY##B(); break;
      VCL_BYTECODE_LIST(__)
      default: break;
#undef __ // __
    }
  }

#undef DISPLAY0 // DISPLAY0
#undef DISPLAY1 // DISPLAY1

}

vcl::util::CodeLocation BytecodeBuffer::kNullCodeLocation;

} // namespace vm
} // namespace vcl

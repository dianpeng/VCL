#include "ip-address.h"
#include "ast.h"

#include <boost/asio/ip/address.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/format.hpp>
#include <boost/system/error_code.hpp>
#include <boost/variant.hpp>

#include <cctype>
#include <climits>

#include <arpa/inet.h>

namespace vcl {
namespace vm {
namespace ast {
struct ACL;
}  // namespace ast

template <typename T, typename E>
class ByteArray {
  BOOST_STATIC_ASSERT(sizeof(T) == 4 || sizeof(T) == 16);
  BOOST_STATIC_ASSERT(sizeof(E) == 1 || sizeof(E) == 2);
  static const size_t kArrayLength = sizeof(T) / sizeof(E);

 public:
  ByteArray(const T& value) : m_array(), m_value(value) {
    BreakIntoBytes(value);
  }

  ByteArray() : m_array(), m_value() {}

  operator T() const { return m_value; }

  E* data() { return m_array; }

  const E* data() const { return m_array; }

 public:
  uint8_t Index(size_t index) const {
    CHECK(index < size());
    return m_array[index];
  }

  template <size_t INDEX>
  uint8_t Index() const {
    BOOST_STATIC_ASSERT(INDEX < kArrayLength);
    return m_array[INDEX];
  }

  size_t size() const { return kArrayLength; }

  uint8_t operator[](size_t index) const { return Index(index); }

  const T& value() const { return m_value; }

 private:
  void BreakIntoBytes(const T& value);
  E m_array[sizeof(T) / sizeof(E)];
  T m_value;
};

template <typename T, typename E>
void ByteArray<T, E>::BreakIntoBytes(const T& value) {
#ifndef BOOST_LITTLE_ENDIAN
#error "Only support little endian!"
#endif  // BOOST_LITTLE_ENDIAN

  if (sizeof(E) == 1) {
    uint32_t v;
    memcpy(&v, reinterpret_cast<const void*>(&value), sizeof(v));
    m_array[0] = static_cast<E>(v & 0x000000ff);
    m_array[1] = static_cast<E>((v & 0x0000ff00) >> 8);
    m_array[2] = static_cast<E>((v & 0x00ff0000) >> 16);
    m_array[3] = static_cast<E>((v & 0xff000000) >> 24);
  } else {
    uint64_t lower;
    uint64_t higher;
    memcpy(&higher, reinterpret_cast<const void*>(&value), sizeof(higher));
    memcpy(&lower, reinterpret_cast<const void*>(&lower), sizeof(lower));
    higher = boost::endian::big_to_native(higher);
    lower = boost::endian::big_to_native(lower);
    m_array[3] = static_cast<E>(higher & 0x000000000000ffff);
    m_array[2] = static_cast<E>(higher & 0x00000000ffff0000);
    m_array[1] = static_cast<E>(higher & 0x0000ffff00000000);
    m_array[0] = static_cast<E>(higher & 0xffff000000000000);
    m_array[7] = static_cast<E>(lower & 0x000000000000ffff);
    m_array[6] = static_cast<E>(lower & 0x00000000ffff0000);
    m_array[5] = static_cast<E>(lower & 0x0000ffff00000000);
    m_array[4] = static_cast<E>(lower & 0xffff000000000000);
  }
}  // namespace detail

///////////////////////////////////////////
// IP stuff
///////////////////////////////////////////
bool IsValidIPAddress(const char* string) {
  if (strcmp(string, "localhost") == 0)
    return true;
  else {
    boost::system::error_code ec;
    boost::asio::ip::address::from_string(string, ec);
    if (ec) {
      return false;
    }
  }
  return true;
}

namespace {

enum {
  TOKEN_DOT,
  TOKEN_COLON,
  TOKEN_COMPONENT,
  TOKEN_STAR,
  TOKEN_LSQR,
  TOKEN_RSQR,
  TOKEN_DASH,
  TOKEN_ERROR,
  TOKEN_EOF
};

template <typename E, size_t BOUND, size_t BASE>
class Tokenizer {
 public:
  BOOST_STATIC_ASSERT(BASE == 10 || BASE == 16);
  typedef E ElementType;
  Tokenizer(const char* string) : m_string(string), m_pos(0) {}

  struct Lexeme {
    int token;
    E value;
    Lexeme() : token(TOKEN_ERROR), value() {}
  };

  const Lexeme& Next();
  const Lexeme& lexeme() const { return m_lexeme; }
  const char* string() const { return m_string; }

 private:
  const Lexeme& ScanComponent();
  const char* m_string;
  size_t m_pos;
  Lexeme m_lexeme;
};

template <typename E, size_t BASE>
inline E ToDigit(char c) {
  if (BASE == 10) {
    return static_cast<E>(c - '0');
  } else {
    // TODO:: Use lookup table
    switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return c - '0';
      case 'a':
      case 'A':
        return 10;
      case 'b':
      case 'B':
        return 11;
      case 'c':
      case 'C':
        return 12;
      case 'd':
      case 'D':
        return 13;
      case 'e':
      case 'E':
        return 14;
      case 'f':
      case 'F':
        return 15;
      default:
        VCL_UNREACHABLE();
        return E();
    }
  }
}

template <size_t BASE>
inline bool IsDigit(char c) {
  if (BASE == 10) {
    return std::isdigit(c);
  } else {
    return std::isxdigit(c);
  }
}

#define YIELD(TK, LEN)     \
  do {                     \
    m_lexeme.token = (TK); \
    m_pos += (LEN);        \
    return m_lexeme;       \
  } while (false)

template <typename E, size_t BOUND, size_t BASE>
const typename Tokenizer<E, BOUND, BASE>::Lexeme&
Tokenizer<E, BOUND, BASE>::Next() {
  int c = m_string[m_pos];
  switch (c) {
    case '.':
      YIELD(TOKEN_DOT, 1);
    case ':':
      YIELD(TOKEN_COLON, 1);
    case '[':
      YIELD(TOKEN_LSQR, 1);
    case ']':
      YIELD(TOKEN_RSQR, 1);
    case '-':
      YIELD(TOKEN_DASH, 1);
    case '*':
      YIELD(TOKEN_STAR, 1);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return ScanComponent();
    case 'a':
    case 'A':
    case 'b':
    case 'B':
    case 'c':
    case 'C':
    case 'd':
    case 'D':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
      if (BASE != 16) YIELD(TOKEN_ERROR, 0);
      return ScanComponent();
    case 0:
      YIELD(TOKEN_EOF, 0);
    default:
      YIELD(TOKEN_ERROR, 0);
  }
}

#undef YIELD  // YIELD

template <typename E, size_t BOUND, size_t BASE>
const typename Tokenizer<E, BOUND, BASE>::Lexeme&
Tokenizer<E, BOUND, BASE>::ScanComponent() {
  size_t i;
  uint64_t component = ToDigit<E, BASE>(m_string[m_pos]);
  for (i = 1; i < BOUND; ++i) {
    int c = m_string[m_pos + i];
    if (IsDigit<BASE>(c)) {
      component *= (BASE);
      component += ToDigit<E, BASE>(c);
    } else
      break;
  }

  // Is the component too large ?
  if (component >= std::numeric_limits<E>::max()) {
    m_lexeme.token = TOKEN_ERROR;
    return m_lexeme;
  }

  m_lexeme.token = TOKEN_COMPONENT;
  m_lexeme.value = static_cast<E>(component);
  m_pos = i;
  return m_lexeme;
}

// Here we implement a slightly more complicated numeric ip address pattern
// string schema to extend the ACL in VCL. The grammar is as following :
// 1. For each IPV4 or IPV6 addresses, we allow certain element appear at
//    certain places. A typical IPV4 is represented as dotted string, each
//    component separated by dot can hold either a value , a class or a star.
//    Eg : 1.2.3.4
//         [10-100].2.3.4
//         *.2.3*
//         IPV6:
//         1ABC:E:*::3
//         1ABC:[3C-4F]:*::
// 2. Support for IPV4 condensed string representation. Per RFC, we are allowed
//    to have condensed string representation , which means in any ipv6 string
//    representation, user is allowed to have a \"::\" to represent consequtive
//    0 components.
// 3. All the IPV4 and IPV6 will be compiled into an object called
// IPMatchProgram,
//    which composes of a very simple virtual machine for the matcher to do
//    matching.

// We want to represent the OpCode with static array size which makes compiler
// easier to optimize our comparison phase by unrolling the loop. Therefore each
// instruction should be large enough to hold IPV4 or IPV6 patterns. The largest
// pattern we can have is a range pattern which composes 2 number and 1 opcode.
// The 2 number represents the *start* and the *end* of this range. In IPV6,
// each
// component should be represented as a 16 bytes number, so we need something
// at least larger than 32 bytes to represent everything. For simplicity, we
// choose to use a 8 bytes word to hold one instruction.
enum {
  MATCH = 0,
  ANY = 1,
  RANGE = 2,
  ZRANGE = 3,
  ANY_RANGE = 4,
  SIZE_OF_OPCODE
};

struct InstructionType {
  uint8_t opcode;
  uint16_t arg1;
  uint16_t arg2;

  InstructionType() : opcode(ANY), arg1(0), arg2(0) {}

  InstructionType(uint8_t op) : opcode(op), arg1(0), arg2(0) {}

  InstructionType(uint8_t op, uint16_t a1) : opcode(op), arg1(a1), arg2(0) {}

  InstructionType(uint8_t op, uint16_t a1, uint16_t a2)
      : opcode(op), arg1(a1), arg2(a2) {}
};

template <size_t ComponentSize>
struct IPMatchProgram {
  static const size_t kComponentSize = ComponentSize;
  typedef boost::array<InstructionType, ComponentSize> CodeBuffer;
  CodeBuffer code_buffer;
  bool negative;
  IPMatchProgram() : code_buffer(), negative(false) {}
};

// We use boost::variant to *devirtualize* a virtual call here.
enum { PROGRAM_IPV4, PROGRAM_IPV6 };

typedef ByteArray<in_addr, uint8_t> IPV4ByteArray;
typedef ByteArray<in6_addr, uint16_t> IPV6ByteArray;
typedef IPMatchProgram<4> IPV4Program;
typedef IPMatchProgram<8> IPV6Program;
typedef boost::variant<IPV4Program, IPV6Program> IPProgram;

template <typename Program, typename Address>
bool MatchProgram(const Program& program, const Address& address) {
  for (size_t i = 0; i < Program::kComponentSize; ++i) {
    const InstructionType& instr = program.code_buffer[i];
    switch (instr.opcode) {
      case MATCH:
        if (address[i] != instr.arg1) return false;
        break;
      case ANY:
        ++i;
        break;
      case RANGE:
        if (instr.arg1 <= address[i] && address[i] <= instr.arg2) return false;
        break;
      case ZRANGE: {
        for (size_t j = 0; j < instr.arg1; ++j) {
          if (address[j + i] != 0) return false;
        }
        i += instr.arg1;
      } break;
      case ANY_RANGE:
        i += instr.arg1 - 1;
        break;
      default:
        VCL_UNREACHABLE();
        return false;
    }
  }
  return true;
}

template <typename Program, typename Address>
bool DoMatch(const Program& prg, const Address& addr) {
  bool result = MatchProgram(prg, addr);
  if (prg.negative)
    return !result;
  else
    return result;
}

// Compiler for Compiling an unknown string into a IPMatchProgram object
struct Compiler {
 private:
  // Because we don't know what kind of ip address we are facing ,and also
  // ipv6 have condensed string representation. To support this kind of
  // variance,
  // we will compile our string representation into a intermediate
  // representation
  // and then later convert this intermediate representation into the
  // IPMatchProgram.
  enum { OP_V6_CONDENSED = SIZE_OF_OPCODE + 1 };

  enum { INVALID, IPV4, IPV6 };

  // This function will try to categorize this pattern string into IPV4 or IPV6.
  // This is faster than handle it in a one pass Compilation. I mean you really
  // cannot do it efficiently , so we may end up with some intermediate
  // representation,
  // but using this function to *categorize* will be fast. Since the following
  // loop will be unrolled and most case we will just hit in very few
  // instructions.
  int CheckPatternType(const char* pattern) {
    // We just try to guess whether this pattern is IPV4 or not.
    // The easist way would be check whether we see that dot or
    // colon.
    // For ipv4 : [100-100]. , after 9 characters we could see that dot;
    // for ipv6 : [1234-1234]: , after 11 characters we could see that colon;
    static const size_t kLongestLength = 12;

    for (size_t i = 0; i < kLongestLength; ++i) {
      if (!pattern[i]) return INVALID;
      if (pattern[i] == ':') return IPV6;
      if (pattern[i] == '.') return IPV4;
    }
    return INVALID;
  }

  template <typename T, bool IPV4, bool WILDCARD>
  bool CompileComponent(T* tokenizer, InstructionType* output) {
    switch (tokenizer->lexeme().token) {
      case TOKEN_COMPONENT: {
        *output = InstructionType(MATCH, tokenizer->lexeme().value);
        tokenizer->Next();
        return true;
      }
      case TOKEN_LSQR: {
        if (!WILDCARD) return false;
        typename T::ElementType start, end;
        if (tokenizer->Next().token != TOKEN_COMPONENT) return false;
        start = tokenizer->lexeme().value;
        if (tokenizer->Next().token != TOKEN_DASH ||
            tokenizer->Next().token != TOKEN_COMPONENT)
          return false;
        end = tokenizer->lexeme().value;
        if (tokenizer->Next().token != TOKEN_RSQR) return false;
        tokenizer->Next();

        *output = InstructionType(RANGE, start, end);
        return true;
      }
      case TOKEN_STAR: {
        if (!WILDCARD) return false;
        *output = InstructionType();
        tokenizer->Next();
        return true;
      }
      case TOKEN_COLON:
        if (IPV4)
          return false;
        else {  // Condensed string representation
          if (tokenizer->Next().token == TOKEN_COLON) {
            *output = InstructionType(OP_V6_CONDENSED);
            return true;
          } else {
            return false;
          }
        }
      default:
        return false;
    }
  }

  template <typename T, typename Program, bool IPV4, bool Wildcard,
            size_t ComponentSize>
  bool DoCompile(const char* pattern, Program* ptr) {
    T tokenizer(pattern);
    tokenizer.Next();
    const int delimiter = IPV4 ? TOKEN_DOT : TOKEN_COLON;
    bool has_condensed_string = false;
    size_t size = ComponentSize;
    int8_t zrange = -1;

    // 1. Now try the first 3 components
    for (size_t i = 0; i < ComponentSize - 1; ++i) {
      const size_t index = ComponentSize - i - 1;

      if (!CompileComponent<T, IPV4, Wildcard>(&tokenizer,
                                               &(ptr->code_buffer[index])))
        return false;

      if (!IPV4) {
        // A beautiful hack here :)
        // So we when we have a condensed string representation, we will
        // not be able to see the full 8 component in the wildcard string
        // representation. So this loop should break if we meet a EOF but
        // instead of a delimiter.
        if (ptr->code_buffer[i].opcode == OP_V6_CONDENSED) {
          // :: can only be appeared *once*
          if (has_condensed_string) return false;
          has_condensed_string = true;
          zrange = static_cast<int8_t>(index);
        }
      }

      // Consume the delimiter
      if (tokenizer.lexeme().token != delimiter) tokenizer.Next();

      if (has_condensed_string && tokenizer.lexeme().token == TK_EOF) {
        size = static_cast<uint8_t>(i + 1);
        break;
      }
    }

    // 2. Now try the last component
    if (!has_condensed_string) {
      if (!CompileComponent<T, IPV4, Wildcard>(
              &tokenizer, &(ptr->code_buffer[ComponentSize])))
        return false;
    }

    if (tokenizer.lexeme().token != TOKEN_EOF) return false;

    // Then we do some marshelling to make the code fits into 8 length
    // array to make matcher easier to work and more normalized
    if (!IPV4) {
      if (has_condensed_string) {
        const size_t zero_range_size = ComponentSize - size;
        const size_t end_index = size - (ComponentSize - zrange - 1) - 1;
        size_t start_index = (ComponentSize - size);
        for (size_t i = 0; start_index < end_index; ++start_index, ++i) {
          ptr->code_buffer[i] = ptr->code_buffer[start_index];
        }
        ptr->code_buffer[zrange].opcode = ZRANGE;
        ptr->code_buffer[zrange].arg1 = zero_range_size;
      }
    }
    return true;
  }

  template <bool Wildcard>
  bool CompileAsIPV4(const char* pattern, IPProgram* pt) {
    *pt = IPV4Program();
    IPV4Program& ptr = boost::get<IPV4Program>(*pt);
    typedef Tokenizer<int8_t, 3, 10> Tokenizer;
    return DoCompile<Tokenizer, IPV4Program, true, Wildcard, 4>(pattern, &ptr);
  }

  template <bool Wildcard>
  bool CompileAsIPV6(const char* pattern, IPProgram* pt) {
    *pt = IPV6Program();
    IPV6Program& ptr = boost::get<IPV6Program>(*pt);
    typedef Tokenizer<int16_t, 5, 16> Tokenizer;
    return DoCompile<Tokenizer, IPV6Program, false, Wildcard, 8>(pattern, &ptr);
  }

  bool EnsureMaskV4(IPV4Program* program, uint8_t mask) {
    size_t size;
    switch (mask) {
      case 8:
        size = 1;
        break;
      case 16:
        size = 2;
        break;
      case 24:
        size = 3;
        break;
      case 32:
        size = 4;
        break;
      default:
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
      if (program->code_buffer[i].opcode != MATCH ||
          program->code_buffer[i].arg1 != 0)
        return false;
    }
    program->code_buffer[0].opcode = ZRANGE;
    program->code_buffer[0].arg1 = size;
    return true;
  }

  bool EnsureMaskV6(IPV6Program* program, uint8_t mask) {
    size_t size;
    switch (mask) {
      case 16:
        size = 1;
        break;
      case 32:
        size = 2;
        break;
      case 48:
        size = 3;
        break;
      case 64:
        size = 4;
        break;
      case 80:
        size = 5;
        break;
      case 96:
        size = 6;
        break;
      case 112:
        size = 7;
        break;
      case 128:
        size = 8;
        break;
      default:
        return false;
    }
    // Try to check if we have condensed representation
    if (program->code_buffer[size - 1].opcode != ZRANGE ||
        program->code_buffer[size - 1].arg1 != (size)) {
      // Check if we have those zero literals here
      for (size_t i = 0; i < size; ++i) {
        if (program->code_buffer[i].opcode != MATCH ||
            program->code_buffer[i].arg1 == 0)
          return false;
      }
    }

    // Now just do a simple rewrite
    program->code_buffer[0].opcode = ANY_RANGE;
    program->code_buffer[0].arg1 = size;
    return true;
  }

 public:
  // Compiling wildcard entry. This entry is sololy used for compiling wildcard,
  // however in ACL we know that sometimes we don't have wildcard semantic when
  // user explicitly specify the netmask. If we have a netmask there we can just
  // treat the IP address as numeric address
  bool CompileWildcard(const char* pattern, IPProgram* pt) {
    int type = CheckPatternType(pattern);
    switch (type) {
      case INVALID:
        return false;  // Unknown pattern
      case IPV4:
        return CompileAsIPV4<true>(pattern, pt);
      case IPV6:
        return CompileAsIPV6<true>(pattern, pt);
      default:
        VCL_UNREACHABLE();
        return false;
    }
  }

  // Compiling a numeric entry. This is not wildcard entry
  bool CompileNumeric(const char* pattern, uint8_t mask, IPProgram* pt) {
    int type = CheckPatternType(pattern);
    switch (type) {
      case INVALID:
        return false;
      case IPV4:
        if (!CompileAsIPV4<false>(pattern, pt) ||
            !EnsureMaskV4(&boost::get<IPV4Program>(*pt), mask))
          return false;
        break;
      case IPV6:
        if (!CompileAsIPV6<false>(pattern, pt) ||
            !EnsureMaskV6(&boost::get<IPV6Program>(*pt), mask))
          return false;
        break;
      default:
        VCL_UNREACHABLE();
        return false;
    }
    return true;
  }
};

struct IPPatternImpl : public IPPattern {
  virtual bool Match(const char* ip_name) const {
    in_addr ipv4;
    in6_addr ipv6;
    int ret = inet_pton(AF_INET, ip_name, &ipv4);
    if (ret) {
      return Match(ipv4);
    } else {
      ret = inet_pton(AF_INET6, ip_name, &ipv6);
      if (ret) {
        return Match(ipv6);
      }
    }
    return false;
  }

  template <int PROGRAM_INDEX, typename E, typename B, typename PT>
  bool MatchImpl(E addr) const {
    for (size_t i = 0; i < program_list.size(); ++i) {
      const IPProgram& prg = program_list[i];
      if (prg.which() == PROGRAM_INDEX) {
        if (DoMatch(boost::get<PT>(prg), B(addr))) return true;
      }
    }
    return false;
  }

  virtual bool Match(const in_addr& addr) const {
    return MatchImpl<PROGRAM_IPV4, const in_addr&, IPV4ByteArray, IPV4Program>(
        addr);
  }

  virtual bool Match(const in6_addr& addr) const {
    return MatchImpl<PROGRAM_IPV6, const in6_addr&, IPV6ByteArray, IPV6Program>(
        addr);
  }

  virtual ~IPPatternImpl() {}

  bool Compile(const ast::ACL& acl_node);

  std::vector<IPProgram> program_list;
};

bool IPPatternImpl::Compile(const ast::ACL& acl_node) {
  Compiler compiler;
  for (size_t i = 0; i < acl_node.list.size(); ++i) {
    const ast::ACL::ACLItem& item = acl_node.list[i];
    program_list.push_back(IPProgram());
    IPProgram& program = program_list.back();

    if (item.mask != 0) {
      if (!compiler.CompileNumeric(item.name->data(), item.mask, &program))
        return false;
      boost::get<IPV4Program>(program).negative = item.negative;
    } else {
      if (!compiler.CompileWildcard(item.name->data(), &program)) return false;
      boost::get<IPV6Program>(program).negative = item.negative;
    }
  }

  return true;
}
}  // namespace

IPPattern* IPPattern::Compile(const ast::ACL& acl_node) {
  vcl::util::scoped_ptr<IPPatternImpl> impl(new IPPatternImpl());
  if (!impl->Compile(acl_node)) return NULL;
  return impl.release();
}

}  // namespace vm
}  // namespace vcl

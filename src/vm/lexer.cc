#include "lexer.h"
#include <boost/range.hpp>

namespace vcl {
namespace vm  {

using namespace vcl::util;

const char* GetTokenName( Token tk ) {
#define __(A,B) case A: return B;
  switch(tk) {
    VCL_TOKEN_LIST(__)
    default: return NULL;
  }
#undef __ // __
}

std::string Lexeme::symbol() const {
  CHECK(is_symbol);
  switch(token) {
    case TK_SUB_ROUTINE: return "sub";
    case TK_CALL: return "call";
    case TK_RETURN: return "return";
    case TK_NEW: return "new";
    case TK_SET: return "set";
    case TK_UNSET: return "unset";
    case TK_VCL: return "vcl";
    case TK_ACL: return "acl";
    case TK_IF:  return "if";
    case TK_DECLARE: return "declare";
    case TK_ELIF: return "elif";
    case TK_ELSIF:return "elsif";
    case TK_ELSEIF: return "elseif";
    case TK_ELSE: return "else";
    case TK_IMPORT: return "import";
    case TK_INCLUDE:return "include";
    case TK_GLOBAL: return "global";
    case TK_TRUE:   return "true";
    case TK_FALSE:  return "false";
    case TK_NULL:   return "null";
    case TK_VAR: return string();
    default: VCL_UNREACHABLE(); return std::string();
  }
}


const Lexeme& Lexer::Next() {
  if( m_state == LEXER_STATE_NORMAL ) {
    const Lexeme& lexeme = LexCode();
    if(lexeme.token == TK_INTERP_START ) {
      m_state = LEXER_STATE_STRING_INTERPOLATION;
    }
    return lexeme;
  } else {
    if( m_code_segment ) {
      return LexCode();
    } else {
      const Lexeme& lexeme = LexStringInterpolation();
      if( lexeme.token == TK_INTERP_END ) {
        m_state = LEXER_STATE_NORMAL;
      } else if( lexeme.token == TK_CODE_START ) {
        m_code_segment = true;
      }
      return lexeme;
    }
  }
}

const Lexeme& Lexer::LexStringInterpolation() {
  const char* src = m_source.c_str();
  if(src[m_pos] == '$') {
    int nc = src[m_pos+1];
    if(nc == '{') {
      m_pos += 2;
      m_lexeme.token = TK_CODE_START;
      m_lexeme.token_length = 2;
      m_lexeme.is_symbol = false;
      m_ccount += 2;
      return m_lexeme;
    }
  } else if(src[m_pos] == '\'') {
    m_pos += 1;
    m_lexeme.token = TK_INTERP_END;
    m_lexeme.token_length = 1;
    m_lexeme.is_symbol = false;
    m_ccount += 1;
    return m_lexeme;
  }

  int c;
  size_t start = m_pos;
  std::string buf; buf.reserve(64);
  for( ; (c = src[start]) ; ++start ) {
    if(c == '\\') {
      int nc = src[start+1];
      if(nc == '\\' || nc == '\'' || nc == '{') {
        buf.push_back(nc);
        ++start;
        continue;
      }
    } else if(c == '$') {
      int nc = src[start+1];
      if(nc == '{') break;
    } else if(c == '\'') {
      break;
    } else {
      buf.push_back(c);
    }
  }

  m_lexeme.value = buf;
  m_lexeme.token = TK_SEGMENT;
  m_lexeme.is_symbol = false;
  m_lexeme.token_length = (start - m_pos);
  m_ccount += (start-m_pos);
  m_pos = start;
  return m_lexeme;
}

#define YIELD(TK,LEN) \
  do { \
    m_lexeme.token = (TK); \
    m_lexeme.token_length = (LEN); \
    m_lexeme.is_symbol = false; \
    m_pos += (LEN); \
    m_ccount += (LEN); \
    return m_lexeme; \
  } while(false)

const Lexeme& Lexer::LexCode() {
  const char* src = m_source.c_str();
  do {
    int c = src[m_pos];
    switch(c) {
      case '+': return Lookahead(TK_SELF_ADD,'=',TK_ADD);
      case '-': return Lookahead(TK_SELF_SUB,'=',TK_SUB);
      case '*': return Lookahead(TK_SELF_MUL,'=',TK_MUL);
      case '/': {
        int nc = src[m_pos+1];
        if(nc == '/') {
          SkipLineComment();
          continue;
        } else if(nc == '*') {
          if(SkipMultilineComment())
            continue;
          else
            continue;
        } else
          return Lookahead(TK_SELF_DIV,'=',TK_DIV);
      }
      case '%': return Lookahead(TK_SELF_MOD,'=',TK_MOD);
      case '~': YIELD(TK_MATCH,1);
      case '!': return Lookahead(TK_NOT_MATCH,'~', TK_NE,'=', TK_NOT);
      case '#': SkipLineComment(); continue;
      case '=': return Lookahead(TK_EQ,'=',TK_ASSIGN);
      case '<': return Lookahead(TK_LE,'=',TK_LT);
      case '>': return Lookahead(TK_GE,'=',TK_GT);
      case '&': return Lookahead(TK_AND,'&',TK_ERROR);
      case '|': return Lookahead(TK_OR,'|',TK_ERROR);
      case ':': YIELD(TK_COLON,1);
      case ';': YIELD(TK_SEMICOLON,1);
      case ',': YIELD(TK_COMMA,1);
      case '.': YIELD(TK_DOT,1);
      case '(': YIELD(TK_LPAR,1); case ')': YIELD(TK_RPAR,1);
      case '[': YIELD(TK_LSQR,1); case ']': YIELD(TK_RSQR,1);
      case '{': {
        int nc = src[m_pos+1];
        if(nc == '"') {
          return LexMultilineStr();
        } else YIELD(TK_LBRA,1);
      }
      case '}': YIELD(TK_RBRA,1);
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return LexNumPrefix();
      case '"':
        return LexLineStr<'"'>();
      case ' ': case '\t': case '\r': case '\v':
        ++m_ccount; ++m_pos; break;
      case '\n':
        m_ccount = 1; ++m_line; ++m_pos; break;
      case 0: YIELD(TK_EOF,0);
      case '\'':
        if(m_state == LEXER_STATE_NORMAL)
          YIELD(TK_INTERP_START,1);
        else {
          LexerError("Nested string interpolation is not allowed");
          return m_lexeme;
        }
      default:
        return LexVarOrKeyword();
    }
  } while(true);
}

#undef YIELD

bool Lexer::TryTokenAsExtendedVar() {
  const char* src = m_source.c_str();
  int c = src[m_pos];
  int start = m_pos;
  if(IsSymbolInitChar(c)) {
    while( IsExtendedVarChar(src[start]) )
      ++start;
    m_lexeme.token_length = (start - m_pos);
    m_lexeme.token = TK_VAR;
    m_lexeme.is_symbol = true;
    m_lexeme.value = m_source.substr(m_pos , (start - m_pos));
    m_ccount += (start - m_pos);
    m_pos = start;
    return true;
  } else if( c == '"' || c == '\'' ) {
    if(c == '"') {
      if(LexLineStr<'"'>().token == TK_ERROR)
        return false;
      else
        return true;
    } else {
      if(LexLineStr<'\''>().token == TK_ERROR)
        return false;
      else
        return true;
    }
  } else {
    LexerError("Expect a variable name,you are allowed to put dash inside of "
               "the variable name here as well!");
    return false;
  }
}

const Lexeme& Lexer::LexerError() {
  m_lexeme.token = TK_ERROR;
  m_lexeme.is_symbol = false;
  m_lexeme.value = ReportError( m_source , location() ,
      "syntax","unknown character %c!",m_source[m_pos]);
  return m_lexeme;
}

const Lexeme& Lexer::LexerError( const char* format , ... ) {
  va_list vlist;
  va_start(vlist,format);
  m_lexeme.token = TK_ERROR;
  m_lexeme.is_symbol = false;
  m_lexeme.value = ReportErrorV( m_source , location() , "syntax",format,vlist);
  return m_lexeme;
}

void Lexer::SkipLineComment() {
  const char* src = m_source.c_str();
  ++m_pos;
  while( src[m_pos] && src[m_pos] != '\n' ) {
    ++m_pos;
    ++m_ccount;
  }
  if(src[m_pos] == '\n') {
    ++m_pos;
    m_ccount = 1;
    ++m_line;
  }
}

bool Lexer::SkipMultilineComment() {
  const char* src = m_source.c_str();
  m_pos += 2; // Skip /*
  while( src[m_pos] ) {
    if(src[m_pos] == '*' && src[m_pos+1] == '/' )
      break;
    else if(src[m_pos] == '\n') {
      m_line++;
      m_ccount = 1;
    }
    ++m_pos;
    ++m_ccount;
  }
  if(!src[m_pos]) {
    LexerError("multiline comments are not closed properly by \"*/\"");
    return false;
  } else {
    m_pos += 2; // Skip */
  }
  return true;
}

// We have lots of number prefix value they are basically following :
// 1. integer
// 2. real
// 3. size
// 4. duration/time
const Lexeme& Lexer::LexNumPrefix() {
  int start;
  const char* src = m_source.c_str();

  start = m_pos + 1;

  // 1. Try to consume as much digits as possible
  while( std::isdigit(src[start]) )
    ++start;

  // 2. Checking possible postfix
  switch(src[start]) {
    case '.': return LexReal(start);
    case 's': return LexDuration<DURATION_SEC>(start+1,start);
    case 'm': {
      int nc = src[start+1];
      if(nc == 's')
        return LexDuration<DURATION_MSEC>(start+2,start);
      else if(nc == 'b')
        return LexSize<SIZE_MB>(start+2,start);
      else if(nc == 'i') {
        int nnc = src[start+2];
        if(nnc == 'n')
          return LexDuration<DURATION_MIN>(start+3,start);
      }
    }
    break;
    case 'g':
      if(src[start+1] == 'b') return LexSize<SIZE_GB>(start+2,start);
      break;
    case 'k':
      if(src[start+1] == 'b') return LexSize<SIZE_KB>(start+2,start);
      break;
    case 'M':
      if(src[start+1] == 'B') return LexSize<SIZE_MB>(start+2,start);
      break;
    case 'K':
      if(src[start+1] == 'B') return LexSize<SIZE_KB>(start+2,start);
      break;
    case 'G':
      if(src[start+1] == 'B') return LexSize<SIZE_GB>(start+2,start);
    case 'b': case 'B':
      return LexSize<SIZE_B>(start+1,start);
    case 'h':
      return LexDuration<DURATION_HOUR>(start+1,start);
    default:
      break;
  }

  // 3. Reach here we know it is a integer, so convert it to integer
  try {
    m_lexeme.value = boost::lexical_cast<int32_t>(
        m_source.substr(m_pos,start-m_pos));
  } catch( ... ) {
    return LexerError("cannot convert number written as %s to int32_t!",
        m_source.substr(m_pos,start-m_pos).c_str());
  }
  m_lexeme.token = TK_INTEGER;
  m_lexeme.is_symbol = false;
  m_lexeme.token_length = (start - m_pos);
  m_ccount += (start - m_pos);
  m_pos = start;
  return m_lexeme;
}

const Lexeme& Lexer::LexReal( int pos ) {
  int start = pos + 1; // Skip the dot
  const char* src = m_source.c_str();
  while( std::isdigit(src[start]) )
    ++start;
  if( start == pos + 1 ) {
    return LexerError("real number is in ill-formatted, expect more digits "
        "after the dot");
  }

  // Convert it to double
  try {
    m_lexeme.value = boost::lexical_cast<double>(
        m_source.substr(m_pos,start-m_pos));
  } catch( ... ) {
    return LexerError("cannot convert number writte as %s to double!",
        m_source.substr(m_pos,start-m_pos).c_str());
  }

  m_lexeme.token_length = (start - m_pos);
  m_lexeme.token = TK_REAL;
  m_lexeme.is_symbol = false;
  m_ccount += (start - m_pos);
  m_pos = start;
  return m_lexeme;
}

template< int DELIMITER >
const Lexeme& Lexer::LexLineStr() {
  // Line based string in VCL doesn't support escape any characters. But our
  // version do the escacpe only for double quotes. Except it nothing will be
  // escaped. This includes line breaks and other stuff. Which just tries to
  // be compatible with VCL's syntax
  int start = m_pos + 1; // Skip the quote
  const char* src = m_source.c_str();
  std::string buf;
  buf.reserve(32); // Bare with my guess :)

  do {
    int c = src[start];
    if(c == '\\') {
      // Check the only escape allowed
      int nc = src[start+1];
      if(nc == DELIMITER ) {
        c = DELIMITER; ++start;
      }
    } else if(c == DELIMITER) {
      ++start; break;
    } else if(!c) {
      return LexerError("string is not closed properly with: \"");
    } else if(c == '\n') {
      return LexerError("single line string has line break character ,"
                        "please use multiple line character!");
    }
    buf.push_back(c);
    ++start;
  } while(true);

  m_lexeme.value = buf;
  m_lexeme.token = TK_STRING;
  m_lexeme.token_length = (start - m_pos);
  m_lexeme.is_symbol= false;
  m_ccount += (start - m_pos);
  m_pos = start;
  return m_lexeme;
}

const Lexeme& Lexer::LexMultilineStr() {
  int start = m_pos + 2;
  const char* src = m_source.c_str();
  std::string buf;
  buf.reserve(64);

  ++m_ccount;

  do {
    int c = src[start];
    if(c == '"') {
      int nc = src[start+1];
      if(nc == '}') {
        m_ccount +=2 ; start+=2; break;
      }
    } else if(!c) {
      return LexerError("multiple line string is not closed with: \"}");
    } else if(c == '\n') {
      ++m_line;
      m_ccount = 1;
    }
    buf.push_back(c);
    ++start;
    ++m_ccount;
  } while(true);

  m_lexeme.value = buf;
  m_lexeme.token = TK_STRING;
  m_lexeme.is_symbol = false;
  m_pos = start;
  m_lexeme.token_length = (start - m_pos);
  return m_lexeme;
}

namespace {

template< size_t N >
int Compare( const char* LHS , const char (&RHS)[N] ) {
  // It is resilient to early null terminator since if LHS
  // has null terminator, it won't match any character in
  // RHS. So it will still return false without consuming
  // more characters.
  for( int i = 0 ; i < static_cast<int>(N - 1); ++i ) {
    if(LHS[i] != RHS[i]) return i;
  }
  return N - 1;
}

template< size_t N >
int KeywordCompare( const char* LHS , const char (&RHS)[N] ) {
  int ret;
  const size_t string_length = N - 1;
  if(((ret=Compare(LHS,RHS)) == string_length) &&
      !Lexer::IsSymbolChar(LHS[string_length]))
    return N;
  else
    return ret;
}

} // namespace

// Keyword lexing. VCL has many keyword , but in its original lexer it doesn't
// lexing keyword at all but deferred to parser to figure out what they want.
// We can do that but I don't like that style simply because the parser has too
// much dependency instead of depending on the grammar. In the Lexeme, we set up
// a flag called is_symbol. This flag will tell the parser that whether they want
// to treat the lexeme as a symbol name even the lexer recognize it as a token.

#define CHECK_KEYWORD(MATCH,L,R) \
  ((MATCH=KeywordCompare((L),(R))) == (static_cast<int>(boost::size((R)))))

#define YIELD(TK,LEN) \
  do { \
    m_lexeme.is_symbol = true; \
    m_lexeme.token = (TK); \
    m_lexeme.token_length = (LEN); \
    m_pos+= (LEN); \
    m_ccount += (LEN); \
    return m_lexeme; \
  } while(false)

const Lexeme& Lexer::LexVarOrKeyword() {
  const char* src = m_source.c_str();
  int c = src[m_pos];
  int len;
  switch(c) {
    case 'a':
      if(CHECK_KEYWORD(len,src+m_pos+1,"cl"))
        YIELD(TK_ACL,3);
      return LexVar(len);
    case 'b':
      if(CHECK_KEYWORD(len,src+m_pos+1,"reak"))
        YIELD(TK_BREAK,5);
      return LexVar(len);
    case 'c':
      if(CHECK_KEYWORD(len,src+m_pos+1,"all"))
        YIELD(TK_CALL,4);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"ontinue"))
        YIELD(TK_CONTINUE,8);
      return LexVar(len);
    case 'd':
      if(CHECK_KEYWORD(len,src+m_pos+1,"eclare"))
        YIELD(TK_DECLARE,7);
      return LexVar(len);
    case 'e':
      if(CHECK_KEYWORD(len,src+m_pos+1,"lse"))
        YIELD(TK_ELSE,4);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"lif"))
        YIELD(TK_ELIF,4);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"lsif"))
        YIELD(TK_ELSIF,5);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"lseif"))
        YIELD(TK_ELSEIF,6);
      return LexVar(len);
    case 'f':
      if(CHECK_KEYWORD(len,src+m_pos+1,"alse"))
        YIELD(TK_FALSE,5);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"or"))
        YIELD(TK_FOR,3);
      return LexVar(len);
    case 'g':
      if(CHECK_KEYWORD(len,src+m_pos+1,"lobal"))
        YIELD(TK_GLOBAL,6);
      return LexVar(len);
    case 'i':
      if(CHECK_KEYWORD(len,src+m_pos+1,"nclude"))
        YIELD(TK_INCLUDE,7);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"f"))
        YIELD(TK_IF,2);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"mport"))
        YIELD(TK_IMPORT,6);
      return LexVar(len);
    case 'n':
      if(CHECK_KEYWORD(len,src+m_pos+1,"ew"))
        YIELD(TK_NEW,3);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"ull"))
        YIELD(TK_NULL,4);
      return LexVar(len);
    case 'r':
      if(CHECK_KEYWORD(len,src+m_pos+1,"eturn"))
        YIELD(TK_RETURN,6);
      return LexVar(len);
    case 's':
      if(CHECK_KEYWORD(len,src+m_pos+1,"et"))
        YIELD(TK_SET,3);
      else if(CHECK_KEYWORD(len,src+m_pos+1,"ub"))
        YIELD(TK_SUB_ROUTINE,3);
      return LexVar(len);
    case 't':
      if(CHECK_KEYWORD(len,src+m_pos+1,"rue"))
        YIELD(TK_TRUE,4);
      return LexVar(len);
    case 'u':
      if(CHECK_KEYWORD(len,src+m_pos+1,"nset"))
        YIELD(TK_UNSET,5);
      return LexVar(len);
    case 'v':
      if(CHECK_KEYWORD(len,src+m_pos+1,"cl"))
        YIELD(TK_VCL,3);
      return LexVar(len);
    default:
      if(Lexer::IsSymbolInitChar(c))
        return LexVar(1);
      else
        return LexerError();
  }
}

#undef YIELD
#undef CHECK_KEYWORD

const Lexeme& Lexer::LexVar( int start ) {
  const char* src = m_source.c_str();
  int pos = m_pos + start;

  while( IsSymbolChar(src[pos]) )
    ++pos;

  m_lexeme.value = m_source.substr( m_pos , (pos - m_pos) );
  m_lexeme.is_symbol = true;
  m_lexeme.token = TK_VAR;
  m_lexeme.token_length = (pos - m_pos);
  m_ccount += (pos - m_pos);
  m_pos = pos;
  return m_lexeme;
}

} // namespace vm
} // namespace vcl

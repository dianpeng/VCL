#ifndef LEXER_H_
#define LEXER_H_
#include <string>
#include <cctype>
#include <inttypes.h>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>

#include <vcl/util.h>

namespace vcl {
namespace vm  {

// Original Varnshi VCC's lexer is really interesting. It doesn't have concept
// of keywords, but treat all the symbol as IDs and then decide what it is when
// it does parsing/transpiling. We don't do that since that makes our code not
// easy to maintain. A separate lexer will help us extend our grammar easier.
// But by this we slightly change the semnatic of the grammar , the keyword cannot
// be used as symbol now. However I don't think any one will use keyword as variable
// name.
//
// Order matters !!!!
#define VCL_TOKEN_LIST(__) \
  /* Arithmatic operators */ \
  __(TK_ADD,"+") \
  __(TK_SUB,"-") \
  __(TK_MUL,"*") \
  __(TK_DIV,"/") \
  __(TK_MOD,"%") \
  /* Comparison */ \
  __(TK_MATCH,"~") \
  __(TK_NOT_MATCH,"!~") \
  __(TK_EQ,"==") \
  __(TK_NE,"!=") \
  __(TK_LT,"<") \
  __(TK_LE,"<=") \
  __(TK_GT,">") \
  __(TK_GE,">=") \
  /* Logic */ \
  __(TK_AND,"&&") \
  __(TK_OR,"||") \
  __(TK_NOT,"!") \
  /* Assign */ \
  __(TK_SELF_DIV,"/=") \
  __(TK_SELF_MUL,"*=") \
  __(TK_SELF_SUB,"-=") \
  __(TK_SELF_ADD,"+=") \
  __(TK_SELF_MOD,"%=") \
  __(TK_ASSIGN,"=") \
  /* Punction */ \
  __(TK_SEMICOLON,";") \
  __(TK_COLON,":") \
  __(TK_COMMA,",") \
  __(TK_DOT,".") \
  __(TK_LPAR,"(") \
  __(TK_RPAR,")") \
  __(TK_LBRA,"{") \
  __(TK_RBRA,"{") \
  __(TK_LSQR,"[") \
  __(TK_RSQR,"]") \
  /* Keywords */ \
  __(TK_SUB_ROUTINE,"sub") \
  __(TK_CALL,"call") \
  __(TK_RETURN,"return") \
  __(TK_NEW,"new") \
  __(TK_SET,"set") \
  __(TK_UNSET,"unset") \
  __(TK_VCL,"vcl") \
  __(TK_ACL,"acl") \
  __(TK_IF,"if") \
  /* Fastly syntax */ \
  __(TK_DECLARE,"declare") \
  /* VCL has 3 types of else if , elseif elif and elsif */ \
  __(TK_ELIF,"elif") \
  __(TK_ELSIF,"elsif") \
  __(TK_ELSEIF,"elseif") \
  __(TK_ELSE,"else") \
  __(TK_FOR,"for") \
  __(TK_BREAK,"break") \
  __(TK_CONTINUE,"continue") \
  __(TK_IMPORT,"import") \
  __(TK_INCLUDE,"include") \
  __(TK_GLOBAL,"global") \
  /* VCL literals */ \
  __(TK_STRING,"<string>") \
  __(TK_INTEGER,"<integer>") \
  __(TK_REAL,"<real>") \
  __(TK_TRUE,"true") \
  __(TK_FALSE,"false") \
  __(TK_NULL ,"null") \
  __(TK_DURATION,"duration") \
  __(TK_SIZE,"size") \
  __(TK_VAR  ,"<var>") \
  /* String Interpolation */ \
  __(TK_SEGMENT,"<segment>") \
  __(TK_INTERP_START,"<interp-start>") \
  __(TK_INTERP_END  ,"<interp-end>") \
  __(TK_CODE_START  ,"<code-start>") \
  /* We do not have a TK_CODE_END sybmol is because the TK_CODE_END symbol */ \
  /* is aliased from TK_LBRA , the parser needs to disambigious this symbol */ \
  /* MISCs */ \
  __(TK_ERROR,"<error>") \
  __(TK_EOF,"<eof>")


enum Token {
#define __(A,B) A,
  VCL_TOKEN_LIST(__)
  SIZE_OF_VCL_TOKENS
#undef __ // __
};

const char* GetTokenName( Token tk );

inline bool TokenIsBinaryOperator( Token tk ) {
  return tk >= 0 && tk <= TK_OR;
}

inline bool TokenIsLogicOperator( Token tk ) {
  return tk == TK_AND || tk == TK_OR;
}

struct Lexeme {
  Token token;
  boost::variant<
    int32_t,
    double,
    std::string,
    bool,
    vcl::util::Duration,
    vcl::util::Size > value;
  size_t token_length;

  // A generalized way to get symbol name regardless of what token
  // underlying it is
  std::string symbol() const;

  int32_t integer() const {
    return boost::get<int32_t>(value);
  }
  double real() const {
    return boost::get<double>(value);
  }
  const std::string& string() const {
    return boost::get<std::string>(value);
  }
  bool boolean() const {
    return boost::get<bool>(value);
  }
  const vcl::util::Duration& duration() const {
    return boost::get<vcl::util::Duration>(value);
  }
  const vcl::util::Size& size() const {
    return boost::get<vcl::util::Size>(value);
  }

  bool is_symbol;
  Lexeme():
    token(SIZE_OF_VCL_TOKENS),
    value(),
    token_length(0),
    is_symbol(false)
  {}
};

class Lexer {
 public: // Static interfaces
  static bool IsSymbolChar( int c ) {
    return std::isalnum(c) || c == '_';
  }

  static bool IsSymbolInitChar( int c ) {
    return std::isalpha(c) || c == '_';
  }

  static bool IsExtendedVarChar( int c ) {
    return IsSymbolChar(c) || c == '-';
  }

 public:
  Lexer( const std::string& source , const std::string& file_name ):
    m_source(source),
    m_file_name(file_name),
    m_pos(0),
    m_line(1),
    m_ccount(1),
    m_lexeme(),
    m_state( LEXER_STATE_NORMAL ),
    m_code_segment(false)
  {}

  const Lexeme& lexeme() const {
    return m_lexeme;
  }

  const std::string& source() const {
    return m_source;
  }

  size_t pos() const {
    return m_pos;
  }

  vcl::util::CodeLocation location() const {
    CHECK(m_pos >= static_cast<int>(m_lexeme.token_length));
    return vcl::util::CodeLocation(m_line,
                                   m_ccount,
                                   m_pos-m_lexeme.token_length);
  }

 public:
  const Lexeme& Next();

  // Parser needs to call this function when it knows it is the end of the
  // string interpolation code segment.
  void SetCodeEnd() {
    DCHECK( m_code_segment );
    m_code_segment = false;
  }

  // The VCL's syntax/grammar is *not* context free. In their sample,
  // they allow field 'A-B-C' existed as a variable name. This is kind
  // of awful since a variable name A-B can be interpreted as:
  // 1. A (variable)
  // 2. - (sub)
  // 3. B (variable)
  // This function existed sololy for the parser to indicate lexer , now
  // tries to scan the next token as a extended variable name which *allow*
  // that dash or minus sign existed.
  bool TryTokenAsExtendedVar();

 public:
  bool Expect( Token tk ) {
    if(tk == m_lexeme.token) {
      Next();
      return true;
    } else {
      return false;
    }
  }

  bool Try( Token tk ) {
    if(Next().token == tk) {
      return true;
    } else {
      return false;
    }
  }

 private: // Predicator
  const Lexeme& LexCode();
  const Lexeme& LexStringInterpolation();

  struct Predicator {
    Token token;
    int next_char;
  };

  template< size_t LEN >
  const Lexeme& Lookahead( const Predicator (&predicators)[LEN] ,
      Token fallback ) {
    int nchar = m_source[m_pos+1];
    for( size_t i = 0 ; i < LEN ; ++i ) {
      if( predicators[i].next_char == nchar ) {
        m_lexeme.token = predicators[i].token;
        m_lexeme.token_length = 2;
        m_lexeme.is_symbol = false;
        m_pos += 2;
        m_ccount += 2;
        return m_lexeme;
      }
    }
    if(fallback == TK_ERROR) {
      LexerError();
    } else {
      m_lexeme.is_symbol = false;
      m_lexeme.token_length = 1;
      m_lexeme.token = fallback;
      m_pos += 1;
      m_ccount += 1;
    }
    return m_lexeme;
  }

  const Lexeme& Lookahead( Token token1 , int nchar1 , Token fallback ) {
    Predicator p[] = {
      {token1,nchar1}
    };
    return Lookahead(p,fallback);
  }

  const Lexeme& Lookahead( Token token1 , int nchar1 ,
                   Token token2 , int nchar2 ,
                   Token fallback ) {
    Predicator p[] = {
      {token1,nchar1},
      {token2,nchar2}
    };
    return Lookahead(p,fallback);
  }

  const Lexeme& Lookahead( Token token1 , int nchar1 ,
                   Token token2 , int nchar2 ,
                   Token token3 , int nchar3 ,
                   Token fallback ) {
    Predicator p[] = {
      {token1,nchar1},
      {token2,nchar2},
      {token3,nchar3}
    };
    return Lookahead(p,fallback);
  }

 private:
  void SkipLineComment();
  bool SkipMultilineComment();
  const Lexeme& LexerError();
  const Lexeme& LexerError( const char* format , ... );
  const Lexeme& LexNumPrefix();
  const Lexeme& LexReal( int );
  // Order matters
  enum {
    SIZE_B = 0,
    SIZE_KB,
    SIZE_MB,
    SIZE_GB
  };
  template< int TYPE >
  const Lexeme& LexSize( int , int );
  template< int TYPE >
  const Lexeme& LexSizeImpl( int , int );
  // Order matters
  enum {
    DURATION_MSEC = 0,
    DURATION_SEC,
    DURATION_MIN,
    DURATION_HOUR
  };
  template< int TYPE >
  const Lexeme& LexDuration( int , int );
  template< int TYPE >
  const Lexeme& LexDurationImpl( int , int );

  template< int DELIMITER >
  const Lexeme& LexLineStr();
  const Lexeme& LexMultilineStr();
  const Lexeme& LexVarOrKeyword();
  const Lexeme& LexVar( int start );

 private:
  const std::string& m_source;
  const std::string& m_file_name;
  int m_pos;
  int m_line;
  int m_ccount;
  Lexeme m_lexeme;

  // Stats for supporting string interpolation
  enum {
    LEXER_STATE_NORMAL ,
    LEXER_STATE_STRING_INTERPOLATION
  };

  // State represent the lexer state. If it is normal then just mean we are
  // lexing the normal code word. If it is in STRING_INTERPOLATION, then it
  // means we are lexing inside of the string interpolation.
  int m_state;

  // Flag to tell whether we are in code segment inside of the string interpolation
  // This flag is only valid when m_state == LEXER_STATE_STRING_INTERPOLATION
  bool m_code_segment;
};

template< int TYPE >
const Lexeme& Lexer::LexDurationImpl( int start , int digit ) {
  uint32_t value;
  try {
    value = boost::lexical_cast<uint32_t>(
        m_source.substr(m_pos,(digit-m_pos)));
  } catch( ... ) {
    return LexerError("cannot convert duration quantity component %s to uint32_t!",
        m_source.substr(m_pos,(start-m_pos)).c_str());
  }

  vcl::util::Duration& dur = boost::get<vcl::util::Duration>(m_lexeme.value);
  switch(TYPE) {
    case DURATION_MSEC:dur.millisecond=value; break;
    case DURATION_SEC: dur.second=value; break;
    case DURATION_MIN: dur.minute=value; break;
    case DURATION_HOUR:dur.hour = value; break;
    default: VCL_UNREACHABLE(); break;
  }

  m_pos = start;

  if(DURATION_MSEC != TYPE) {
    const char* src = m_source.c_str();
    int pos = start;

    // Skip the number component as much as possible
    while( std::isdigit(src[pos]) )
      ++pos;
    switch(src[pos]) {
      case 'm':
        if(src[pos+1] == 's' && DURATION_MSEC < TYPE)
          return LexDurationImpl<DURATION_MSEC>(pos+2,pos);
        else if(src[pos+1] == 'i' && src[pos+2] == 'n' && DURATION_MIN < TYPE)
          return LexDurationImpl<DURATION_MIN>(pos+3,pos);
      case 's':
        if(DURATION_SEC < TYPE)
          return LexDurationImpl<DURATION_SEC>(pos+1,pos);
        break;
      default:
        break;
    }
  }

  // When we reach here it means we finish parsing our Size literals
  m_lexeme.token = TK_DURATION;
  m_lexeme.is_symbol = false;
  return m_lexeme;
}

template< int TYPE >
const Lexeme& Lexer::LexDuration( int start , int digit ) {
  size_t p = m_pos;
  m_lexeme.value = vcl::util::Duration();
  LexDurationImpl<TYPE>(start,digit);
  m_lexeme.token_length = (m_pos - p);
  return m_lexeme;
}

template< int TYPE >
const Lexeme& Lexer::LexSizeImpl( int start , int digit ) {
  uint32_t value;
  try {
    value = boost::lexical_cast<uint32_t>(m_source.substr(m_pos,(digit-m_pos)));
  } catch( ... ) {
    return LexerError("cannot convert size quantity component %s to uint32_t!",
        m_source.substr(m_pos,(start-m_pos)).c_str());
  }

  vcl::util::Size& size = boost::get<vcl::util::Size>(m_lexeme.value);
  switch(TYPE) {
    case SIZE_B: size.bytes = value; break;
    case SIZE_KB:size.kilobytes=value; break;
    case SIZE_MB:size.megabytes=value; break;
    case SIZE_GB:size.gigabytes=value; break;
    default: VCL_UNREACHABLE(); break;
  }

  m_pos = start;

  if(SIZE_B != TYPE) {
    const char* src = m_source.c_str();
    int pos = start;

    // Skip the number component as much as possible
    while( std::isdigit(src[pos]) )
      ++pos;
    switch(src[pos]) {
      case 'm':
        if(src[pos+1] == 'b' && SIZE_MB < TYPE )
          return LexSizeImpl<SIZE_MB>(pos+2,pos);
        break;
      case 'M':
        if(src[pos+1] == 'B' && SIZE_MB < TYPE )
          return LexSizeImpl<SIZE_MB>(pos+2,pos);
        break;
      case 'K':
        if(src[pos+1] == 'B' && SIZE_KB < TYPE )
          return LexSizeImpl<SIZE_KB>(pos+2,pos);
        break;
      case 'k':
        if(src[pos+1] == 'b' && SIZE_KB < TYPE )
          return LexSizeImpl<SIZE_KB>(pos+2,pos);
        break;
      case 'b': case 'B':
        if(SIZE_B < TYPE)
          return LexSizeImpl<SIZE_B>(pos+1,pos);
        break;
      default:
        break;
    }
  }

  // When we reach here it means we finish parsing our Size literals
  m_lexeme.token = TK_SIZE;
  m_lexeme.is_symbol = false;
  return m_lexeme;
}

template< int TYPE >
const Lexeme& Lexer::LexSize( int start , int pos ) {
  size_t p = m_pos;
  m_lexeme.value = vcl::util::Size();
  LexSizeImpl<TYPE>(start,pos);
  m_lexeme.token_length = (m_pos - p);
  return m_lexeme;
}

} // namespace vm
} // namespace vcl

#endif // LEXER_H_

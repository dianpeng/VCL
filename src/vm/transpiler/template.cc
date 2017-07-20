#include "template.h"
#include <vcl/util.h>
#include <cctype>

namespace vcl {
namespace vm  {
namespace transpiler {
namespace {

enum {
  TOKEN_TEXT ,
  TOKEN_KEY  ,
  TOKEN_ERROR,
  TOKEN_EOF
};

class TemplateScanner {
 public:
  TemplateScanner( const std::string& text ):
    m_text(text),
    m_position(0),
    m_token(TOKEN_ERROR),
    m_string()
  {}

  int Next();

  const std::string& string() const { return m_string; }

 private:
  int LexKey();

  const std::string& m_text;
  size_t m_position;
  int m_token;
  std::string m_string;
};


int TemplateScanner::Next() {
  const char* src = m_text.c_str();
  m_string.clear();

  for( ; m_position < m_text.size() ; ++m_position ) {
    int c = src[m_position];
    switch(c) {
      case '$': {
        int nc = src[m_position+1];
        if(nc == '{') {
          if(m_string.empty())
            return LexKey();
          else
            return (m_token = TOKEN_TEXT);
        }
      }
      break;
      case 0:
        VCL_UNREACHABLE();
        break;
      default:
        break;
    }
    m_string.push_back(c);
  }

  if(m_string.empty()) {
    m_token = TOKEN_EOF;
  } else {
    m_token = TOKEN_TEXT;
  }
  return m_token;
}


int TemplateScanner::LexKey() {
  DCHECK( m_text[m_position] == '$' && m_text[m_position+1] == '{' );
  m_position += 2;

  const char* src = m_text.c_str();

  // skip the leading spaces
  for( ; src[m_position] ; ++m_position) {
    if(src[m_position] == '}')
      return (m_token = TOKEN_EOF);
    else if(!std::isspace(src[m_position]))
      break;
  }

  if(!src[m_position])
    return (m_token = TOKEN_EOF);

  // read the key into the m_string buffer
  for( ; src[m_position] ; ++m_position) {
    if(std::isspace(src[m_position])) break;
    if(src[m_position] == '}') break;
    m_string.push_back(src[m_position]);
  }

  if(!src[m_position])
    return (m_token = TOKEN_EOF);

  if(std::isspace(src[m_position])) {
    // skip the trailing spaces
    for( ++m_position ; src[m_position] && src[m_position] != '}' ;
        ++m_position ) ;
    if(!src[m_position])
      return (m_token = TOKEN_EOF);
  }

  DCHECK( src[m_position] == '}' );
  ++m_position;
  return (m_token = TOKEN_KEY);
}


} // namespace

bool Template::Render( const std::string& text , Template::Argument& arg ,
    std::string* output ) {
  TemplateScanner scanner(text);
  int token = scanner.Next();

  do {
    switch(token) {
      case TOKEN_KEY:
        {
          Argument::iterator itr = arg.find(scanner.string());
          if(itr != arg.end()) {
            switch(itr->second.which()) {
              case STRING:
                output->append( boost::get<std::string>(itr->second) );
                break;
              case FUNCTION:
                {
                  Generator& gen = boost::get<Generator>(itr->second);
                  if(!gen(scanner.string(),output))
                    return false;
                  break;
                }
              default:
                VCL_UNREACHABLE();
                break;
            }
          } else {
            return false;
          }
        }
        break;
      case TOKEN_TEXT:
        output->append(scanner.string());
        break;
      default:
        VCL_UNREACHABLE();
        break;
    }
  } while((token = scanner.Next()) != TOKEN_EOF);
  return true;
}

} // namespace transpiler
} // namespace vm
} // namespace vcl

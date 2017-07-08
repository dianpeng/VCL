#include "procedure.h"
#include "zone.h"
#include "vcl-pri.h"

namespace vcl {
namespace vm  {
namespace {
struct StringComparator {
  bool operator () ( const vcl::String* left ,
      const zone::ZoneString* right ) const {
    return *left == right->data();
  }
};

struct StringStdStringComparator {
  bool operator() ( const vcl::String* left , const std::string& right ) const {
    return *left == right;
  }
};
} // namespace

int Procedure::Add( vcl::ImmutableGC* gc , zone::ZoneString* string ) {
  int index = Find<vcl::String*,
                   zone::ZoneString*,
                   StringComparator>(string);
  if(index <0) {
    vcl::String* s = gc->NewString( string->data() );
    m_lit_array.push_back(detail::Value(s));
    return static_cast<int>(m_lit_array.size()-1);
  } else {
    return index;
  }
}

// Just do a stupid copy and paste here
int Procedure::Add( vcl::ImmutableGC* gc , const std::string& string ) {
  int index = Find<vcl::String*,
                   const std::string&,
                   StringStdStringComparator>(string);
  if(index <0) {
    vcl::String* s = gc->NewString( string );
    m_lit_array.push_back(detail::Value(s));
    return static_cast<int>(m_lit_array.size()-1);
  } else {
    return index;
  }
}

int Procedure::Add( vcl::ImmutableGC* gc , IPPattern* pattern ) {
  vcl::InternalAllocator allocator(gc);
  vcl::ACL* new_acl = allocator.NewACL(pattern);
  m_lit_array.push_back(detail::Value(new_acl));
  return static_cast<int>(m_lit_array.size()-1);
}

void Procedure::Dump( std::ostream& output ) const {
  output<<"Protocol:"<<m_protocol<<"\n\n";
  // Const literal dump
  for( size_t i = 0 ; i < m_lit_array.size() ; ++i) {
    const detail::Value& v = m_lit_array[i];
    switch(v.which()) {
      case detail::VALUE_TYPE_INTEGER:
        output<<(i)<<". "<<boost::get<int32_t>(v)<<'\n';
        break;
      case detail::VALUE_TYPE_REAL:
        output<<(i)<<". "<<boost::get<double>(v)<<'\n';
        break;
      case detail::VALUE_TYPE_STRING:
        output<<(i)<<". "<<boost::get<vcl::String*>(v)->data()<<'\n';
        break;
      case detail::VALUE_TYPE_SIZE:
        output<<(i)<<". "<<boost::get<vcl::util::Size>(v)<<'\n';
        break;
      case detail::VALUE_TYPE_DURATION:
        output<<(i)<<". "<<boost::get<vcl::util::Duration>(v)<<'\n';
        break;
      case detail::VALUE_TYPE_ACL:
        output<<(i)<<". "<<"__acl__\n";
        break;
      default:
        VCL_UNREACHABLE();return;
    }
  }
  output<<'\n';
  m_code_buffer.Serialize(output);
}

} // namespace vm
} // namespace vcl

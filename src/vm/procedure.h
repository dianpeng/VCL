#ifndef PROCEDURE_H_
#define PROCEDURE_H_
#include <string>
#include <vector>
#include <boost/variant.hpp>

#include "bytecode.h"
#include <vcl/util.h>

namespace vcl {
class String;
class ImmutableGC;
class ACL;

namespace vm  {
class Compiler;
class IPPattern;

namespace zone {
class ZoneString;
} // namespace zone

namespace detail {

typedef boost::variant<int32_t,
                       double,
                       vcl::String*,
                       vcl::util::Size,
                       vcl::util::Duration,
                       vcl::ACL*> Value;
enum {
  VALUE_TYPE_INTEGER,
  VALUE_TYPE_REAL,
  VALUE_TYPE_STRING,
  VALUE_TYPE_SIZE,
  VALUE_TYPE_DURATION,
  VALUE_TYPE_ACL
};

template< typename T >
struct type_to_id {
  static const int value = -1;
};

template< typename T >
struct type_to_rtype {
  typedef T return_type;
};

#define DEFINE_TYPE_MAP(T,TT,RT) \
template<> struct type_to_id<T> { \
  static const int value = TT; \
}; \
template<> struct type_to_rtype<T> { \
  typedef RT return_type; \
};

DEFINE_TYPE_MAP(int32_t,VALUE_TYPE_INTEGER,int32_t)
DEFINE_TYPE_MAP(double,VALUE_TYPE_REAL,double)
DEFINE_TYPE_MAP(vcl::String*,VALUE_TYPE_STRING,vcl::String*)
DEFINE_TYPE_MAP(vcl::util::Size,VALUE_TYPE_SIZE,const vcl::util::Size&)
DEFINE_TYPE_MAP(vcl::util::Duration,VALUE_TYPE_DURATION,const vcl::util::Duration&)
DEFINE_TYPE_MAP(vcl::ACL*,VALUE_TYPE_ACL,vcl::ACL*)

#undef DEFINE_TYPE_MAP // DEFINE_TYPE_MAP

template< typename T >
struct PrimitiveComparator {
  bool operator () ( const T& l , const T& r ) const {
    return l == r;
  }
};
} // namespace detail


// Procedure is the implementation of SubRoutine object. SubRoutine object is just
// a shim layer of procedure object. A procedure object is bounded to use ImmutableGC
// and it stores inside of CompiledCode object. Whenever a new Context is created,
// then a SubRoutine object will be created with ContextGC and rerefence to this
// procedure object. A procedure object is sort of permenent since it represents a
// compiled procedure defined in code. A SubRoutine object represents its runtime.
class Procedure {
 public:
  Procedure( const std::string& name ,
             const std::string& protocol ,
             size_t arg_count ):
    m_name(name),
    m_code_buffer(),
    m_protocol(protocol),
    m_arg_count(arg_count),
    m_lit_array()
  {}
 public: // Protocol related
  const std::string& name() const {
    return m_name;
  }
  const std::string& protocol() const {
    return m_protocol;
  }
  size_t argument_size() const {
    return m_arg_count;
  }

  // Code buffer
  BytecodeBuffer& code_buffer() {
    return m_code_buffer;
  }

  const BytecodeBuffer& code_buffer() const {
    return m_code_buffer;
  }

  // Dump the code to output
  void Dump( std::ostream& output ) const;
 public:
  int Add( vcl::ImmutableGC* gc , zone::ZoneString* str );
  int Add( vcl::ImmutableGC* gc , const std::string& );
  int Add( int32_t value ) {
    return AddImpl<int32_t,detail::PrimitiveComparator<int32_t> >(value);
  }
  int Add( double value ) {
    return AddImpl<double,detail::PrimitiveComparator<double> >(value);
  }
  int Add( const vcl::util::Size& value ) {
    return AddImpl<vcl::util::Size,
                   detail::PrimitiveComparator<vcl::util::Size> >(value);
  }
  int Add( const vcl::util::Duration& value ) {
    return AddImpl<vcl::util::Duration,
                   detail::PrimitiveComparator<vcl::util::Duration> >(value);
  }
  int Add( vcl::ImmutableGC* , IPPattern* );

 public:
  template< typename T >
  typename detail::type_to_rtype<T>::return_type
  Index( int index ) const {
    DCHECK( index < static_cast<int>(m_lit_array.size()) );
    DCHECK( detail::type_to_id<T>::value != -1 );
    const detail::Value& v = m_lit_array[index];
    DCHECK( v.which() == detail::type_to_id<T>::value );
    return boost::get<T>(v);
  }

  int32_t IndexInteger( int index ) const {
    return Index<int32_t>(index);
  }

  double IndexReal( int index ) const {
    return Index<double>(index);
  }

  vcl::String* IndexString( int index ) const {
    return Index<vcl::String*>(index);
  }

  const vcl::util::Size& IndexSize( int index ) const {
    return Index<vcl::util::Size>(index);
  }

  const vcl::util::Duration& IndexDuration( int index ) const {
    return Index<vcl::util::Duration>(index);
  }

  vcl::ACL* IndexACL( int index ) const {
    return Index<vcl::ACL*>(index);
  }

 private:
  template< typename T , typename IT , typename CMP >
  int Find( IT value ) {
    for( size_t i = 0 ; i < m_lit_array.size() ; ++i ) {
      detail::Value& v = m_lit_array[i];
      if(v.which() == detail::type_to_id<T>::value &&
         CMP()(boost::get<T>(v),value)) return static_cast<int>(i);
    }
    return -1;
  }

  template< typename T , typename CMP >
  int AddImpl( T value ) {
    int index = Find<T,T,CMP>(value);
    if(index <0) {
      m_lit_array.push_back( detail::Value(value) );
      return static_cast<int>(m_lit_array.size()-1);
    } else {
      return index;
    }
  }

  std::string m_name;
  BytecodeBuffer m_code_buffer;
  std::string m_protocol;
  size_t m_arg_count;
  typedef std::vector<detail::Value> LiteralArray;
  LiteralArray m_lit_array;
  friend class Compiler;
};

} // namespace vm
} // namespace vcl

#endif // PROCEDURE_H_

#ifndef UTIL_H_
#define UTIL_H_
#include "config.h"

#include <string>
#include <vector>
#include <glog/logging.h>
#include <boost/static_assert.hpp>

#define VCL_UNREACHABLE() LOG(FATAL)<<"[Unreachable]"

namespace vcl {
namespace util{

struct Duration {
  uint32_t hour;
  uint32_t minute;
  uint32_t second;
  uint32_t millisecond;

  Duration( uint32_t h = 0 , uint32_t min = 0 , uint32_t s = 0 ,
                                                uint32_t m = 0 ):
    hour(h),
    minute(min),
    second(s),
    millisecond(m)
  {}

 public:
  inline Duration& operator + ( const Duration& );
  inline Duration& operator - ( const Duration& );
  bool operator == ( const Duration& duration ) const {
    return hour == duration.hour &&
           minute == duration.minute &&
           second == duration.second &&
           millisecond == duration.millisecond;
  }
 public:
  static inline std::string ToString( const Duration& );
};

struct Size {
  uint32_t bytes;
  uint32_t kilobytes;
  uint32_t megabytes;
  uint32_t gigabytes;

  Size( uint32_t gb = 0 , uint32_t mb = 0 , uint32_t kb = 0 ,
                                            uint32_t b = 0 ):
    bytes(b),
    kilobytes(kb),
    megabytes(mb),
    gigabytes(gb)
  {}
 public:
  inline Size& operator + ( const Size& );
  inline Size& operator - ( const Size& );

  bool operator == ( const Size& size ) const {
    return bytes == size.bytes &&
           kilobytes == size.kilobytes &&
           megabytes == size.megabytes &&
           gigabytes == size.gigabytes;
  }
 public:
  static inline std::string ToString( const Size& );
};

struct CodeLocation {
  int line;
  int ccount;
  size_t position;

  CodeLocation():
    line(0),
    ccount(0),
    position(0)
  {}

  CodeLocation( int l , int c , size_t p ):
    line(l),
    ccount(c),
    position(p)
  {}
};

inline char* StringAsArray( std::string& output , size_t offset = 0 ) {
  return &(*(output.begin())) + offset;
}

inline const char* StringAsArray( const std::string& output , size_t offset = 0 ) {
  return &(*output.begin()) + offset;
}

template< typename T >
inline T* VectorAsArray( std::vector<T>& vec , size_t offset = 0 ) {
  return &(*vec.begin()) + offset;
}

template< typename T >
inline const T* VectorAsArray( const std::vector<T>& vec , size_t offset = 0 ) {
  return &(*vec.begin()) + offset;
}

// Code diagnostic information display ======================================
std::string GetCodeSnippetHighlight( const std::string& , const CodeLocation& );

std::string ReportErrorV( const std::string& source ,
    const CodeLocation& loc ,
    const char* module,
    const char* format ,
    va_list );

inline std::string ReportError( const std::string& source ,
    const CodeLocation& loc ,
    const char* module,
    const char* format ,
    ... ) {
  va_list vl;
  va_start(vl,format);
  return ReportErrorV(source,loc,module,format,vl);
}

// Operators ================================================================
inline std::ostream& operator << ( std::ostream& output ,
    const CodeLocation& cl ) {
  return output<<cl.line<<':'<<cl.ccount;
}

inline std::ostream& operator << ( std::ostream& output ,
    const Duration& duration ) {
  return (output << Duration::ToString(duration));
}

inline std::ostream& operator << ( std::ostream& output ,
    const Size& size ) {
  return (output << Size::ToString(size));
}

inline std::string Duration::ToString( const Duration& duration ) {
  std::stringstream formatter;
  if(duration.hour) formatter << duration.hour << 'h';
  if(duration.minute) formatter << duration.minute << "min";
  if(duration.second) formatter << duration.second << 's';
  if(duration.millisecond) formatter << duration.millisecond << "ms";
  return formatter.str();
}

inline std::string Size::ToString( const Size& size ) {
  std::stringstream formatter;
  if(size.gigabytes) formatter<<size.gigabytes<<"gb";
  if(size.megabytes) formatter<<size.megabytes<<"mb";
  if(size.kilobytes) formatter<<size.kilobytes<<"kb";
  if(size.bytes)     formatter<<size.bytes<<'b';
  return formatter.str();
}

std::string RealToString( double );

// Format APIs ==================================================
void FormatV( std::string* , const char* , va_list );

inline std::string FormatV( const char* format , va_list vl ) {
  std::string ret;
  FormatV(&ret,format,vl);
  return ret;
}

inline std::string Format( const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  return FormatV(format,vl);
}

inline void Format( std::string* output , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  FormatV(output,format,vl);
}

// Miscs APIs ===============================================
bool LoadFile( const std::string& file , std::string* output );

enum FilePathStatus {
  FILE_PATH_ABSOLUTE,
  FILE_PATH_RELATIVE,
  FILE_PATH_UNKNOWN
};

FilePathStatus GetFilePathStatus( const std::string& );

inline bool IsDirectorySeparator( int c ) {
  return c == '/';
}

inline int GetDirectorySeparator() {
  return '/';
}

// ======================================================================
// Work around C++03 doesn't have std::unique_ptr and boost:scoped_ptr or
// boost::shared_ptr cannot give up ownership.
template < typename T >
class scoped_ptr {
 public:
  explicit scoped_ptr( T* ptr ):
    m_ptr(ptr)
  {}

  scoped_ptr():
    m_ptr(NULL)
  {}

  ~scoped_ptr() {
    if(m_ptr) delete m_ptr;
  }

  T* operator -> () {
    DCHECK(m_ptr);
    return m_ptr;
  }

  const T* operator ->() const {
    DCHECK(m_ptr);
    return m_ptr;
  }

  T& operator* () {
    DCHECK(m_ptr);
    return *m_ptr;
  }

  const T& operator* () const {
    DCHECK(m_ptr);
    return *m_ptr;
  }

  T* get() {
    return m_ptr;
  }

  const T* get() const {
    return m_ptr;
  }

  void reset( T* ptr ) {
    if(m_ptr) delete m_ptr;
    m_ptr = ptr;
  }

  T* release() {
    T* ret = m_ptr;
    m_ptr = NULL;
    return ret;
  }

  operator bool() const {
    return m_ptr != NULL;
  }

 private:
  T* m_ptr;
  VCL_DISALLOW_COPY_AND_ASSIGN(scoped_ptr);
};

template< typename T > void Destruct( T* ptr ) {  ptr->~T(); }


} // namespace util
} // namespace vcl

#endif // UTIL_H_

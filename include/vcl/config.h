#ifndef CONFIG_H_
#define CONFIG_H_
#include <cstdarg>
#include <inttypes.h>

#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_FINAL
#define VCL_FINAL final
#else
#define VCL_FINAL
#endif // BOOST_NO_CXX11_FINAL

#define VCL_DISALLOW_COPY_AND_ASSIGN(X) \
  void operator = ( const X& ); \
  X( const X& );

#define VCL_DISALLOW_ASSIGN(X) \
  void operator = ( const X& );

#define VCL_UNUSED(X) (void)(X)

#endif // CONFIG_H_

#ifndef TEMPLATE_H_
#define TEMPLATE_H_
#include <map>
#include <string>

#include <boost/variant.hpp>
#include <boost/function.hpp>


namespace vcl {
namespace vm  {
namespace transpiler {

// A template is a simple in memory template engine. It will replace anything inside of
// text with format of ${key} and use the key to do a lookup inside of the map you provided
// and then substitute the value from that map.
class Template {
 public:
  enum { STRING , FUNCTION }; // Order matters
  // Generator interface is used to customize the template engine
  // when it reaches a certain substitution key, instead of directly
  // emit string store in the Argument map , but call the callback
  // function Generate.
  typedef boost::function< bool ( const std::string& , std::string* ) > Generator;

  // The value that gonna store inside of the key value map for later
  // rendering process and substituion
  typedef boost::variant<std::string,Generator> Value;

  // Argument is the key value used for rendering
  typedef std::map<std::string,Value> Argument;

  // Helper function for us to generate Value object
  static Value Str( const char* string ) {
    return Value( std::string(string) );
  }

  static Value Str( const std::string& string ) {
    return Value(string);
  }

 public:
  // Public API for real rendering
  bool Render( const std::string& text , Argument& argument , std::string* output );
};

} // namespace transpiler
} // namespace vm
} // namespace vcl

#endif // TEMPLATE_H_

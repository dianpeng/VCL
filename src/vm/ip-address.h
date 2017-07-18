#ifndef IP_ADDRESS_H_
#define IP_ADDRESS_H_
#include <vcl/util.h>
// OS specific header
#include <netinet/in.h>

#include <string>

namespace vcl {
namespace vm {
namespace ast {
struct ACL;
}  // namespace ast

// Check the input string is valid ipv4 or ipv6 address. It does support
// special string named localhost.
bool IsValidIPAddress(const char* string);

// IPPattern is a class represent the common ip string representation or
// ip wildcard string representation. The Match function inside of it allow
// user to match a string against a specific ip pattern. This class is mainly
// used by ACL implementation
class IPPattern {
 public:
  virtual bool Match(const char* ip_name) const = 0;
  virtual bool Match(const in_addr&) const = 0;
  virtual bool Match(const in6_addr&) const = 0;

  bool Match(const std::string& ip_name) const {
    return Match(ip_name.c_str());
  }

  virtual ~IPPattern() {}

 public:  // Interfaces
  // Compile an IPAddress or IPAdress wildcard into a IPPattern objects
  // which can be used to do matching against the string ip name.
  static IPPattern* Compile(const ast::ACL& acl_node);
};

}  // namespace vm
}  // namespace vcl

#endif  // IP_ADDRESS_H_

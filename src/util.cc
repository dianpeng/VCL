#include <vcl/util.h>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace vcl {
namespace util {

namespace {

const size_t kPrefixBufferLength = 256;
const size_t kSourceCodeSnippetLength = 128;

void FormatPrefix(const std::string& source,
                  const CodeLocation& loc,
                  const char* module,
                  std::string* output) {
  // 2. Get the location/coordinate that we can highlight the error place
  output->append(
      (boost::format(
           "[%s]:\naround line %d and position %d ,close to source code:%s") %
       module % loc.line % loc.ccount % GetCodeSnippetHighlight(source, loc))
          .str());
}

size_t FindNearestLineBreakBackward(const std::string& source, size_t start) {
  int istart = static_cast<int>(start);  // Avoid overflow with unsigned
  for (--istart; istart >= 0; --istart) {
    if (source[istart] == '\n') break;
  }
  if (istart < 0) istart = 0;
  return static_cast<size_t>(istart);
}

size_t FindNearestLineBreakForward(const std::string& source, size_t start) {
  int istart = static_cast<int>(start);
  for (; istart < static_cast<int>(source.size()); ++istart) {
    if (source[istart] == '\n') break;
  }
  return static_cast<size_t>(istart);
}

}  // namespace

std::string GetCodeSnippetHighlight(const std::string& source,
                                    const CodeLocation& loc) {
  static const char* kPrefix = " |   ";
  static size_t kHalfBufferLength = kSourceCodeSnippetLength / 2;
  size_t start =
      loc.position > kHalfBufferLength ? loc.position - kHalfBufferLength : 0;

  size_t end = source.size() <= (loc.position + kHalfBufferLength)
                   ? source.size()
                   : loc.position + kHalfBufferLength;

  start = FindNearestLineBreakBackward(source, start);
  end = FindNearestLineBreakForward(source, end);

  std::string ret;
  ret.reserve((end - start) + 128);
  ret.append("\n\n");
  {
    size_t position = 1;
    size_t line = 1;
    size_t stop_pos;

    ret.append(kPrefix);

    for (stop_pos = start; stop_pos < end; ++stop_pos) {
      if (source[stop_pos] == '\n') {
        ++line;
        position = 1;
        ret.push_back('\n');
        ret.append(kPrefix);
        if (line > 1 && stop_pos >= loc.position) {
          // find next line break or just end up here
          for (++stop_pos; stop_pos < end; ++stop_pos) {
            if (source[stop_pos] == '\n') break;
          }
          break;
        }
      } else {
        ++position;
        ret.push_back(source[stop_pos]);
      }
    }

    ret.append(
        "\n     ^^^^^^^^^^ error appears before this line ^^^^^^^^^^^\n");
  }
  return ret;
}

std::string ReportErrorV(const std::string& source,
                         const CodeLocation& loc,
                         const char* module,
                         const char* format,
                         va_list vl) {
  va_list backup;
  std::string output;
  char stk_buf[1024];
  size_t buf_cap = 1024;
  char* buf = stk_buf;
  boost::scoped_array<char> heap_buf;
  va_copy(backup, vl);
  int ret = vsnprintf(buf, buf_cap, format, vl);
  if (ret >= 1024) {
    heap_buf.reset(new char[ret + 1]);
    buf = heap_buf.get();
    CHECK(vsnprintf(buf, ret + 1, format, backup) == ret);
  }
  output.reserve(ret + kPrefixBufferLength);
  FormatPrefix(source, loc, module, &output);
  output.append(buf);
  return output;
}

void FormatV(std::string* buffer, const char* format, va_list vl) {
  va_list backup;
  size_t old_size = buffer->size();

  va_copy(backup, vl);
  buffer->resize(old_size + 1024);

  int ret = vsnprintf(StringAsArray(*buffer, old_size), 1024, format, vl);
  if (ret >= 1024) {
    buffer->resize(old_size + ret);
    ret = vsnprintf(StringAsArray(*buffer, old_size), ret, format, backup);
  }
  buffer->resize(old_size + ret);
}

bool LoadFile(const std::string& file_name, std::string* output) {
  std::ifstream input;
  input >> std::noskipws;
  input.open(file_name.c_str(), std::ios::in);

  if (!input) return false;
  std::istream_iterator<char> beg(input), end;
  output->assign(beg, end);
  return true;
}

FilePathStatus GetFilePathStatus(const std::string& path) {
  if (path.front() == GetDirectorySeparator())
    return FILE_PATH_ABSOLUTE;
  else
    return FILE_PATH_RELATIVE;
}

// In the future we can control the precision of showing real number
std::string RealToString(double real) {
  try {
    return boost::lexical_cast<std::string>(real);
  } catch (...) {
    std::stringstream formatter;
    formatter << real;
    return formatter.str();
  }
}

}  // namespace util
}  // namespace vcl

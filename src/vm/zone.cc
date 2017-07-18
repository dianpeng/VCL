#include "zone.h"

namespace vcl {
namespace vm {
namespace zone {

const char* ZoneString::kEmptyCStr = "";

struct Zone::Segment {
  Segment* next;
  Segment() : next(NULL) {}
};

void Zone::Grow(size_t new_cap, size_t guarantee) {
  if (new_cap < guarantee) new_cap += guarantee;
  void* buf = ::malloc(new_cap + sizeof(Segment));
  reinterpret_cast<Segment*>(buf)->next = m_segment;
  m_segment = reinterpret_cast<Segment*>(buf);
  m_capacity = new_cap;
  m_size = new_cap;
  m_pool = static_cast<char*>(buf) + sizeof(Segment);
  m_total_segment_size += new_cap;
}

void Zone::Clear() {
  while (m_segment) {
    Segment* s = m_segment->next;
    ::free(m_segment);
    m_segment = s;
  }
  m_capacity = m_initial_capacity;
  m_size = 0;
  m_pool = NULL;
  m_total_size = 0;
  m_total_segment_size = 0;
}

}  // namespace zone
}  // namespace vm
}  // namespace vcl

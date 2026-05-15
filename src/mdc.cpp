#include <logspine/mdc.hpp>

#include <algorithm>

namespace logspine {

namespace {
thread_local std::vector<field> thread_local_fields;
}

void mdc::put(std::string_view key, field::value_type value) {
  for (auto& f : thread_local_fields) {
    if (f.key() == key) {
      f = field(std::string(key), std::move(value));
      return;
    }
  }
  thread_local_fields.emplace_back(std::string(key), std::move(value));
}

void mdc::remove(std::string_view key) {
  auto it = std::remove_if(thread_local_fields.begin(), thread_local_fields.end(),
                           [key](const field& f) { return f.key() == key; });
  if (it != thread_local_fields.end()) {
    thread_local_fields.erase(it, thread_local_fields.end());
  }
}

void mdc::clear() { thread_local_fields.clear(); }

std::vector<field> mdc::get_all() { return thread_local_fields; }

scoped_mdc::scoped_mdc(std::string_view key, field::value_type value) : key_(key) {
  mdc::put(key_, std::move(value));
}

scoped_mdc::~scoped_mdc() { mdc::remove(key_); }

}  // namespace logspine

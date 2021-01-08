#ifndef ENTRY_HPP
#define ENTRY_HPP

#include <memory>
#include <vector>

template <class T>
class Entry {
 public:
  unsigned nb_buckets;
  unsigned nb_entries;

 private:
  std::unique_ptr<std::vector<T>> entries{nullptr};

 public:
  Entry() : nb_buckets(0), nb_entries(0), entries(nullptr) {}

  Entry(unsigned nb_buckets, unsigned nb_entries)
      : nb_buckets(0), nb_entries(0), entries(nullptr) {
    init(nb_buckets, nb_entries);
  }

  void init(unsigned nb_buckets, unsigned nb_entries) {
    this->nb_buckets = nb_buckets;
    this->nb_entries = nb_entries;
    entries.reset(new std::vector<T>(nb_buckets * nb_entries));
    for (std::size_t i = 0; i != entries->size(); ++i){
      (*entries)[i] = 0.0;
    }
  }

  std::size_t size() { return entries->size(); }

  const T &operator[](const int index) const { return (*entries)[index]; }

  T &operator[](const int index) { return (*entries)[index]; }
};

#endif

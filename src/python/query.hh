#ifndef HGDB_RTL_QUERY_HH
#define HGDB_RTL_QUERY_HH

#include "object.hh"

class Selector : QueryArray {};

class SelectorQueryArray : QueryArray {
public:
    std::vector<std::shared_ptr<Selector>> selectors_;
};

#endif  // HGDB_RTL_QUERY_HH

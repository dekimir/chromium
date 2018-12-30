#include "ramfuzz-rt.hpp"

int main(int argc, char *argv[]) {
  ramfuzz::runtime::gen g(argc, argv);
  // auto cache = g.make<disk_cache::MemBackendImpl>();
  // for (int i = 0, e = g.between(1, 200); i < e; ++i)
  //   cache.CreateEntry();
  //   ignore callback/result?
  // write enough sparse data into entries to pass eviction threshold
  // close all existing entries at once (why not earlier?)
  // create one more entry
}

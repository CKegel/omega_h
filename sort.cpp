#if defined(USE_CUDA)
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <thrust/device_ptr.h>
#include <thrust/sort.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#elif defined(USE_OPENMP)
#include "bundled/parallel_stable_sort.hpp"
#else
#include <algorithm>
#endif

template <typename T, typename Comp>
void parallel_sort(T* b, T* e, Comp c) {
#if defined(USE_CUDA)
  thrust::stable_sort(
      thrust::device_ptr<T>(b),
      thrust::device_ptr<T>(e), c);
#elif defined(USE_OPENMP)
  pss::parallel_stable_sort(b, e, c);
#else
  std::stable_sort(b, e, c);
#endif
}

template <typename T, UInt N>
struct Compare {
  Read<T> keys_;
  Compare(Read<T> keys):keys_(keys) {}
  INLINE bool operator()(const LO& a, const LO& b) {
    for (UInt i = 0; i < N; ++i)
      if (keys_[a * N + i] < keys_[b * N + i])
        return true;
    return false;
  }
};

template <typename T, UInt N>
LOs sort_by_key(Read<T> keys) {
  CHECK(keys.size() % N == 0);
  auto perm = make_linear<LO>(keys.size() / N, 0, 1);
  parallel_sort(&perm[0], &perm[0] + perm.size(), Compare<T,N>(keys));
  return perm;
}
template LOs sort_by_key<U32,1>(Read<U32> keys);
template LOs sort_by_key<U32,2>(Read<U32> keys);
template LOs sort_by_key<U32,3>(Read<U32> keys);
template LOs sort_by_key<U64,1>(Read<U64> keys);
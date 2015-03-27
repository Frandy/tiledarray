/*
 *  This file is a part of TiledArray.
 *  Copyright (C) 2013  Virginia Tech
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  justus
 *  Department of Chemistry, Virginia Tech
 *
 *  vector_op.h
 *  Nov 17, 2013
 *
 */

#ifndef TILEDARRAY_MATH_VECTOR_OP_H__INCLUDED
#define TILEDARRAY_MATH_VECTOR_OP_H__INCLUDED

#include <TiledArray/math/math.h>
#include <TiledArray/math/eigen.h>
#include <TiledArray/type_traits.h>
#include <TiledArray/madness.h>

#ifndef TILEARRAY_ALIGNMENT
#define TILEARRAY_ALIGNMENT 16
#endif // TILEARRAY_ALIGNMENT

#ifndef TILEDARRAY_CACHELINE_SIZE
#define TILEDARRAY_CACHELINE_SIZE 64
#endif // TILEDARRAY_CACHELINE_SIZE

#define TILEDARRAY_LOOP_UNWIND ::TiledArray::math::LoopUnwind::value

// Add macro TILEDARRAY_FORCE_INLINE which does as the name implies.
#if (defined _MSC_VER) || (defined __INTEL_COMPILER)

#define TILEDARRAY_FORCE_INLINE __forceinline

#elif(__clang__)

#define TILEDARRAY_FORCE_INLINE __attribute__((always_inline)) inline

#elif defined(__GNUC__)

#if (__GNUC__ >= 4)
#define TILEDARRAY_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define TILEDARRAY_FORCE_INLINE inline
#endif // (__GNUC__ >= 4)

#else

#define TILEDARRAY_FORCE_INLINE inline

#endif


#if (defined __GNUC__) || (defined __PGI) || (defined __IBMCPP__) || (defined __ARMCC_VERSION)

#define TILEDARRAY_ALIGNED_STORAGE __attribute__((aligned(TILEARRAY_ALIGNMENT)))

#elif (defined _MSC_VER)

#define TILEDARRAY_ALIGNED_STORAGE __declspec(align(TILEARRAY_ALIGNMENT))

#else

#define TILEDARRAY_ALIGNED_STORAGE
#warning FIXEME!!! TiledArray alignment attribute is not definded for this platform.

#endif


namespace TiledArray {
  namespace math {

    // Define compile time constant for loop unwinding.
    typedef std::integral_constant<std::size_t, TILEDARRAY_CACHELINE_SIZE / sizeof(double)> LoopUnwind;
    typedef std::integral_constant<std::size_t, ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul)> index_mask;

    template <std::size_t> struct VectorOpUnwind;

    /// Vector loop unwind helper class

    /// This object will unwind \c 1 step of a vector operation loop, and
    /// terminate the loop
    template <>
    struct VectorOpUnwind<0ul> {

      static constexpr std::size_t offset = TILEDARRAY_LOOP_UNWIND - 1ul;

      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      for_each(Op&& op, Result* restrict const result, const Args* restrict const ...args) {
        op(result[offset], args[offset]...);
      }

      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      for_each_ptr(Op&& op, Result* restrict const result, const Args* restrict const ...args) {
        op(result + offset, args[offset]...);
      }

      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      reduce(Op&& op, Result& restrict result, const Args* restrict const ...args) {
        op(result, args[offset]...);
      }

      template <typename Result, typename Arg>
      static TILEDARRAY_FORCE_INLINE void
      scatter(Result* restrict const result, const Arg* restrict const arg,
          const std::size_t /*result_stride*/)
      {
        *result = arg[offset];
      }

      template <typename Result, typename Arg>
      static TILEDARRAY_FORCE_INLINE void
      gather(Result* restrict const result, const Arg* restrict const arg,
          std::size_t /*arg_stride*/)
      {
        result[offset] = *arg;
      }

    }; //  struct VectorOpUnwind

    /// Vector loop unwind helper class

    /// This object will unwind \c N steps of a vector operation loop.
    /// \tparam N The number of steps to unwind
    template <std::size_t N>
    struct VectorOpUnwind : public VectorOpUnwind<N - 1ul> {

      typedef VectorOpUnwind<N - 1ul> VectorOpUnwindN1;

      static constexpr std::size_t offset = TILEDARRAY_LOOP_UNWIND - N - 1ul;


      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      for_each(Op&& op, Result* restrict const result, const Args* restrict const ...args) {
        op(result[offset], args[offset]...);
        VectorOpUnwindN1::for_each(std::forward<Op>(op), result, args...);
      }

      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      for_each_ptr(Op&& op, Result* restrict const result, const Args* restrict const ...args) {
        op(result + offset, args[offset]...);
        VectorOpUnwindN1::for_each_ptr(std::forward<Op>(op), result, args...);
      }

      template <typename Op, typename Result, typename... Args>
      static TILEDARRAY_FORCE_INLINE void
      reduce(Op&& op, Result& restrict result, const Args* restrict const ...args) {
        op(result, args[offset]...);
        VectorOpUnwindN1::reduce(std::forward<Op>(op), result, args...);
      }

      template <typename Result, typename Arg>
      static TILEDARRAY_FORCE_INLINE void
      scatter(Result* restrict const result, const Arg* restrict const arg,
          const std::size_t result_stride)
      {
        *result = arg[offset];
        VectorOpUnwindN1::scatter(result + result_stride, arg, result_stride);
      }

      template <typename Result, typename Arg>
      static TILEDARRAY_FORCE_INLINE void
      gather(Result* restrict const result, const Arg* restrict const arg,
          std::size_t arg_stride)
      {
        result[offset] = *arg;
        VectorOpUnwindN1::gather(result, arg + arg_stride, arg_stride);
      }

    }; //  struct VectorOpUnwind


    typedef VectorOpUnwind<TILEDARRAY_LOOP_UNWIND - 1> VecOpUnwindN;

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE void
    for_each_block(Op&& op, Result* const result,
        const Args* const... args)
    {
      VecOpUnwindN::for_each(std::forward<Op>(op), result, args...);
    }

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE void
    for_each_block(Op&& op, const std::size_t n, Result* restrict const result,
        const Args* restrict const... args)
    {
      for(std::size_t i = 0ul; i < n; ++i)
        op(result[i], args[i]...);
    }

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE void
    for_each_block_ptr(Op&& op, Result* const result,
        const Args* const... args)
    {
      VecOpUnwindN::for_each_ptr(std::forward<Op>(op), result, args...);
    }

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE void
    for_each_block_ptr(Op&& op, const std::size_t n, Result* restrict const result,
        const Args* restrict const... args)
    {
      for(std::size_t i = 0ul; i < n; ++i)
        op(result + i, args[i]...);
    }

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE
    typename std::enable_if<detail::is_type<typename std::result_of<Op(Result&, Args...)>::type>::value>::type
    reduce_block(Op&& op, Result& result, const Args* const... args) {
      VecOpUnwindN::reduce(std::forward<Op>(op), result, args...);
    }

    template <typename Op, typename Result, typename... Args>
    TILEDARRAY_FORCE_INLINE
    typename std::enable_if<detail::is_type<typename std::result_of<Op(Result&, Args...)>::type>::value>::type
    reduce_block(const Op& op, const std::size_t n, Result& restrict result,
        const Args* restrict const... args)
    {
      for(std::size_t i = 0ul; i < n; ++i)
        op(result, args[i]...);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    copy_block(Result* restrict const result, const Arg* restrict const arg) {
      for_each_block([] (Result& lhs, const Arg rhs) { lhs = rhs; }, result, arg);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    copy_block(std::size_t n, Result* const result, const Arg* const arg) {
      for_each_block([] (Result& lhs, const Arg rhs) { lhs = rhs; }, n, result, arg);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    fill_block(Result* const result, const Arg arg) {
      for_each_block([arg] (Result& lhs) { lhs = arg; }, result);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    fill_block(const std::size_t n, Result* const result, const Arg arg) {
      for_each_block([arg] (Result& lhs) { lhs = arg; }, n, result);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    scatter_block(Result* const result, const std::size_t stride, const Arg* const arg) {
      VecOpUnwindN::scatter(result, arg, stride);
    }


    template <typename Result, typename Arg>
    TILEDARRAY_FORCE_INLINE void
    scatter_block(const std::size_t n, Result* const result,
        const std::size_t stride, const Arg* const arg)
    {
      for(std::size_t i = 0; i < n; ++i, result += stride)
        *result = arg[i];
    }

    template <typename Result, typename Arg>
    TILEDARRAY_FORCE_INLINE void
    gather_block(Result* const result, const Arg* const arg, const std::size_t stride) {
      VecOpUnwindN::gather(result, arg, stride);
    }

    template <typename Arg, typename Result>
    TILEDARRAY_FORCE_INLINE void
    gather_block(const std::size_t n, Result* const result, const Arg* const arg,
        const std::size_t stride)
    {
      for(std::size_t i = 0; i < n; ++i, arg += stride)
        result[i] = *arg;
    }


    template <typename Arg, typename Result, typename Op>
    void binary_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {
        Result* restrict const result_i = result + i;

        TILEDARRAY_ALIGNED_STORAGE Arg arg_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(arg_block, arg + i);
        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(result_block, result_i);

        for_each_block(op, result_block, arg_block);

        copy_block(result + i, result_block);
      }

      for_each_block(op, n - i, result + i, arg + i);
    }

    template <typename Left, typename Right, typename Result, typename Op>
    void binary_vector_op(const std::size_t n, const Left* restrict const left,
        const Right* restrict const right, Result* restrict const result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto wrapper_op = [&op] (Result& res, const Left l, const Right r) {
        res = op(l, r);
      };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Left left_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(left_block, left + i);
        TILEDARRAY_ALIGNED_STORAGE Right right_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(right_block, right + i);

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        for_each_block(wrapper_op, result_block, left_block, right_block);

        copy_block(result + i, result_block);
      }

      for_each_block(wrapper_op, n - i, result + i, left + i, right + i);
    }

    template <typename Result, typename Op>
    void unary_vector_op(const std::size_t n, Result* restrict const result, const Op& op) {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += 8ul) {
        Result* restrict const result_i = result + i;

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(result_block, result_i);

        for_each_block(op, result_block);

        copy_block(result_i, result_block);
      }

      for_each_block(op, n - i, result + i);
    }

    template <typename Arg, typename Result, typename Op>
    void unary_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto wrapper_op = [&op] (Result& res, const Arg a) { res = op(a); };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Arg arg_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(arg_block, arg + i);

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        for_each_block(wrapper_op, result_block, arg_block);

        copy_block(result + i, result_block);
      }

      for_each_block(wrapper_op, n - i, result + i, arg + i);
    }

    template <typename Arg, typename Result>
    typename madness::disable_if_c<std::is_same<Arg, Result>::value && std::is_scalar<Arg>::value>::type
    copy_vector(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND)
        copy_block(result + i, arg + i);
      copy_block(n - i, result + i, arg + i);
    }

    template <typename T>
    inline typename madness::enable_if<std::is_scalar<T> >::type
    copy_vector(const std::size_t n, const T* restrict const arg, T* restrict const result) {
      std::memcpy(result, arg, n * sizeof(T));
    }

    template <typename Arg, typename Result>
    void fill_vector(const std::size_t n, const Arg& restrict arg, Result* restrict const result) {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto op = [arg] (Result& res) { res = arg; };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND)
        for_each_block(op, result + i);
      for_each_block(op, n - i, result + i);
    }


    template <typename Arg, typename Result>
    typename madness::disable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_copy_vector(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto op = [] (Result* restrict const res, const Arg a) { new(res) Result(a); };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND)
        for_each_block_ptr(op, result + i, arg + i);
      for_each_block_ptr(op, n - i, result + i, arg + i);
    }

    template <typename Arg, typename Result>
    inline typename madness::enable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_copy_vector(const std::size_t n, const Arg* restrict const arg, Result* restrict const result) {
      copy_vector(n, arg, result);
    }

    template <typename Arg, typename Result>
    typename madness::disable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_fill_vector(const std::size_t n, const Arg& restrict arg,
        Result* restrict const result)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto op = [arg] (Result* restrict const res) { new(res) Result(arg); };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND)
        for_each_block_ptr(op, result + i);
      for_each_block_ptr(op, n - i, result + i);
    }


    template <typename Arg, typename Result>
    inline typename madness::enable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_fill_vector(const std::size_t n, const Arg& restrict arg,
        Result* restrict const result)
    { fill_vector(n, arg, result); }


    template <typename Arg>
    typename madness::disable_if<std::is_scalar<Arg> >::type
    destroy_vector(const std::size_t n, const Arg* restrict const arg) {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto op = [] (Arg* restrict const a) { a->~Arg(); };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND)
        for_each_block_ptr(op, arg + i);
      for_each_block_ptr(op, n - i, arg + i);
    }

    template <typename Arg>
    inline typename madness::enable_if<std::is_scalar<Arg> >::type
    destroy_vector(const std::size_t, const Arg* restrict const) { }


    template <typename Arg, typename Result, typename Op>
    typename madness::disable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_unary_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto wrapper_op = [&op] (Result* restrict const res, const Arg a) {
        new(res) Result(op(a));
      };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Arg arg_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(arg_block, arg + i);

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        for_each_block_ptr(wrapper_op, result_block, arg_block);

        copy_block(result + i, result_block);

      }

      for_each_block_ptr(wrapper_op, n - i, result + i, arg + i);
    }

    template <typename Arg, typename Result, typename Op>
    inline typename madness::enable_if_c<std::is_scalar<Arg>::value && std::is_scalar<Result>::value>::type
    uninitialized_unary_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result* restrict const result, const Op& op)
    {
      unary_vector_op(n, arg, result, op);
    }


    template <typename Left, typename Right, typename Result, typename Op>
    typename madness::disable_if_c<std::is_scalar<Left>::value &&
        std::is_scalar<Right>::value && std::is_scalar<Result>::value>::type
    uninitialized_binary_vector_op(const std::size_t n, const Left* restrict const left,
        const Right* restrict const right, Result* restrict const result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      auto wrapper_op = [&op] (Result* restrict const res, const Left l, const Right r) {
        new(res) Result(op(l, r));
      };

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Left left_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(left_block, left + i);
        TILEDARRAY_ALIGNED_STORAGE Right right_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(right_block, right + i);

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        for_each_block_ptr(wrapper_op, result_block, left_block, right_block);

        copy_block(result + i, result_block);
      }

      for_each_block_ptr(wrapper_op, n - i, result + i, left + i, right + i);
    }

    template <typename Left, typename Right, typename Result, typename Op>
    typename madness::enable_if_c<std::is_scalar<Left>::value &&
        std::is_scalar<Right>::value && std::is_scalar<Result>::value>::type
    uninitialized_binary_vector_op(const std::size_t n, const Left* restrict const left,
        const Right* restrict const right, Result* restrict const result, const Op& op)
    {
      binary_vector_op(n, left, right, result, op);
    }

    template <typename Left, typename Right, typename Result, typename Op>
    typename madness::disable_if<std::is_pointer<Result> >::type
    reduce_vector_op(const std::size_t n, const Left* restrict const left,
        const Right* restrict const right, Result& restrict result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += 8ul) {

        TILEDARRAY_ALIGNED_STORAGE Left left_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(left_block, left + i);
        TILEDARRAY_ALIGNED_STORAGE Right right_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(right_block, right + i);

        reduce_block(op, result, left_block, right_block);

      }

      reduce_block(op, n - i, result, left + i, right + i);
    }

    template <typename Arg, typename Result, typename Op>
    typename madness::disable_if<std::is_pointer<Result> >::type
    reduce_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result& restrict result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Arg arg_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(arg_block, arg + i);

        reduce_block(op, result, arg_block);

      }

      reduce_block(op, n - i, result, arg + i);
    }


    template <typename Left, typename Right, typename Result, typename Op>
    void reduce_vector_op(const std::size_t n, const Left* restrict const left,
        const Right* restrict const right, Result* restrict result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += 8ul) {

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(result_block, result + i);
        TILEDARRAY_ALIGNED_STORAGE Left left_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(left_block, left + i);
        TILEDARRAY_ALIGNED_STORAGE Right right_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(right_block, right + i);

        for_each_block(op, result_block, left_block, right_block);

        copy_block(result + i, result_block);
      }

      for_each_block(op, n - i, result + i, left + i, right + i);
    }

    template <typename Arg, typename Result, typename Op>
    void reduce_vector_op(const std::size_t n, const Arg* restrict const arg,
        Result* restrict result, const Op& op)
    {
      std::size_t i = 0ul;

      // Compute block iteration limit
      constexpr std::size_t index_mask = ~std::size_t(TILEDARRAY_LOOP_UNWIND - 1ul);
      const std::size_t nx = n & index_mask;

      for(; i < nx; i += TILEDARRAY_LOOP_UNWIND) {

        TILEDARRAY_ALIGNED_STORAGE Result result_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(result_block, result + i);
        TILEDARRAY_ALIGNED_STORAGE Arg arg_block[TILEDARRAY_LOOP_UNWIND];
        copy_block(arg_block, arg + i);

        for_each_block(op, result_block, arg_block);

        copy_block(result + i, result_block);
      }

      for_each_block(op, n - i, result + i, arg + i);
    }

  }  // namespace math
} // namespace TiledArray

#endif // TILEDARRAY_MATH_VECTOR_OP_H__INCLUDED

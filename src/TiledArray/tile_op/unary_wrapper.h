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
 *  Justus Calvin
 *  Department of Chemistry, Virginia Tech
 *
 *  binary_interface.h
 *  Oct 6, 2013
 *
 */

#ifndef TILEDARRAY_TILE_OP_UNARY_WRAPPER_H__INCLUDED
#define TILEDARRAY_TILE_OP_UNARY_WRAPPER_H__INCLUDED

#include <TiledArray/tile_op/tile_interface.h>
#include <TiledArray/permutation.h>
#include <TiledArray/zero_tensor.h>

namespace TiledArray {
  namespace detail {

    /// Unary tile operation wrapper

    /// This wrapper class is handles evaluation of lazily evaluated tiles in unary operations and
    /// forwards the evaluated arguments to the base operation object.
    ///
    /// The base unary operation class must have the following interface.
    /// \code
    /// template <typename Arg, typename Scalar, bool Consumable>
    /// class Operator {
    /// public:
    ///
    ///   typedef ... argument_type;
    ///   typedef ... result_type;
    ///
    ///   static constexpr bool is_consumable =
    ///       Consumable && std::is_same<result_type, Arg>::value;
    ///
    ///   // Constructor
    ///   Operator();
    ///
    ///   // Constructor required for scaling operations only, and may be omitted for other operations
    ///   Operator(const Scalar);
    ///
    ///   // Operation evaluation operators
    ///   // The A template parameter type may be Arg or
    ///   // TiledArray::ZeroTensor.
    ///
    ///   // Evaluate and permute the result.
    ///   template <typename A>
    ///   result_type operator()(A&& arg, const Permutation& perm) const;
    ///
    ///   // Evaluate only
    ///   // If is_consumable is true, then this
    ///   // may consume arg.
    ///   template <typename A>
    ///   result_type operator()(A&& arg) const;
    ///
    ///   // Evaluate the operation and try to consume the
    ///   // argument. This function may not consume arg if it is not consumable.
    ///   template <typename A>
    ///   result_type consume(argument_type& arg) const;
    ///
    /// }; // class Operator
    /// \endcode
    /// \tparam Op The base binary operation type
    template <typename Op>
    class UnaryWrapper {
    public:
      typedef UnaryWrapper<Op> UnaryWrapper_;
      typedef typename Op::argument_type argument_type; ///< Argument type
      typedef typename Op::result_type result_type; ///< The result tile type


      /// Boolean value that indicates the argument can always be consumed
      static constexpr bool is_consumable = Op::is_consumable;

      template <typename T>
      using decay_t = typename std::decay<T>::type;

      template <typename T>
      using is_lazy_tile_t = is_lazy_tile<decay_t<T> >;

      template <typename T>
      using is_array_tile_t = is_array_tile<decay_t<T> >;

      template <typename T>
      using eval_t = typename eval_trait<decay_t<T> >::type;

    private:

      Op op_; ///< Tile operation
      Permutation perm_; ///< Permutation applied to the result

    public:

      // Compiler generated functions
      UnaryWrapper(const UnaryWrapper_&) = default;
      UnaryWrapper(UnaryWrapper_&&) = default;
      ~UnaryWrapper() = default;
      UnaryWrapper_& operator=(const UnaryWrapper_&) = default;
      UnaryWrapper_& operator=(UnaryWrapper_&&) = default;

      UnaryWrapper(const Op& op, const Permutation& perm) : op_(op), perm_(perm) { }

      UnaryWrapper(const Op& op) : op_(op), perm_() { }


      /// Permutation accessor

      /// \return A reference to the permutation applied to the result tile
      const Permutation& permutation() const { return perm_; }


      /// Apply operator to `arg` and possibly permute the result

      /// \param arg The argument
      /// \return The result tile from the unary operation applied to the
      /// \c arg .
      auto operator()(argument_type& arg) const {
        return (perm_ ? op_(arg, perm_) : op_(arg) );
      }

      /// Apply operator to `arg` and possibly permute the result

      /// \param arg The argument
      /// \return The result tile from the unary operation applied to the
      /// \c arg .
      auto operator()(const argument_type& arg) const {
        return (perm_ ? op_(arg, perm_) : op_(arg) );
      }

      // The following operators will evaluate lazy tile and use the base class
      // interface functions to call the correct evaluation kernel.

      /// Evaluate a lazy tile

      /// This function will evaluate `arg`, then pass the evaluated tile to
      /// the `UnaryInterfaceBase_::operator()` function.
      /// \tparam A The lazy tile type
      /// \param arg The lazy tile argument
      /// \return The result of the unary operation applied to the evaluated
      /// `arg`.
      template <typename A,
          typename std::enable_if<
              is_lazy_tile_t<A>::value && (! is_array_tile_t<A>::value)
          >::type* = nullptr>
      auto operator()(A&& arg) const {
        auto cast = Cast<eval_t<A>,decay_t<A>>{};
        using TiledArray::meta::invoke;
        auto cast_arg = invoke(cast, arg);
        return (perm_ ? invoke(op_, cast_arg, perm_) : invoke(op_, cast_arg) );
      }


      /// Evaluate a lazy array tile

      /// This function will evaluate `arg`, then pass the evaluated tile to
      /// the `UnaryInterfaceBase_::operator()` function.
      /// \tparam A The lazy tile type
      /// \param arg The lazy tile argument
      /// \return The result of the unary operation applied to the evaluated
      /// `arg`.
      template <typename A,
          typename std::enable_if<
              is_array_tile_t<A>::value
          >::type* = nullptr>
      result_type operator()(A&& arg) const {
        auto cast = Cast<eval_t<A>,decay_t<A>>{};
        using TiledArray::meta::invoke;
        auto cast_arg = invoke(cast, arg);
//        NB using this generic lambda breaks TaskFn ...
//        need to make TaskFn variadic and accepting callables, but this is a lot of MP
//
//        auto op_consume = [this](auto&& arg) {
//          return op_.consume(std::forward<decltype(arg)>(arg));
//        };
        auto op_consume = [this](eval_t<A>& arg) {
          return op_.consume(arg);
        };
        return (perm_ ?
           invoke(op_, cast_arg, perm_) :
           (arg.is_consumable() ? invoke(op_consume, cast_arg) : invoke(op_, cast_arg) ));
      }

      /// Consume lazy tile
      template <typename A,
          typename std::enable_if<
              is_lazy_tile_t<A>::value
          >::type* = nullptr>
      result_type consume(A&& arg) const {
        auto cast = Cast<eval_t<A>,decay_t<A>>{};
        using TiledArray::meta::invoke;
        auto cast_arg = invoke(cast, arg);
//        NB using this generic lambda breaks TaskFn ...
//        need to make TaskFn variadic and accepting callables, but this is a lot of MP
//
//        auto op_consume = [this](auto&& arg) {
//          return op_.consume(std::forward<decltype(arg)>(arg));
//        };
        auto op_consume = [this](eval_t<A>& arg) {
          return op_.consume(arg);
        };
        return (perm_ ?
            invoke(op_, cast_arg, perm_) :
            invoke(op_consume, cast_arg));
      }

      template <typename A,
          typename std::enable_if<
              ! is_lazy_tile_t<A>::value
          >::type* = nullptr>
      result_type consume(A&& arg) const {
        return (perm_ ?
           op_(std::forward<A>(arg), perm_) :
           op_.consume(std::forward<A>(arg)) );
      }

    }; // class UnaryWrapper

  } // namespace detail
} // namespace TiledArray

#endif // TILEDARRAY_TILE_OP_UNARY_WRAPPER_H__INCLUDED

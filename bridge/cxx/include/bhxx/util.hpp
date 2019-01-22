/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <sstream>
#include <algorithm>
#include "BhArray.hpp"
#include <bhxx/functor.hpp>

namespace bhxx {

/** Convert an array with exactly one element to a scalar by calling
 *  sync and flush and returning the value. */
template<typename T>
T as_scalar(BhArray<T> ary);

/** Convert an array to a contiguous representation if it is not yet
 *  contiguous. */
template<typename T>
BhArray<T> as_contiguous(BhArray<T> ary) {
    if (ary.isContiguous()) return ary;

    BhArray<T> contiguous{ary.shape};
    identity(contiguous, ary);
    return contiguous;
}

/** Insert a broadcast axis of the given size
 *
 * \param axis   Position at which the broadcast axis is to be inserted
 *               (i.e. 4 between 4 and 5, ...)
 * \param size   Size of the new broadcasted axis
 */
template<typename T>
BhArray<T> broadcast(BhArray<T> ary, int64_t axis, size_t size);

/** Perform a matrix-matrix multiplication
 *
 * Multiplies the rightmost dimension of lhs with the leftmost
 * dimension of rhs.
 * */
template<typename T>
BhArray<T> matmul(BhArray<T> lhs, BhArray<T> rhs);

/** Performs a full reduction of the array along all axis using the
 *  add_reduce operation.
 *
 *  \note Performs exactly the same job as std::accumulate, but on
 *  BhArray objects.
 */
template<typename T>
BhArray<T> accumulate(BhArray<T> op) {
    return accumulate(std::move(op), bhxx::AddReduce<T>{});
}

/** Performs a full reduction of the array along all axis using
 *  an reduction operation of the callers choice.
 *
 *  \note Performs exactly the same job as std::accumulate, but on
 *  BhArray objects.
 */
template<typename T, typename AddReduction>
BhArray<T> accumulate(BhArray<T> op, AddReduction &&reduction) {
    // Reduce to a single value by repetitively calling the reduction function
    // until the rank is down to 1:
    const size_t rank = op.rank();
    for (size_t r = 0; r < rank; ++r) {
        op = reduction(op, op.rank() - 1);
    }
    assert(op.rank() == 1);
    assert(op.size() == 1);
    return op;
}

/** Make an inner product between the Bohrium arrays given, i.e.
 *  elementwise multiplication followed by an accumulation
 *  (full reduction).
 *
 *  \note Performs exactly the same job as std::inner_product, but
 *  on BhArray objects.
 */
template<typename T>
BhArray<T> inner_product(const BhArray<T> &oplhs, const BhArray<T> &oprhs) {
    return inner_product(oplhs, oprhs, bhxx::Multiply<T>{}, bhxx::AddReduce<T>{});
}

/** Make an inner product between the Bohrium arrays given, i.e.
 *  elementwise multiplication followed by an accumulation
 *  (full reduction).
 *
 *  This version allows to specify the operations used for multiplication
 *  and addition, such that other things as inner products can be achieved
 *  as well (e.g. equality comparision is multiplication == equal and
 *  add_reduction == local_and_reduce)
 *
 *  \note Performs exactly the same job as std::inner_product, but
 *  on BhArray objects.
 */
template<typename T, typename Multiplication, typename AddReduction>
auto inner_product(const BhArray<T> &oplhs, const BhArray<T> &oprhs,
                   Multiplication &&multiplication, AddReduction &&add_reduction)
-> decltype(multiplication(oplhs, oprhs)) {
    return accumulate(multiplication(oplhs, oprhs),
                      std::forward<AddReduction>(add_reduction));
}


/** Return the result of broadcasting `shapes` against each other
 *
 * @param shapes  Array of shapes
 * @return        Broadcasted shape
 */
template<int N>
Shape broadcasted_shape(std::array<Shape, N> shapes) {
    // Find the number of dimension of the broadcasted shape
    uint64_t ret_ndim = 0;
    for (const Shape &shape: shapes) {
        if (shape.size() > ret_ndim) {
            ret_ndim = shape.size();
        }
    }

    // Make sure that all shapes has the same length by appending ones
    for (Shape &shape: shapes) {
        shape.insert(shape.end(), ret_ndim - shape.size(), 1);
    }

    // The resulting shape is the max of each dimension
    Shape ret;
    for (uint64_t i = 0; i < ret_ndim; ++i) {
        uint64_t greatest = 0;
        for (const Shape &shape: shapes) {
            if (shape[i] > greatest) {
                greatest = shape[i];
            }
        }
        ret.push_back(greatest);
    }
    return ret;
}

/** Return a new view of `ary` that is broadcasted to `shape`
 *
 * @param ary    Input array
 * @param shape  The new shape
 * @return       The broadcasted array
 */
template<typename T>
BhArray<T> broadcast_to(BhArray<T> ary, const Shape &shape) {
    if (ary.shape.size() < shape.size()) {
        throw std::runtime_error("The length of `shape` is smaller than `ary.shape`");
    }
    // Append ones to shape and zeros to stride in order to make them the same lengths as `shape`
    assert(ary.shape.size() == ary.stride.size());
    ary.shape.insert(ary.shape.end(), shape.size() - ary.shape.size(), 1);
    ary.stride.insert(ary.stride.end(), shape.size() - ary.stride.size(), 0);

    // Broadcast each dimension by setting ary.stride to zero and ary.shape to `shape`
    for (uint64_t i = 0; i < shape.size(); ++i) {
        if (ary.shape[i] != shape[i]) {
            if (ary.shape[i] == 1) {
                ary.shape[i] = shape[i];
                ary.stride[i] = 0;
            } else {
                std::stringstream ss;
                ss << "Cannot broadcast `shape[" << i << "]=" << ary.shape << "` to `" << shape[i] << "`.";
                throw std::runtime_error(ss.str());
            }
        }
    }
    return std::move(ary);
}

}  // namespace bhxx

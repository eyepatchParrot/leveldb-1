// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_H_

#include <stddef.h>
#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb {

struct BlockContents;
class Comparator;

struct Interpolator {
        int CountShared(Slice left, Slice right) const {
                int shared = 0;
                for (; shared < left.size() && shared < right.size() && left[shared] == right[shared];
                                shared++);
                return shared-1;
        }

        double ApproxKey(Slice target) const {
                const int approx_size = 8;
                uint64_t rv = 0, place_value = 1, range = '9' - '0' + 1;
                for (int i = 1; i < approx_size; i++) place_value *= range;
                for (int i = shared.size(); i < shared.size() + approx_size; i++, place_value /= range) {
                        rv += place_value * (i >= target.size() || target[i] < '0' || target[i] > '9' ? 0 : target[i] - '0');
                }
                double rv2 = static_cast<double>(rv);
                //assert(rv2 < 34000000);
                return rv2;
        }

        // Expects the contents to already be initialized so it can use the scan functions
        using Index = int64_t;
        Interpolator(Slice left, Slice right, int64_t width) :
                shared(left.data(), CountShared(left, right)),
                first(ApproxKey(left)),
                width_range(width > 0 ? (double)(width) / (double)(ApproxKey(right) - first) : 0.)
                { assert(width_range >= 0.); assert(width_range < 1e9); }

        const Slice shared;
        const double first;
        const double width_range;

        Index operator()(const double target, const Index mid, const double mid_value) const {
                return  mid + (Index)((target - mid_value) * width_range);
        }

        Index operator()(const double target) const {
                return (Index)((target - first) * width_range);
        }
};

class Block {
 public:
  // Initialize the block with the specified contents.
  explicit Block(const BlockContents& contents);

  ~Block();

  size_t size() const { return size_; }
  Iterator* NewIterator(const Comparator* comparator);

 private:
  uint32_t NumRestarts() const;
  Slice Front() const;
  Slice Back() const;

  const char* data_;
  size_t size_;
  uint32_t restart_offset_;     // Offset in data_ of restart array
  bool owned_;                  // Block owns data_[]
  Interpolator interpolate_;

  // No copying allowed
  Block(const Block&);
  void operator=(const Block&);

  class Iter;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_H_

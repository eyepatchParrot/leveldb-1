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
        // Expects the contents to already be initialized so it can use the scan functions
        using Index = int64_t;
        Interpolator(double first, double last, int64_t last_index)
                : first(first), width_range(last_index > 0 ? (double)(last_index) / (double)(last - first) : 0.)
                { assert(width_range >= 0.); }

        const double first;
        const double width_range;

        Index operator()(const double target, const Index mid, const double mid_value) {
                return  mid + (Index)((target - mid_value) * width_range);
        }

        Index operator()(const double target) {
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

  const char* data_;
  size_t size_;
  uint32_t restart_offset_;     // Offset in data_ of restart array
  bool owned_;                  // Block owns data_[]

  // No copying allowed
  Block(const Block&);
  void operator=(const Block&);

  class Iter;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_H_

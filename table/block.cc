// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Decodes the blocks generated by block_builder.cc.

#include "table/block.h"

#include <vector>
#include <algorithm>
#include "leveldb/comparator.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/logging.h"
#include "db/dbformat.h"

namespace leveldb {

Block::~Block() {
  if (owned_) {
    delete[] data_;
  }
}

// Helper routine: decode the next block entry starting at "p",
// storing the number of shared key bytes, non_shared key bytes,
// and the length of the value in "*shared", "*non_shared", and
// "*value_length", respectively.  Will not dereference past "limit".
//
// If any errors are detected, returns nullptr.  Otherwise, returns a
// pointer to the key delta (just past the three decoded values).
static inline const char* DecodeEntry(const char* p, const char* limit,
                                      uint32_t* shared,
                                      uint32_t* non_shared,
                                      uint32_t* value_length) {
  if (limit - p < 3) return nullptr;
  *shared = reinterpret_cast<const unsigned char*>(p)[0];
  *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
  *value_length = reinterpret_cast<const unsigned char*>(p)[2];
  if ((*shared | *non_shared | *value_length) < 128) {
    // Fast path: all three values are encoded in one byte each
    p += 3;
  } else {
    if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
    if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
    if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) return nullptr;
  }

  if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
    return nullptr;
  }
  return p;
}


  uint32_t GetRestartPoint2(const char * data, uint32_t restarts, uint32_t index) {
    //assert(index < num_restarts);
    return DecodeFixed32(data + restarts + index * sizeof(uint32_t));
    }
  // SeekToRestartPoint doesn't do any parsing, so it can't know how long the
  // shared bit is
  Slice SliceAtRestartPoint(const char * data, uint32_t restarts, uint32_t offset) {
      uint32_t region_offset = GetRestartPoint2(data, restarts, offset);
      uint32_t shared, non_shared, value_length;
      const char* key_ptr = DecodeEntry(data + region_offset,
                                        data + restarts,
                                        &shared, &non_shared, &value_length);
      if (key_ptr == nullptr || (shared != 0)) {
        // TODO figure out how to include CorruptionError();
        return Slice();
      }
      return Slice(key_ptr, non_shared);
  }



inline uint32_t Block::NumRestarts() const {
  assert(size_ >= sizeof(uint32_t));
  return DecodeFixed32(data_ + size_ - sizeof(uint32_t));
}

uint32_t RestartOffset(uint32_t size, uint32_t num_restarts) {
	return size - (1 + num_restarts) * sizeof(uint32_t);
}

Slice Block::Front() const {
	return SliceAtRestartPoint(data_, restart_offset_, 0);
}

Slice Block::Back() const {
	return SliceAtRestartPoint(data_, restart_offset_, NumRestarts()-1);
}

Block::Block(const BlockContents& contents)
    : data_(contents.data.data()),
      size_(contents.data.size()),
      restart_offset_(RestartOffset(size_, NumRestarts())),
      owned_(contents.heap_allocated),
      interpolate_(Front(), Back(), NumRestarts()-1)
      {
  if (size_ < sizeof(uint32_t)) {
    size_ = 0;  // Error marker
  } else {
    size_t max_restarts_allowed = (size_-sizeof(uint32_t)) / sizeof(uint32_t);
    if (NumRestarts() > max_restarts_allowed) {
      // The size is too small for NumRestarts()
      size_ = 0;
    } else {
      restart_offset_ = size_ - (1 + NumRestarts()) * sizeof(uint32_t);
    }
  }
}


class Block::Iter : public Iterator {
 private:
  const Comparator* const comparator_;
  const char* const data_;      // underlying block contents
  uint32_t const restarts_;     // Offset of restart array (list of fixed32)
  uint32_t const num_restarts_; // Number of uint32_t entries in restart array

  // current_ is offset in data_ of current entry.  >= restarts_ if !Valid
  uint32_t current_;
  uint32_t restart_index_;  // Index of restart block in which current_ falls
  Interpolator interpolate_;

  std::string key_;
  Slice value_;
  Status status_;


  inline int Compare(const Slice& a, const Slice& b) const {
    return comparator_->Compare(a, b);
  }

  // Return the offset in data_ just past the end of the current entry.
  inline uint32_t NextEntryOffset() const {
    return (value_.data() + value_.size()) - data_;
  }

  uint32_t GetRestartPoint(uint32_t index) {
    return GetRestartPoint2(data_, restarts_, index);
  }

  void SeekToRestartPoint(uint32_t index) {
    key_.clear();
    restart_index_ = index;
    // current_ will be fixed by ParseNextKey();

    // ParseNextKey() starts at the end of value_, so set value_ accordingly
    uint32_t offset = GetRestartPoint(index);
    value_ = Slice(data_ + offset, 0);
  }

 public:
  Iter(const Comparator* comparator,
       const char* data,
       uint32_t restarts,
       uint32_t num_restarts,
       Interpolator interpolate)
      : comparator_(comparator),
        data_(data),
        restarts_(restarts),
        num_restarts_(num_restarts),
        current_(restarts_),
        restart_index_(num_restarts_),
        interpolate_(interpolate) {
    assert(num_restarts_ > 0);
  }

  virtual bool Valid() const { return current_ < restarts_; }
  virtual Status status() const { return status_; }
  virtual Slice key() const {
    assert(Valid());
    return key_;
  }
  virtual Slice value() const {
    assert(Valid());
    return value_;
  }

  virtual void Next() {
    assert(Valid());
    ParseNextKey();
  }

  virtual void Prev() {
    assert(Valid());

    // Scan backwards to a restart point before current_
    const uint32_t original = current_;
    while (GetRestartPoint(restart_index_) >= original) {
      if (restart_index_ == 0) {
        // No more entries
        current_ = restarts_;
        restart_index_ = num_restarts_;
        return;
      }
      restart_index_--;
    }

    SeekToRestartPoint(restart_index_);
    do {
      // Loop until end of current entry hits the start of original entry
    } while (ParseNextKey() && NextEntryOffset() < original);
  }

#define DO_SIP
#ifdef DO_SIP


  int SIP(const Slice& target) {
    // Search restart array to find the last restart point
    // with a key < target
    const int guard_size = 8;
    using Index = int64_t;
    using Key = double;

    auto cmp = Slice(target.data(), interpolate_.shared.size()).compare(interpolate_.shared);
    if (cmp < 0) return 0;
    if (cmp > 0) return num_restarts_ - 1;
    //if (Compare(target, interpolate_.left) < 0) {
    //	return 0;
    //} else if (Compare(target, interpolate_.right) > 0) {
    //	return num_restarts_ - 1;
    //}


    Key x = interpolate_.ApproxKey(target);
    Index left = 0, right = num_restarts_ - 1, next = interpolate_(x);

    if (next > right) return right;
    if (next < left) return left;

    assert(next > -1000);

    // TODO try left == next as break condition,
    // but have to be guaranteed restart block?
    for (int i = 1;; i++) {
            if (left == right) {
	    	return left;
	    }
            // TODO consider using seek since we'll need to seek at the end anyway
            Slice next_slice = SliceAtRestartPoint(data_, restarts_, next);
            if (!status_.ok())
                    return 1 << 30;

            double next_key = interpolate_.ApproxKey(next_slice);
            if (next_key < x) {
                    assert(Compare(next_slice, target) < 0);
                    left = next;
            } else if (next_key > x) {
                    assert(Compare(next_slice, target) > 0);
                    right = next-1;
            } else {
                    for (right = next; right > left && Compare(SliceAtRestartPoint(data_, restarts_, right),  target) >= 0; right--) ;
                    return right;
            }
            if (left >= right) {
                    // If left == right, then it doesn't matter which one we choose.
                    // If left > right, then we want the smaller one
                    assert(left - right <= 1);
                    assert(left < num_restarts_);
                    return left;
            }
            //assert(left < right);
            assert(left >= 0);
            assert(right < num_restarts_);
            next = interpolate_(x, next, next_key);
            if (next + guard_size >= right) {
                    // reverse linear search
                    for (; right > left && Compare(SliceAtRestartPoint(data_, restarts_, right),  target) >= 0; right--) ;
                    return right;
            } else if (next - guard_size <= left) {
                    for (; left <= right && Compare(SliceAtRestartPoint(data_, restarts_, left), target) < 0; left++) ;
                    return left > 0 ? left-1 : 0;
            }
            assert(next >= left);
            assert(next <= right);
    }
  }

  virtual void Seek(const Slice& target) {
    // Linear search (within restart block) for first key >= target
    int index = SIP(target);
    if (!status_.ok()) return;
    SeekToRestartPoint(index);
    while (true) {
	    if (!ParseNextKey()) {
		    return;
	    }
	    if (Compare(key_, target) >= 0) {
		    return;
	    }
    }
  }
#else
  virtual void Seek(const Slice& target) {
    // Binary search in restart array to find the last restart point
    // with a key < target
    uint32_t left = 0;
    uint32_t right = num_restarts_ - 1;
    while (left < right) {
      uint32_t mid = (left + right + 1) / 2;
      uint32_t region_offset = GetRestartPoint(mid);
      uint32_t shared, non_shared, value_length;
      const char* key_ptr = DecodeEntry(data_ + region_offset,
                                        data_ + restarts_,
                                        &shared, &non_shared, &value_length);
      if (key_ptr == NULL || (shared != 0)) {
        CorruptionError();
        return;
      }
      Slice mid_key(key_ptr, non_shared);
      if (Compare(mid_key, target) < 0) {
        // Key at "mid" is smaller than "target".  Therefore all
        // blocks before "mid" are uninteresting.
        left = mid;
      } else {
        // Key at "mid" is >= "target".  Therefore all blocks at or
        // after "mid" are uninteresting.
        right = mid - 1;
      }
    }

    // Linear search (within restart block) for first key >= target
    SeekToRestartPoint(left);
    while (true) {
      if (!ParseNextKey()) {
        return;
      }
      if (Compare(key_, target) >= 0) {
        return;
      }
    }
  }
#endif

  virtual void SeekToFirst() {
    SeekToRestartPoint(0);
    ParseNextKey();
  }

  virtual void SeekToLast() {
    SeekToRestartPoint(num_restarts_ - 1);
    while (ParseNextKey() && NextEntryOffset() < restarts_) {
      // Keep skipping
    }
  }

 private:
  void CorruptionError() {
    current_ = restarts_;
    restart_index_ = num_restarts_;
    status_ = Status::Corruption("bad entry in block");
    key_.clear();
    value_.clear();
  }

  bool ParseNextKey() {
    current_ = NextEntryOffset();
    const char* p = data_ + current_;
    const char* limit = data_ + restarts_;  // Restarts come right after data
    if (p >= limit) {
      // No more entries to return.  Mark as invalid.
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return false;
    }

    // Decode next entry
    uint32_t shared, non_shared, value_length;
    p = DecodeEntry(p, limit, &shared, &non_shared, &value_length);
    if (p == nullptr || key_.size() < shared) {
      CorruptionError();
      return false;
    } else {
      key_.resize(shared);
      key_.append(p, non_shared);
      value_ = Slice(p + non_shared, value_length);
      while (restart_index_ + 1 < num_restarts_ &&
             GetRestartPoint(restart_index_ + 1) < current_) {
        ++restart_index_;
      }
      return true;
    }
  }
};

Iterator* Block::NewIterator(const Comparator* cmp) {
  if (size_ < sizeof(uint32_t)) {
    return NewErrorIterator(Status::Corruption("bad block contents"));
  }
  const uint32_t num_restarts = NumRestarts();
  if (num_restarts == 0) {
    return NewEmptyIterator();
  } else {
    return new Iter(cmp, data_, restart_offset_, num_restarts, interpolate_);
  }
}

}  // namespace leveldb

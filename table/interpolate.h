#ifndef STORAGE_LEVELDB_TABLE_INTERPOLATE_H_
#define STORAGE_LEVELDB_TABLE_INTERPOLATE_H_

#include "leveldb/slice.h"

namespace leveldb {

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

        double WidthRange(uint32_t width, Slice right, double first) {
          return width > 1 ? (double)(width) / (double)(ApproxKey(right) - first) : 0.;
        }

        // Expects the contents to already be initialized so it can use the scan functions
        using Index = int64_t;
        Interpolator(Slice left, Slice right, uint32_t width) :
                shared(Slice(left.data(), CountShared(left, right))),
                first(ApproxKey(left)),
                width_range(WidthRange(width, right, first))
                { assert(width_range >= 0.); assert(width_range < 1e9); }
        Interpolator(Slice shared, double first, double width_range) :
                shared(shared),
                first(first),
                width_range(width_range)
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
}

#endif

// Copyright (c) 2012-2013, the scalloc Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#ifndef SCALLOC_BLOCK_HEADER_H_
#define SCALLOC_BLOCK_HEADER_H_

#include "assert.h"
#include "common.h"
#include "freelist.h"
#include "log.h"
#include "size_classes.h"

enum BlockType {
  kUndef,
  kSlab,
  kLargeObject,
  kNumHeaders
};


struct ActiveOwner {
  union {
    struct {
      bool active : 1;
      uint64_t owner : 63;
    };
    uint64_t raw;
  };

  inline ActiveOwner() {}

  inline ActiveOwner(const bool active, const uint64_t owner) {
    Reset(active, owner);
  }

  inline void Reset(const bool active, const uint64_t owner) {
    this->active = active;
    this->owner = owner;
  }
};


class Header {
 public:
  BlockType type;
};


class SpanHeader : public Header {
 public:
  static inline SpanHeader* GetFromObject(void* p) {
    return reinterpret_cast<SpanHeader*>
        (reinterpret_cast<uintptr_t>(p) & kVirtualSpanMask);
  }

  // read-only properties

  //struct {
  size_t size_class;
  size_t max_num_blocks;
  size_t remote_flist;
  size_t flist_aligned_blocksize_offset;
  //} cache_aligned;

  // mostly read properties

  cache_aligned ActiveOwner aowner;

  // thread-local read/write properties

  //struct {
  uint64_t in_use;
  Freelist flist;
  //} cache_aligned;

  inline void Reset(const size_t size_class, const size_t remote_flist) {
    this->type = kSlab;
    this->size_class = size_class;
    this->remote_flist = remote_flist;
    this->in_use = 0;
    this->flist_aligned_blocksize_offset =
        (reinterpret_cast<uintptr_t>(this) + sizeof(SpanHeader))
            % scalloc::ClassToSize[size_class];
  }

  // The utilization of the span in percent.
  inline size_t Utilization() {
    return 100 - ((this->flist.Size() * 100) / this->max_num_blocks);
  }
} cache_aligned;


class LargeObjectHeader : public Header {
 public:
  static inline LargeObjectHeader* GetFromObject(void* p) {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(p);
    uintptr_t page_ptr = ptr & ~(kPageSize - 1);
    
    if (UNLIKELY(ptr == page_ptr)) {
      page_ptr -= kPageSize;
      if (reinterpret_cast<LargeObjectHeader*>(page_ptr)->fwd != NULL) {
        page_ptr = reinterpret_cast<uintptr_t>(
            reinterpret_cast<LargeObjectHeader*>(page_ptr)->fwd);
      }
    }
    
    Header* bh = reinterpret_cast<Header*>(page_ptr);
    if (UNLIKELY(bh->type != kLargeObject)) {
      Fatal("Calling LargeObjectHeader::GetFromObject on kSlab type. "
            "type: %d, ptr: %p, page_ptr: %p",
            bh->type, p, reinterpret_cast<void*>(page_ptr));
    }
    return reinterpret_cast<LargeObjectHeader*>(bh);
  }
  size_t size;
  LargeObjectHeader* fwd;

  inline void Reset(size_t size) {
    this->type = kLargeObject;
    this->size = size;
    this->fwd = NULL;
  }
} cache_aligned;

#endif  // SCALLOC_BLOCK_HEADER_H_

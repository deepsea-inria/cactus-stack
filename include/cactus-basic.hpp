/*
 * Copyright (c) 2017 Deepsea
 *
 * This software may be modified and distributed under
 * the terms of the MIT license.  See the LICENSE file
 * for details.
 *
 */

#include <stdlib.h>
#include <atomic>
#include <assert.h>

#ifndef _CACTUS_STACK_BASIC_H_
#define _CACTUS_STACK_BASIC_H_

namespace cactus_stack {
  namespace basic {
    
    namespace {
      
      /*------------------------------*/
      /* Forward declarations */
      
      struct frame_header_struct;
      
      /* Forward declarations */
      /*------------------------------*/
      
      /*------------------------------*/
      /* Stack chunk */
      
      static constexpr
      int lg_K = 12;
      
      // size in bytes of a chunk
      static constexpr
      int K = 1 << lg_K;
      
      using chunk_header_type = struct {
        std::atomic<int> refcount;
        struct frame_header_struct* sp;
        struct frame_header_struct* lp;
      };
      
      using chunk_type = struct {
        chunk_header_type hdr;
        char frames[K - sizeof(chunk_header_type)];
      };
      
      static inline
      void* aligned_alloc(size_t alignment, size_t size) {
        void* p;
        assert(alignment % sizeof(void*) == 0);
        if (posix_memalign(&p, alignment, size)) {
          p = nullptr;
        }
        return p;
      }
      
      static inline
      chunk_type* create_chunk(struct frame_header_struct* sp,
                               struct frame_header_struct* lp) {
        chunk_type* c = (chunk_type*)aligned_alloc(K, K);
        new (c) chunk_type;
        c->hdr.refcount.store(1);
        c->hdr.sp = sp;
        c->hdr.lp = lp;
        return c;
      }
      
      template <class T>
      chunk_type* chunk_of(T* p) {
        uintptr_t v = (uintptr_t)(((char*)p) - 1);
        v = v & ~(K - 1);
        return (chunk_type*)v;
      }
      
      static inline
      char* chunk_data(chunk_type* c) {
        return &(c->frames[0]);
      }
      
      static inline
      void incr_refcount(chunk_type* c) {
        c->hdr.refcount++;
      }
      
      static inline
      void decr_refcount(chunk_type* c) {
        assert(c->hdr.refcount.load() >= 1);
        if (--c->hdr.refcount == 0) {
          free(c);
        }
      }
      
      /* Stack chunk */
      /*------------------------------*/
      
      /*------------------------------*/
      /* Frame */
      
      
      
      /* Frame */
      /*------------------------------*/
      
    } // end namespace
    
    /*------------------------------*/
    /* Stack */
    
        
    /* Stack */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

#endif /*! _CACTUS_STACK_BASIC_H_ */

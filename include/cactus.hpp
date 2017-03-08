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

#ifndef _ENCORE_CACTUS_H_
#define _ENCORE_CACTUS_H_

namespace encore {
  namespace cactus {
    
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
      
      using call_link_type = enum {
        Call_link_async, Call_link_sync
      };
      
      using loop_link_type = enum {
        Loop_link_child, Loop_link_none
      };
      
      using shared_frame_type = enum {
        Shared_frame_direct, Shared_frame_indirect
      };
      
      using frame_header_ext_type = struct {
        call_link_type clt;
        loop_link_type llt;
        shared_frame_type sft;
        struct frame_header_struct* pred;
        struct frame_header_struct* succ;
      };
      
      using frame_header_type = struct frame_header_struct {
        struct frame_header_struct* pred;
        frame_header_ext_type ext;
      };
      
      template <class T=char>
      T* frame_data(frame_header_type* p) {
        char* r = (char*)p;
        if (r != nullptr) {
          /* later: need to change sizeof(...) calculation if
           * we switch to using variable-length frame-headers
           */
          r += sizeof(frame_header_type);
        }
        return (T*)r;
      }
      
      /* Frame */
      /*------------------------------*/
      
    } // end namespace
    
    /*------------------------------*/
    /* Stack */
    
    using parent_link_type = enum {
      Parent_link_async, Parent_link_sync
    };
    
    using stack_type = struct {
      frame_header_type* fp, * sp, * lp;
      frame_header_type* mhd, * mtl;
    };
    
    stack_type create_stack() {
      return {
        .fp = nullptr, .sp = nullptr, .lp = nullptr,
        .mhd = nullptr, .mtl = nullptr
      };
    }
    
    template <class Private_frame, class Shared_frame>
    stack_type create_stack(Private_frame* p_f, Shared_frame* p_s) {
      stack_type s = create_stack();
      assert(false); // todo
      return s;
    }
    
    bool empty(stack_type s) {
      return s.fp == nullptr;
    }
    
    template <class Frame>
    Frame* peek_back(stack_type s) {
      assert(false); // todo
      return nullptr;
    }
    
    template <class Frame1, class Frame2>
    std::pair<Frame1*, Frame2*> peek_mark(stack_type s) {
      assert(false); // todo
      return std::make_pair(nullptr, nullptr);
    }
    
    template <class Frame, class ...Args>
    stack_type push_back(stack_type s, parent_link_type ty, Args... args) {
      stack_type t = s;
      auto b = sizeof(frame_header_type) + sizeof(Frame);
      t.fp = s.sp;
      t.sp = (frame_header_type*)((char*)t.fp + b);
      if (t.sp >= t.lp) {
        chunk_type* c = create_chunk(s.sp, s.lp);
        t.fp = (frame_header_type*)chunk_data(c);
        t.sp = (frame_header_type*)((char*)t.fp + b);
        t.lp = (frame_header_type*)((char*)c + K);
      }
      new (frame_data<Frame>(t.fp)) Frame(args...);
      frame_header_ext_type ext;
      ext.sft = Shared_frame_direct;
      if (ty == Parent_link_async) {
        ext.clt = Call_link_async;
        ext.pred = s.mtl;
        ext.succ = nullptr;
        if (s.mtl != nullptr) {
          s.mtl->ext.succ = t.fp;
        }
        t.mtl = t.fp;
        if (t.mhd == nullptr) {
          t.mhd = t.mtl;
        }
      } else if (ty == Parent_link_sync) {
        ext.clt = Call_link_sync;
      } else {
        assert(false); // impossible
      }
      t.fp->pred = s.fp;
      ext.llt = (! empty(s) && frame_data<Frame>(s.fp)->p.nb_iters() >= 2
                 ? Loop_link_child : Loop_link_none);
      t.fp->ext = ext;
      return t;
    }
    
    template <class Frame>
    stack_type pop_back(stack_type s) {
      stack_type t = s;
      frame_data<Frame>(s.fp)->~Frame();
      if (s.fp->ext.clt == Call_link_async) {
        if (s.fp->ext.pred == nullptr) {
          t.mhd = nullptr;
        } else {
          s.fp->ext.pred->ext.succ = nullptr;
        }
        t.mtl = s.fp->ext.pred;
      }
      t.fp = s.fp->pred;
      chunk_type* c_fp = chunk_of(s.fp);
      if (chunk_of(t.fp) == c_fp) {
        t.sp = s.fp;
        t.lp = s.lp;
      } else {
        t.sp = c_fp->hdr.sp;
        t.lp = c_fp->hdr.lp;
        decr_refcount(c_fp);
      }
      return t;
    }
    
    std::pair<stack_type, stack_type> fork_mark(stack_type s) {
      stack_type s1 = s;
      stack_type s2 = create_stack();
      if (s.mhd == nullptr) {
        return std::make_pair(s1, s2);
      }
      frame_header_type* p_f1, * p_f2;
      if (s.mhd->pred == nullptr) {
        if (s.mhd->ext.succ == nullptr) {
          return std::make_pair(s1, s2);
        } else {
          p_f2 = s.mhd->ext.succ;
          s.mhd->ext.succ = nullptr;
        }
      } else {
        p_f2 = s.mhd;
        s1.mhd = nullptr;
      }
      p_f1 = p_f2->pred;
      s1.fp = p_f1;
      chunk_type* c_sp = chunk_of(s.sp);
      if (c_sp == chunk_of(p_f1)) {
        s1.sp = p_f2;
        incr_refcount(c_sp);
      } else {
        s1.sp = nullptr;
      }
      s1.lp = s1.sp;
      s1.mtl = s1.mhd;
      s2 = s;
      s2.mhd = p_f2;
      p_f2->pred = nullptr;
      if (p_f2->ext.clt == Call_link_async) {
        p_f2->ext.pred = nullptr;
      }
      return std::make_pair(s1, s2);
    }
    
    std::pair<stack_type, stack_type> split_mark(stack_type s) {
      assert(false); // todo
      return std::make_pair(create_stack(), create_stack());
    }
    
    /* Stack */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

#endif /*! _ENCORE_CACTUS_H_ */

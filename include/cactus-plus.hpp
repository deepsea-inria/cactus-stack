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

#ifndef _CACTUS_STACK_PLUS_H_
#define _CACTUS_STACK_PLUS_H_

#ifndef CACTUS_STACK_BASIC_LG_K
#define CACTUS_STACK_BASIC_LG_K 12
#endif

namespace cactus_stack {
  namespace plus {
    
    namespace {
      
      /*------------------------------*/
      /* Forward declarations */
      
      struct frame_header_struct;
      
      /* Forward declarations */
      /*------------------------------*/
      
      /*------------------------------*/
      /* Stack chunk */
      
      static constexpr
      int lg_K = CACTUS_STACK_BASIC_LG_K;
      
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
    
    bool empty(stack_type s) {
      return s.fp == nullptr;
    }
    
    template <class Read_fn>
    void peek_back(stack_type s, const Read_fn& read_fn) {
      assert(! empty(s));
      read_fn(s.fp->ext.sft, frame_data(s.fp));
    }
    
    template <class Read_fn>
    void peek_mark(stack_type s, const Read_fn& read_fn) {
      assert(s.mhd != nullptr);
      read_fn(s.mhd->ext.sft, s.mhd->ext.clt, s.mhd->pred, frame_data(s.mhd));
    }
    
    using parent_link_type = enum {
      Parent_link_async, Parent_link_sync
    };
    
    template <class Is_splittable_fn>
    stack_type update_back(stack_type s, const Is_splittable_fn& is_splittable_fn) {
      stack_type t = s;
      if (t.mtl == nullptr) {
        return t;
      }
      bool is_async_mtl = (t.mtl->ext.clt == Call_link_async);
      bool is_splittable_mtl = is_splittable_fn(frame_data(t.mtl));
      bool is_loop_child_mtl = (t.mtl->ext.llt == Loop_link_child);
      if (is_async_mtl || is_splittable_mtl || is_loop_child_mtl) {
        return t;
      }
      frame_header_type* pred = t.mtl->ext.pred;
      t.mtl = pred;
      if (pred != nullptr) {
        pred->ext.succ = nullptr;
      }
      if (s.mtl == s.mhd) {
        t.mhd = pred;
      }
      return t;
    }
    
    template <class Is_splittable_fn>
    stack_type update_front(stack_type s, const Is_splittable_fn& is_splittable_fn) {
      stack_type t = s;
      if (t.mhd == nullptr) {
        return t;
      }
      bool is_async_mhd = (t.mhd->ext.clt == Call_link_async);
      bool is_splittable_mhd = is_splittable_fn(frame_data(t.mhd));
      if (is_async_mhd || is_splittable_mhd) {
        return t;
      }
      frame_header_type* succ = t.mhd->ext.succ;
      t.mhd = succ;
      if (succ != nullptr) {
        succ->ext.pred = nullptr;
      }
      if (s.mtl == s.mhd) {
        t.mtl = succ;
      }
      if (t.mhd != nullptr) {
        t.mhd->ext.llt = Loop_link_none;
      }
      return t;
    }
    
    template <int frame_szb, class Initialize_fn, class Is_splittable_fn>
    stack_type push_back(stack_type s,
                         parent_link_type ty,
                         const Initialize_fn& initialize_fn,
                         const Is_splittable_fn& is_splittable_fn) {
      stack_type t = s;
      auto b = sizeof(frame_header_type) + frame_szb;
      assert(b + sizeof(chunk_header_type) <= K);
      t.fp = s.sp;
      t.sp = (frame_header_type*)((char*)t.fp + b);
      if (t.sp >= t.lp) {
        chunk_type* c = create_chunk(s.sp, s.lp);
        t.fp = (frame_header_type*)chunk_data(c);
        t.sp = (frame_header_type*)((char*)t.fp + b);
        t.lp = (frame_header_type*)((char*)c + K);
      }
      initialize_fn(frame_data(t.fp));
      frame_header_ext_type ext;
      bool is_async_tfp = (ty == Parent_link_async);
      bool is_splittable_tfp = is_splittable_fn(frame_data(t.fp));
      frame_header_type* pred = s.fp;
      bool is_loop_child_tfp = ((pred != nullptr) && is_splittable_fn(frame_data(pred)));
      if (is_async_tfp || is_splittable_tfp || is_loop_child_tfp) {
        ext.pred = s.mtl;
        if (s.mtl != nullptr) {
          s.mtl->ext.succ = t.fp;
        }
        t.mtl = t.fp;
        if (t.mhd == nullptr) {
          t.mhd = t.mtl;
        }
      } else {
        ext.pred = nullptr;
      }
      ext.sft = Shared_frame_direct;
      ext.clt = (is_async_tfp ? Call_link_async : Call_link_sync);
      ext.llt = (is_loop_child_tfp ? Loop_link_child : Loop_link_none);
      ext.succ = nullptr;
      t.fp->pred = pred;
      t.fp->ext = ext;
      return t;
    }
    
    template <class Is_splittable_fn, class Destruct_fn>
    stack_type pop_back(stack_type s,
                        const Is_splittable_fn& is_splittable_fn,
                        const Destruct_fn& destruct_fn) {
      stack_type t = s;
      destruct_fn(frame_data(s.fp));
      bool is_splittable_sfp = is_splittable_fn(frame_data(s.fp));
      bool is_async_sfp = (s.fp->ext.clt == Call_link_async);
      bool is_loop_child_sfp = (s.fp->ext.llt == Loop_link_child);
      if (is_async_sfp || is_splittable_sfp || is_loop_child_sfp) {
        frame_header_type* pred = s.fp->ext.pred;
        if (pred == nullptr) {
          t.mhd = nullptr;
        } else {
          pred->ext.succ = nullptr;
        }
        t.mtl = pred;
      }
      t.fp = s.fp->pred;
      chunk_type* cfp = chunk_of(s.fp);
      if (chunk_of(t.fp) == cfp) {
        t.sp = s.fp;
        t.lp = s.lp;
      } else {
        t.sp = cfp->hdr.sp;
        t.lp = cfp->hdr.lp;
        decr_refcount(cfp);
      }
      return update_back(t, is_splittable_fn);
    }
    
    template <class Is_splittable_fn>
    std::pair<stack_type, stack_type> fork_mark(stack_type s,
                                                const Is_splittable_fn& is_splittable_fn) {
      stack_type s1 = s;
      stack_type s2 = create_stack();
      if (s.mhd == nullptr) {
        return std::make_pair(s1, s2);
      }
      frame_header_type* pf1, * pf2;
      if (s.mhd->pred == nullptr) {
        pf2 = s.mhd->ext.succ;
        if (pf2 == nullptr) {
          return std::make_pair(s1, s2);
        } else {
          s.mhd->ext.pred = nullptr;
          s1.mhd = s.mhd;
        }
      } else {
        pf2 = s.mhd;
        s1.mhd = nullptr;
      }
      pf1 = pf2->pred;
      s1.fp = pf1;
      chunk_type* cf1 = chunk_of(pf1);
      if (cf1 == chunk_of(pf2)) {
        incr_refcount(cf1);
      }
      if (chunk_of(s.sp) == cf1) {
        s1.sp = pf2;
      } else {
        s1.sp = nullptr;
      }
      s1.lp = s1.sp;
      s1.mtl = s1.mhd;
      s2 = s;
      s2.mhd = pf2;
      pf2->pred = nullptr;
      pf2->ext.pred = nullptr;
      s1 = update_back(s1, is_splittable_fn);
      s2 = update_front(s2, is_splittable_fn);
      return std::make_pair(s1, s2);
    }
    
    template <class Is_splittable_fn>
    std::pair<stack_type, stack_type> split_mark(stack_type s,
                                                 const Is_splittable_fn& is_splittable_fn) {
      stack_type s1 = s;
      stack_type s2 = create_stack();
      frame_header_type* pf = s.mhd;
      if (pf == nullptr) {
        return std::make_pair(s1, s2);
      }
      frame_header_type* pg = pf->ext.succ;
      if (pg == nullptr) {
        return std::make_pair(s1, s2);
      }
      pf->ext.succ = nullptr;
      pg->ext.pred = nullptr;
      pg->pred = nullptr;
      s1.fp = pf;
      s1.sp = nullptr;
      s1.lp = nullptr;
      s1.mtl = pf;
      pf->ext.llt = Loop_link_none;
      s2 = s;
      s2.mhd = pg;
      if (s.mhd == s.mtl) {
        s2.mtl = s2.mhd;
      }
      s1 = update_back(s1, is_splittable_fn);
      s2 = update_front(s2, is_splittable_fn);
      return std::make_pair(s1, s2);
    }
    
    template <int frame_szb, class Initialize_fn, class Is_splittable_fn>
    stack_type create_stack(const Initialize_fn& initialize_fn,
                            const Is_splittable_fn& is_splittable_fn) {
      stack_type s = create_stack();
      s = push_back<frame_szb>(s, Parent_link_sync, initialize_fn, is_splittable_fn);
      s.fp->ext.sft = Shared_frame_indirect;
      return s;
    }
    
    /* Stack */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

#endif /*! _CACTUS_STACK_PLUS_H_ */

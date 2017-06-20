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
      
#ifndef CACTUS_STACK_BASIC_LG_K
#define CACTUS_STACK_BASIC_LG_K 12
#endif
      
      static constexpr
      int lg_K = CACTUS_STACK_BASIC_LG_K;
      
#undef CACTUS_STACK_BASIC_LG_K
      
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
    
    bool empty(stack_type s) {
      return s.fp == nullptr;
    }
    
    bool empty_mark(stack_type s) {
      return s.mhd == nullptr;
    }
    
    namespace {
      
      template <class Is_splittable_fn>
      bool is_mark_frame(frame_header_type* fp, const Is_splittable_fn& is_splittable_fn) {
        assert(fp != nullptr);
        if (fp->ext.clt == Call_link_async) {
          return true;
        }
        if (fp->ext.llt == Loop_link_child) {
          return true;
        }
        if (is_splittable_fn(frame_data(fp))) {
          return true;
        }
        return false;
      }
      
      stack_type push_mark_back(stack_type s, frame_header_type* fp) {
        stack_type t = s;
        fp->ext.pred = t.mtl;
        if (t.mtl != nullptr) {
          t.mtl->ext.succ = t.fp;
        }
        t.mtl = t.fp;
        if (t.mhd == nullptr) {
          t.mhd = t.mtl;
        }
        return t;
      }
      
      template <class Is_splittable_fn>
      stack_type try_push_mark_back(stack_type s, frame_header_type* fp, const Is_splittable_fn& is_splittable_fn) {
        stack_type t = s;
        if (s.mtl == fp) {
          return t;
        }
        if (is_mark_frame(fp, is_splittable_fn)) {
          t = push_mark_back(t, fp);
        }
        return t;
      }
      
      stack_type pop_mark_back(stack_type s) {
        assert(! empty_mark(s));
        stack_type t = s;
        frame_header_type* succ = t.mtl;
        frame_header_type* pred = succ->ext.pred;
        if (pred == nullptr) {
          t.mhd = nullptr;
        } else {
          pred->ext.succ = nullptr;
          succ->ext.pred = nullptr;
        }
        t.mtl = pred;
        return t;
      }
      
      template <class Is_splittable_fn>
      stack_type try_pop_mark_back(stack_type s, const Is_splittable_fn& is_splittable_fn) {
        stack_type t = s;
        if (empty_mark(t)) {
          return t;
        }
        if (! is_mark_frame(s.mtl, is_splittable_fn)) {
          t = pop_mark_back(t);
        }
        return t;
      }
      
      stack_type pop_mark_front(stack_type s) {
        assert(! empty_mark(s));
        stack_type t = s;
        frame_header_type* pred = t.mhd;
        frame_header_type* succ = pred->ext.succ;
        if (succ == nullptr) {
          t.mtl = nullptr;
        } else {
          succ->ext.pred = nullptr;
          pred->ext.succ = nullptr;
        }
        t.mhd = succ;
        return t;
      }
      
      template <class Is_splittable_fn>
      stack_type try_pop_mark_front(stack_type s, const Is_splittable_fn& is_splittable_fn) {
        stack_type t = s;
        if (empty_mark(t)) {
          return t;
        }
        if (! is_mark_frame(t.mhd, is_splittable_fn)) {
          t = pop_mark_front(t);
        }
        return t;
      }
      
      template <class Is_splittable_fn>
      stack_type collect_mark_stack(stack_type s, const Is_splittable_fn& is_splittable_fn) {
        stack_type t = s;
        frame_header_type* mhd = s.mhd;
        while (mhd != nullptr) {
          if (is_splittable_fn(frame_data(mhd))) {
            break;
          }
          if ((mhd->ext.clt == Call_link_async) && (mhd->pred != nullptr)) {
            break;
          }
          // delete mhd from mark stack
          auto succ = mhd->ext.succ;
          if (succ != nullptr) {
            succ->ext.pred = nullptr;
            mhd->ext.succ = nullptr;
          }
          mhd = succ;
        }
        if (mhd == nullptr) {
          t.mtl = mhd;
        }
        t.mhd = mhd;
        return t;
      }
      
    } // end namespace
    
    template <class Is_splittable_fn>
    stack_type update_mark_stack(stack_type s, const Is_splittable_fn& is_splittable_fn) {
      stack_type t = s;
      if (empty(t)) {
        return t;
      }
      t = try_pop_mark_back(t, is_splittable_fn);
      t = try_pop_mark_front(t, is_splittable_fn);
      t = collect_mark_stack(t, is_splittable_fn);
      if (s.mtl == t.fp) {
        return t;
      }
      if (is_splittable_fn(frame_data(t.fp))) {
        t = push_mark_back(t, t.fp);
      }
      return t;
    }
    
    stack_type create_stack() {
      return {
        .fp = nullptr, .sp = nullptr, .lp = nullptr,
        .mhd = nullptr, .mtl = nullptr
      };
    }
    
    template <class Read_fn>
    void peek_back(stack_type s, const Read_fn& read_fn) {
      assert(! empty(s));
      read_fn(s.fp->ext.sft, s.fp->ext.clt, frame_data(s.fp));
    }
    
    template <class Read_fn>
    void peek_mark(stack_type s, const Read_fn& read_fn) {
      assert(! empty_mark(s));
      auto sft = s.mhd->ext.sft;
      auto clt = s.mhd->ext.clt;
      auto _ar = frame_data(s.mhd);
      auto pred = s.mhd->pred;
      auto pred_sft = (pred == nullptr) ? Shared_frame_direct : pred->ext.sft;
      auto _pred_ar = (pred == nullptr) ? nullptr : frame_data(pred);
      read_fn(sft, clt, _ar, pred_sft, _pred_ar);
    }
    
    using parent_link_type = enum {
      Parent_link_async, Parent_link_sync
    };
    
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
      frame_header_type* pred = s.fp;
      frame_header_ext_type ext;
      ext.pred = nullptr;
      ext.sft = Shared_frame_direct;
      ext.clt = ((ty == Parent_link_async) ? Call_link_async : Call_link_sync);
      ext.llt = (((pred != nullptr) && is_splittable_fn(frame_data(pred))) ? Loop_link_child : Loop_link_none);
      ext.succ = nullptr;
      t.fp->pred = pred;
      t.fp->ext = ext;
      t = try_push_mark_back(t, t.fp, is_splittable_fn);
      return t;
    }
    
    template <class Destruct_fn>
    stack_type pop_back(stack_type s, const Destruct_fn& destruct_fn) {
      stack_type t = s;
      destruct_fn(frame_data(s.fp), s.fp->ext.sft);
      if (t.mtl == t.fp) {
        t = pop_mark_back(t);
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
      return t;
    }
    
    template <class Is_splittable_fn>
    std::pair<stack_type, stack_type> fork_mark(stack_type s, const Is_splittable_fn& is_splittable_fn) {
      stack_type s1 = s;
      stack_type s2 = create_stack();
      if (empty_mark(s)) {
        return std::make_pair(s1, s2);
      }
      frame_header_type* pf2;
      if (s.mhd->pred == nullptr) {
        pf2 = s.mhd->ext.succ;
        if (pf2 == nullptr) {
          return std::make_pair(s1, s2);
        }
      } else {
        pf2 = s.mhd;
        s1.mhd = nullptr;
      }
      frame_header_type* pf1 = pf2->pred;
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
      pf1->ext.succ = nullptr;
      pf2->pred = nullptr;
      pf2->ext.pred = nullptr;
      s1 = try_pop_mark_back(s1, is_splittable_fn);
      s2 = try_pop_mark_front(s2, is_splittable_fn);
      return std::make_pair(s1, s2);
    }
    
    template <class Is_splittable_fn>
    std::pair<stack_type, stack_type> split_mark(stack_type s, const Is_splittable_fn& is_splittable_fn) {
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
      assert(pg->ext.llt == Loop_link_child);
      assert(pg->pred == pf);
      pf->ext.succ = nullptr;
      pg->ext.pred = nullptr;
      pg->pred = nullptr;
      s1.fp = pf;
      s1.sp = nullptr;
      s1.lp = nullptr;
      s1.mtl = pf;
      s2 = s;
      pg->ext.llt = Loop_link_none;
      s2.mhd = pg;
      chunk_type* cpf = chunk_of(pf);
      if (cpf == chunk_of(pg)) {
        incr_refcount(cpf);
      }
      s1 = try_pop_mark_back(s1, is_splittable_fn);
      s2 = try_pop_mark_front(s2, is_splittable_fn);
      return std::make_pair(s1, s2);
    }
    
    template <int frame_szb, class Initialize_fn, class Is_splittable_fn>
    stack_type create_stack(parent_link_type ty,
                            const Initialize_fn& initialize_fn,
                            const Is_splittable_fn& is_splittable_fn) {
      stack_type s = create_stack();
      s = push_back<frame_szb>(s, ty, initialize_fn, is_splittable_fn);
      s.fp->ext.sft = Shared_frame_indirect;
      s = try_push_mark_back(s, s.fp, is_splittable_fn);
      return s;
    }
    
    /* Stack */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

#endif /*! _CACTUS_STACK_PLUS_H_ */

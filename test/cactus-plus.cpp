/*
 * Copyright (c) 2017 Deepsea
 *
 * This software may be modified and distributed under
 * the terms of the MIT license.  See the LICENSE file
 * for details.
 *
 */

#include <iostream>
#include <memory>
#include <deque>

#include "cactus-plus.hpp"

namespace cactus_stack {
  namespace plus {
    
    /*------------------------------*/
    /* Frame */
    
    class shared_frame {
    public:
      int v;
      parent_link_type plt;
    };
    
    class private_frame {
    public:
      int v;
      int nb_iters() {
        return 1;
      }
      private_frame split(shared_frame*, float r) {
        private_frame p;
        return p;
      }
    };
    
    class frame {
    public:
      private_frame p;
      shared_frame s;
    };
    
    using machine_stack_type = stack_type;
    using reference_stack_type = std::deque<frame>;

    /* Frame */
    /*------------------------------*/
    
    bool is_splittable(private_frame& p) {
      return p.nb_iters() >= 2;
    }
    
    bool is_marked(frame& f) {
      return (f.s.plt == Parent_link_async) || is_splittable(f.p);
    }
    
    using fork_result_tag = enum {
      Fork_result0, Fork_result1, Fork_result_S, Fork_result_N
    };
    
    using fork_result_type = struct {
      fork_result_tag tag;
      struct {
        reference_stack_type s;
        frame f;
      } fork_1;
      struct {
        reference_stack_type s1, s2;
        frame f;
      } fork_s;
      struct {
        reference_stack_type s1, s2;
        frame f1, f2;
      } fork_n;
    };
    
    fork_result_type fork_mark(reference_stack_type& s) {
      fork_result_type r;
      constexpr int not_found = -1;
      size_t k = not_found;
      auto nb_frames = s.size();
      for (size_t i = 0; i < nb_frames; i++) {
        if (is_marked(s[i])) {
          k = i;
          break;
        }
      }
      if (k == not_found) {
        r.tag = Fork_result0;
        return r;
      }
      auto oldest_frame_is_k = [&] {
        return k == 0;
      };
      reference_stack_type t1(s.begin(), s.begin() + k);
      reference_stack_type t2(s.begin() + k, s.end());
      if (oldest_frame_is_k() && ! is_splittable(s[k].p)) {
        r.tag = Fork_result1;
        r.fork_1.s = t2;
        r.fork_1.s.pop_front();
        r.fork_1.f = t2.front();
        return r;
      }
      if (s[k].s.plt == Parent_link_async) {
        assert(! oldest_frame_is_k());
        r.tag = Fork_result_N;
        r.fork_n.s1 = t1;
        r.fork_n.s2 = t2;
        r.fork_n.s1.pop_back();
        r.fork_n.s2.pop_front();
        r.fork_n.f1 = t1.back();
        r.fork_n.f2 = t2.front();
        return r;
      }
      assert(is_splittable(s[k].p));
      r.tag = Fork_result_S;
      r.fork_s.s1 = t1;
      r.fork_s.f = t2.front();
      r.fork_s.s2 = t2;
      r.fork_s.s2.pop_front();
      return r;
    }

    /*------------------------------*/
    /* Trace */
    
    using trace_tag_type = enum {
      Trace_push_back, Trace_pop_back,
      Trace_fork_mark,
      Trace_nil
    };
    
    struct trace_struct {
      trace_tag_type tag;
      struct {
        frame f;
        std::unique_ptr<struct trace_struct> k;
      } push_back;
      struct {
        std::unique_ptr<struct trace_struct> k;
      } pop_back;
      struct fork_mark_struct {
        std::unique_ptr<struct trace_struct> k1;
        std::unique_ptr<struct trace_struct> k2;
      } fork_mark;
    };
    
    using trace_type = struct trace_struct;
    
    trace_type mk_push_back(frame f) {
      trace_type t;
      t.tag = Trace_push_back;
      t.push_back.f = f;
      return t;
    }
    
    trace_type mk_pop_back() {
      trace_type t;
      t.tag = Trace_pop_back;
      return t;
    }
    
    trace_type mk_fork_mark() {
      trace_type t;
      t.tag = Trace_fork_mark;
      return t;
    }
    
    using thread_config_type = struct {
      trace_type t;
      reference_stack_type rs; // reference stack
      machine_stack_type ms;
    };
    
    using machine_tag_type = enum {
      Machine_fork_mark, Machine_thread, Machine_stuck
    };
    
    using machine_config_type = struct machine_config_struct {
      machine_tag_type tag;
      struct {
        std::unique_ptr<struct machine_config_struct> m1;
        std::unique_ptr<struct machine_config_struct> m2;
      } fork_mark;
      thread_config_type thread;
    };
    
    template <class Coin_flip>
    machine_config_type step(const Coin_flip& coin_flip, machine_config_type& m) {
      machine_config_type n;
      n.tag = Machine_stuck;
      switch (m.tag) {
        case Machine_fork_mark: {
          n.tag = Machine_fork_mark;
          if (coin_flip()) {
            *n.fork_mark.m1 = step(coin_flip, *m.fork_mark.m1);
            n.fork_mark.m2.swap(m.fork_mark.m2);
          } else {
            n.fork_mark.m1.swap(m.fork_mark.m1);
            *n.fork_mark.m2 = step(coin_flip, *m.fork_mark.m2);
          }
          break;
        }
        case Machine_thread: {
          thread_config_type& tc_m = m.thread;
          thread_config_type& tc_n = n.thread;
          switch (tc_m.t.tag) {
            case Trace_fork_mark: {
              n.tag = Machine_fork_mark;
              auto mp = fork_mark(tc_m.ms);
              auto rp = fork_mark(tc_m.rs);
              reference_stack_type rs1, rs2;
              if (rp.tag == Fork_result_N) {
                rs1 = rp.fork_n.s1;
                rs1.push_back(rp.fork_n.f1);
                rs2 = rp.fork_n.s2;
                rs2.push_back(rp.fork_n.f2);
              } else {
                rs1 = tc_m.rs;
              }
              n.fork_mark.m1.reset(new machine_config_type);
              n.fork_mark.m2.reset(new machine_config_type);
              machine_config_type& m1 = *n.fork_mark.m1;
              machine_config_type& m2 = *n.fork_mark.m2;
              m1.tag = Machine_thread;
              m1.thread.t = std::move(*tc_m.t.fork_mark.k1);
              m1.thread.ms = mp.first;
              m1.thread.rs = rs1;
              m2.tag = Machine_thread;
              m2.thread.t = std::move(*tc_m.t.fork_mark.k2);
              m2.thread.ms = mp.second;
              m2.thread.rs = rs2;
              break;
            }
            case Trace_push_back: {
              frame f = tc_m.t.push_back.f;
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.push_back(f);
              tc_n.ms = push_back<frame>(tc_m.ms, f.s.plt, f);
              break;
            }
            case Trace_pop_back: {
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.pop_back();
              tc_n.ms = pop_back<frame>(tc_m.ms);
              break;
            }
            default: {
              assert(false); // todo
            }
          }
          break;
        }
        case Machine_stuck: {
          break;
        }
        default: {
          break;
        }
      }
      return n;
    }
    
    /* Trace */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Checks for invariants */
    
    using frame_addr_rng = std::pair<frame_header_type*, frame_header_type*>;
    
    std::deque<frame_addr_rng> frame_addrs(frame_header_type* fp,
                                           frame_header_type* sp) {
      std::deque<frame_addr_rng> r;
      if (fp == nullptr) {
        // nothing to do
      } else if (fp != nullptr && chunk_of(fp) == chunk_of(fp->pred)) {
        r = frame_addrs(fp->pred, fp);
        r.push_back(std::make_pair(fp, sp));
      } else {
        r = frame_addrs(fp->pred, chunk_of(fp)->hdr.sp);
        r.push_back(std::make_pair(fp, sp));
      }
      return r;
    }
    
    std::deque<frame_header_type*> all_frame_ptrs(frame_header_type* fp,
                                                  frame_header_type* sp) {
      std::deque<frame_header_type*> r;
      for (auto v : frame_addrs(fp, sp)) {
        r.push_back(v.first);
      }
      return r;
    }
    
    reference_stack_type all_frames(frame_header_type* fp, frame_header_type* sp) {
      reference_stack_type r;
      for (auto p : all_frame_ptrs(fp, sp)) {
        frame f = *(frame_data<frame>(p));
        r.push_back(f);
      }
      return r;
    }
    
    std::deque<frame_header_type*> marked_frame_ptrs_fwd(frame_header_type* mhd) {
      std::deque<frame_header_type*> r;
      if (mhd != nullptr) {
        r = marked_frame_ptrs_fwd(mhd->ext.succ);
        if (is_splittable(frame_data<frame>(mhd)->p) ||
            (mhd->ext.clt == Call_link_async)) {
          r.push_front(mhd);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frame_fwd(frame_header_type* mhd) {
      reference_stack_type r;
      for (auto p : marked_frame_ptrs_fwd(mhd)) {
        r.push_back(*(frame_data<frame>(p)));
      }
      return r;
    }
    
    std::deque<frame_header_type*> marked_frame_ptrs_bkw(frame_header_type* mtl) {
      std::deque<frame_header_type*> r;
      if (mtl != nullptr) {
        r = marked_frame_ptrs_fwd(mtl->ext.pred);
        if (is_splittable(frame_data<frame>(mtl)->p) ||
            (mtl->ext.clt == Call_link_async)) {
          r.push_back(mtl);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frame_bkw(frame_header_type* mtl) {
      reference_stack_type r;
      for (auto p : marked_frame_ptrs_bkw(mtl)) {
        r.push_back(*(frame_data<frame>(p)));
      }
      return r;
    }
    
    bool equals(frame f1, frame f2) {
      return (f1.p.v == f2.p.v) && (f1.s.v == f2.s.v);
    }
    
    bool equals(reference_stack_type& fs1, reference_stack_type& fs2) {
      auto n = fs1.size();
      if (n != fs2.size()) {
        return false;
      }
      for (size_t i = 0; i < n; i++) {
        if (! equals(fs1[i], fs2[i])) {
          return false;
        }
      }
      return true;
    }
    
    template <class F, class T>
    std::deque<T> filter(const F& f, std::deque<T>& d) {
      std::deque<T> r;
      for (auto v : d) {
        if (f(v)) {
          r.push_back(v);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frames_of(reference_stack_type& d) {
      return filter([] (frame& f) {
        return is_marked(f);
      }, d);
    }
    
    bool is_consistent(thread_config_type& tc) {
      bool r = true;
      auto af_r = tc.rs;
      auto af_m = all_frames(tc.ms.fp, tc.ms.sp);
      r = r && equals(af_r, af_m);
      auto mf_r = marked_frames_of(tc.rs);
      auto mff_m = marked_frame_fwd(tc.ms.mhd);
      r = r && equals(mf_r, mff_m);
      auto mfb_m = marked_frame_bkw(tc.ms.mtl);
      r = r && equals(mf_r, mfb_m);
      return r;
    }
    
    bool is_consistent(machine_config_type& mc) {
      bool r = true;
      switch (mc.tag) {
        case Machine_fork_mark: {
          r = r && is_consistent(*mc.fork_mark.m1);
          r = r && is_consistent(*mc.fork_mark.m2);
          break;
        }
        case Machine_thread: {
          r = is_consistent(mc.thread);
          break;
        }
        case Machine_stuck: {
          break;
        }
        default: {
          assert(false);
          r = false;
        }
      }
      return r;
    }
    
    /* Checks for invariants */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

int main(int argc, const char * argv[]) {
  cactus_stack::plus::machine_config_type tt;
  std::cout << "Hello, World!\n";
  return 0;
}

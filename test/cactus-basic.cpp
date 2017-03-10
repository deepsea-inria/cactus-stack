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
#include <cmath>
#include <string>
#include <time.h>

#include "cactus-basic.hpp"

namespace cactus_stack {
  namespace basic {
    
    /*------------------------------*/
    /* Frame */
    
    class frame {
    public:
      int v;
      parent_link_type plt;
    };
    
    frame gen_random_frame() {
      frame f;
      f.v = rand() % 1024;
      f.plt = ((rand() % 2 == 0) ? Parent_link_sync : Parent_link_async);
      return f;
    }
    
    /* Frame */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Trace */
    
    using machine_stack_type = stack_type;
    using reference_stack_type = std::deque<frame>;
    
    using fork_result_tag = enum {
      Fork_result_none, Fork_result_some
    };
    
    using fork_result_type = struct {
      fork_result_tag tag;
      reference_stack_type s1, s2;
      frame f1, f2;
    };
    
    bool is_marked(frame& f) {
      return f.plt == Parent_link_async;
    }
    
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
        r.tag = Fork_result_none;
        return r;
      }
      reference_stack_type t1(s.begin(), s.begin() + k);
      reference_stack_type t2(s.begin() + k, s.end());
      r.tag = Fork_result_some;
      r.s1 = t1;
      r.s2 = t2;
      r.s1.pop_back();
      r.s2.pop_front();
      r.f1 = t1.back();
      r.f2 = t2.front();
      return r;
    }
    
    using trace_tag_type = enum {
      Trace_push_back, Trace_pop_back,
      Trace_fork_mark,
      Trace_nil
    };
    
    struct trace_struct {
      trace_tag_type tag;
      struct {
        frame f;
        std::shared_ptr<struct trace_struct> k;
      } push_back;
      struct {
        std::shared_ptr<struct trace_struct> k;
      } pop_back;
      struct fork_mark_struct {
        std::shared_ptr<struct trace_struct> k1;
        std::shared_ptr<struct trace_struct> k2;
      } fork_mark;
    };
    
    using trace_type = struct trace_struct;
    
    std::shared_ptr<trace_type> mk_push_back(frame f) {
      trace_type t;
      t.tag = Trace_push_back;
      t.push_back.f = f;
      return std::make_shared<trace_type>(t);
    }
    
    std::shared_ptr<trace_type> mk_pop_back() {
      trace_type t;
      t.tag = Trace_pop_back;
      return std::make_shared<trace_type>(t);
    }
    
    std::shared_ptr<trace_type> mk_fork_mark() {
      trace_type t;
      t.tag = Trace_fork_mark;
      return std::make_shared<trace_type>(t);
    }
    
    void print_trace(std::shared_ptr<trace_type> t, const std::string& prefix, bool is_tail) {
      std::cout << (prefix + (is_tail ? "└── " : "├── "));
      switch (t->tag) {
        case Trace_fork_mark: {
          std::cout << "*" << std::endl;
          if (t->fork_mark.k1) {
            print_trace(t->fork_mark.k1, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (t->fork_mark.k2) {
            print_trace(t->fork_mark.k2, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_push_back: {
          auto plt = t->push_back.f.plt;
          auto plt_s = (plt == Parent_link_async ? "A" : "S");
          std::cout << "+[" << t->push_back.f.v << "](" << plt_s << ")" << std::endl;
          if (t->push_back.k) {
            print_trace(t->push_back.k, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_pop_back: {
          std::cout << "-[]" << std::endl;
          if (t->pop_back.k) {
            print_trace(t->pop_back.k, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_nil: {
          break;
        }
        default: {
          assert(false);
        }
      }
    }
    
    void print_trace(std::shared_ptr<trace_type> t) {
      print_trace(t, "", true);
    }
    
    int position_of_top_async(const std::deque<frame>& fs) {
      static constexpr
      int not_found = -1;
      int i = 0;
      for (auto& f : fs) {
        if (f.plt == Parent_link_async) {
          return i;
        }
        i++;
      }
      return not_found;
    }
    
    std::shared_ptr<trace_type> gen_random_trace(const std::deque<frame>& prefix, int d) {
      std::shared_ptr<trace_type> r;
      auto n_p = prefix.size();
      if (n_p == 0) {
        return r;
      }
      int posn = position_of_top_async(prefix);
      if ((posn > 0) && ((rand() % (1 << d)) < 3)) {
        r = mk_fork_mark();
        std::deque<frame> prefix1(prefix.begin(), prefix.begin() + posn);
        std::deque<frame> prefix2(prefix.begin() + posn, prefix.end());
        r->fork_mark.k1 = gen_random_trace(prefix1, d + 1);
        r->fork_mark.k2 = gen_random_trace(prefix2, d + 1);
      } else if ((rand() % (2 + (1 << n_p))) < 3) {
        auto f = gen_random_frame();
        std::deque<frame> prefix2(prefix);
        prefix2.push_back(f);
        r = mk_push_back(f);
        r->push_back.k = gen_random_trace(prefix2, d);
      } else {
        std::deque<frame> prefix2(prefix);
        prefix2.pop_back();
        r = mk_pop_back();
        r->pop_back.k = gen_random_trace(prefix2, d);
      }
      return r;
    }
    
    std::shared_ptr<trace_type> gen_random_trace() {
      std::deque<frame> prefix;
      auto f = gen_random_frame();
      prefix.push_back(f);
      auto r = mk_push_back(f);
      r->push_back.k = gen_random_trace(prefix, 1);
      return r;
    }
    
    using thread_config_type = struct {
      std::shared_ptr<trace_type> t;
      reference_stack_type rs; // reference stack
      machine_stack_type ms;
    };
    
    thread_config_type mk_thread_config(std::shared_ptr<trace_type> t) {
      thread_config_type tc;
      tc.t = t;
      tc.ms = create_stack();
      return tc;
    }
    
    void print_thread_config(thread_config_type& tc) {
      std::cout << "TC" << std::endl;
    }
    
    using machine_tag_type = enum {
      Machine_fork_mark, Machine_thread, Machine_stuck
    };
    
    using machine_config_type = struct machine_config_struct {
      machine_tag_type tag;
      struct {
        std::shared_ptr<struct machine_config_struct> m1;
        std::shared_ptr<struct machine_config_struct> m2;
      } fork_mark;
      thread_config_type thread;
    };
    
    machine_config_type mk_mc_fork_mark(std::shared_ptr<machine_config_type> m1,
                                        std::shared_ptr<machine_config_type> m2) {
      machine_config_type m;
      m.tag = Machine_fork_mark;
      m.fork_mark.m1 = m1;
      m.fork_mark.m2 = m2;
      return m;
    }
    
    machine_config_type mk_mc_thread(thread_config_type tc) {
      machine_config_type m;
      m.tag = Machine_thread;
      m.thread = tc;
      return m;
    }
    
    thread_config_type gen_random_thread_config() {
      return mk_thread_config(gen_random_trace());
    }
    
    void print_machine_config(machine_config_type& mc,
                              const std::string& prefix,
                              bool is_tail) {
      std::cout << (prefix + (is_tail ? "└── " : "├── "));
      switch (mc.tag) {
        case Machine_fork_mark: {
          std::cout << "*" << std::endl;
          if (mc.fork_mark.m1) {
            print_machine_config(*mc.fork_mark.m1, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (mc.fork_mark.m2) {
            print_machine_config(*mc.fork_mark.m1, prefix + (is_tail ?"    " : "│   "), true);
          }
          break;
        }
        case Machine_thread: {
          print_thread_config(mc.thread);
          break;
        }
        case Machine_stuck: {
          std::cout << "Stuck" << std::endl;
          break;
        }
        default: {
          assert(false);
        }
      }
    }
    
    void print_machine_config(machine_config_type& mc) {
      print_machine_config(mc, "", true);
    }
    
    machine_config_type gen_random_machine_config() {
      return mk_mc_thread(gen_random_thread_config());
    }
    
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
          switch (tc_m.t->tag) {
            case Trace_fork_mark: {
              n.tag = Machine_fork_mark;
              auto mp = fork_mark(tc_m.ms);
              auto rp = fork_mark(tc_m.rs);
              reference_stack_type rs1, rs2;
              if (rp.tag == Fork_result_some) {
                rs1 = rp.s1;
                rs1.push_back(rp.f1);
                rs2 = rp.s2;
                rs2.push_back(rp.f2);
              } else {
                rs1 = tc_m.rs;
              }
              n.fork_mark.m1.reset(new machine_config_type);
              n.fork_mark.m2.reset(new machine_config_type);
              machine_config_type& m1 = *n.fork_mark.m1;
              machine_config_type& m2 = *n.fork_mark.m2;
              m1.tag = Machine_thread;
              m1.thread.t = tc_m.t->fork_mark.k1;
              m1.thread.ms = mp.first;
              m1.thread.rs = rs1;
              m2.tag = Machine_thread;
              m2.thread.t = tc_m.t->fork_mark.k2;
              m2.thread.ms = mp.second;
              m2.thread.rs = rs2;
              break;
            }
            case Trace_push_back: {
              frame f = tc_m.t->push_back.f;
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.push_back(f);
              tc_n.ms = push_back<sizeof(frame)>(tc_m.ms, f.plt, [&] (char* p) {
                new ((frame*)p) frame(f);
              });
              tc_n.t = tc_m.t->push_back.k;
              break;
            }
            case Trace_pop_back: {
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.pop_back();
              tc_n.ms = pop_back(tc_m.ms, [&] (char* p) {
                ((frame*)(p))->~frame();
              });
              tc_n.t = tc_m.t->pop_back.k;
              break;
            }
            default: {
              assert(false);
            }
          }
          break;
        }
        case Machine_stuck: {
          break;
        }
        default: {
          assert(false);
        }
      }
      return n;
    }
    
    /* Trace */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Stack-frame enumeration */
    
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
    
    reference_stack_type all_frames(frame_header_type* fp,
                                    frame_header_type* sp) {
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
        r.push_front(mhd);
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
        r.push_back(mtl);
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
      return (f1.v == f2.v);
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
    
    /* Stack-frame enumeration */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Predicates */
    
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
    
    void check_consistent(machine_config_type& mc) {
      if (! is_consistent(mc)) {
        std::cout << "Error: inconsistent machine configuration";
        std::cout << "found." << std::endl;
      }
    }
    
    bool is_tail(std::shared_ptr<trace_type>& p_t) {
      return p_t.get() == nullptr;
    }
    
    bool is_tail(thread_config_type& tc) {
      return is_tail(tc.t);
    }
    
    bool is_finished(machine_config_type& mc) {
      bool r = true;
      switch (mc.tag) {
        case Machine_fork_mark: {
          r = r && is_finished(*mc.fork_mark.m1);
          r = r && is_finished(*mc.fork_mark.m2);
          break;
        }
        case Machine_thread: {
          r = is_tail(mc.thread);
          break;
        }
        case Machine_stuck: {
          break;
        }
        default: {
          assert(false);
        }
      }
      return r;
    }
    
    void check() {
      machine_config_type mc = gen_random_machine_config();
      while (! is_finished(mc)) {
        check_consistent(mc);
        mc = step([&] { return rand() % 2 == 0; }, mc);
      }
    }
    
    /* Predicates */
    /*------------------------------*/
    
    void ex1() {
      srand(time(nullptr));
      print_trace(gen_random_trace());
      return;
      frame f;
      f.v = 123;
      auto t2 = mk_push_back(f);
      t2->push_back.k = mk_pop_back();
      auto t3 = mk_fork_mark();
      t3->fork_mark.k1 = t2;
      t3->fork_mark.k2 = t2;
      auto t4 = mk_fork_mark();
      t4->fork_mark.k2 = t3;
      t4->fork_mark.k1 = t3;
      print_trace(t4);
      return;
      machine_config_type mc = mk_mc_thread(mk_thread_config(t2));
      mc = step([&] { return rand() % 2 == 0; }, mc);
      check_consistent(mc);
      mc = step([&] { return rand() % 2 == 0; }, mc);
      check_consistent(mc);
    }
    
  } // end namespace
} // end namespace

int main(int argc, const char * argv[]) {
  cactus_stack::basic::ex1();
//  cactus_stack::basic::check();
  return 0;
}

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
    
    trace_type* mk_push_back(frame f) {
      trace_type* t = new trace_type;
      t->tag = Trace_push_back;
      t->push_back.f = f;
      return t;
    }
    
    trace_type* mk_pop_back() {
      trace_type* t = new trace_type;
      t->tag = Trace_pop_back;
      return t;
    }
    
    trace_type* mk_fork_mark() {
      trace_type* t = new trace_type;
      t->tag = Trace_fork_mark;
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
        std::shared_ptr<struct machine_config_struct> m1;
        std::shared_ptr<struct machine_config_struct> m2;
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
              auto init_fn = [&] (char* p) {
                new ((frame*)p) frame(f);
              };
              tc_n.ms = push_back<sizeof(frame),decltype(init_fn)>(tc_m.ms, f.plt, init_fn);
              break;
            }
            case Trace_pop_back: {
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.pop_back();
              auto destr_fn = [&] (char* p) {
                ((frame*)(p))->~frame();
              };
              tc_n.ms = pop_back(tc_m.ms, destr_fn);
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
    
    void ex1() {
      frame f;
      f.v = 123;
      auto t2 = mk_push_back(f);
      t2->push_back.k.reset(mk_pop_back());
      machine_config_type m;
      step([&] {
        return rand() % 2 == 0;
      }, m);
    }
    
  } // end namespace
} // end namespace

int main(int argc, const char * argv[]) {
  std::cout << "starting\n";
  cactus_stack::basic::ex1();
  std::cout << "finished\n";
  return 0;
}

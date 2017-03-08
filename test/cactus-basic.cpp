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
      return f.s.plt == Parent_link_async;
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
    
    /* Trace */
    /*------------------------------*/
    
  } // end namespace
} // end namespace

int main(int argc, const char * argv[]) {
  std::cout << "Hello, World!\n";
  return 0;
}

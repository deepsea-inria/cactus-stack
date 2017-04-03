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
#include <set>
#include <cmath>
#include <string>
#include <time.h>
#include "quickcheck.hh"

#include "cactus-plus.hpp"

namespace cactus_stack {
  namespace plus {
    
    bool flip_coin() {
      return quickcheck::generateInRange(0, 1);
    }
    
    /*------------------------------*/
    /* Frame */
    
    class shared_frame {
    public:
      size_t v;
      shared_frame* p = nullptr;
      parent_link_type plt;
    };
    
    class private_frame {
    public:
      
      size_t v;
      size_t lo = 0;
      size_t hi = 0;
      
      size_t nb_iters() const {
        return hi - lo;
      }
      
      private_frame split(shared_frame*, size_t nb) {
        private_frame p = *this;
        lo += nb;
        p.hi = lo;
        return p;
      }
      
    };
    
    class frame {
    public:
      private_frame p;
      shared_frame s;
    };
    
    frame gen_random_frame() {
      static constexpr
      int max_val = 1024;
      static constexpr
      int max_nb_iters = 10;
      frame f;
      f.s.plt = (flip_coin() ? Parent_link_sync : Parent_link_async);
      f.s.v = quickcheck::generateInRange(0, max_val);
      f.p.v = quickcheck::generateInRange(0, max_val);
      f.p.hi = quickcheck::generateInRange(0, max_nb_iters);
      return f;
    }
    
    std::ostream& operator<<(std::ostream& out, const frame& f) {
      auto ty_s = (f.s.plt == Parent_link_async) ? "A" : "S";
      auto sv = (f.s.p == nullptr) ? "null" : std::to_string(f.s.p->v);
      out << "{s.v=" << sv << ", p.v=" << f.p.v <<
      ", ty=" << ty_s << ", nb=" << f.p.nb_iters() << "}";
      return out;
    }
    
    template<class A>
    std::ostream& operator<<(std::ostream& out, const std::deque<A>& xs) {
      out << "[";
      for (size_t i = 0; i < xs.size(); ++i)
        if (i == xs.size() - 1)
          out << xs[i];
        else
          out << xs[i] << ", ";
      return out << "]";
    }
    
    /* Frame */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Trace */
    
    using machine_stack_type = stack_type;
    
    using reference_stack_type = std::deque<frame>;
    
    bool is_splittable(const private_frame& p) {
      return p.nb_iters() >= 2;
    }
    
    bool is_marked(const frame& f) {
      return (f.s.plt == Parent_link_async) || is_splittable(f.p);
    }
    
    using fork_result_tag = enum {
      Fork_result_none, Fork_result_loop_split, Fork_result_fork
    };
    
    using fork_result_type = struct {
      fork_result_tag tag;
      struct {
        reference_stack_type s1, s2;
        frame f1, f2;
      } fork;
      struct {
        reference_stack_type s1, s2;
        frame f;
      } loop_split;
    };
    
    std::pair<fork_result_tag, size_t> fork_mark_status(const reference_stack_type& s) {
      static constexpr
      size_t not_found = -1;
      auto find_index = [&] {
        size_t k = not_found;
        auto nb_frames = s.size();
        for (size_t i = 0; i < nb_frames; i++) {
          if (is_marked(s[i])) {
            k = i;
            break;
          }
        }
        return k;
      };
      size_t k = find_index();
      if ((k == not_found) || ((k == 0) && ! is_splittable(s[k].p))) {
        return std::make_pair(Fork_result_none, k);
      }
      if (! (k == 0) && (s[k].s.plt == Parent_link_async)) {
        return std::make_pair(Fork_result_fork, k);
      }
      assert(is_splittable(s[k].p));
      return std::make_pair(Fork_result_loop_split, k);
    }
    
    fork_result_type fork_mark(reference_stack_type& s) {
      fork_result_type r;
      auto _p = fork_mark_status(s);
      r.tag = _p.first;
      if (r.tag == Fork_result_none) {
        return r;
      }
      size_t k = _p.second;
      reference_stack_type s1(s.begin(), s.begin() + k);
      reference_stack_type s2(s.begin() + k, s.end());
      switch (r.tag) {
        case Fork_result_fork: {
          r.tag = Fork_result_fork;
          r.fork.s1 = s1;
          r.fork.s2 = s2;
          r.fork.s1.pop_back();
          r.fork.s2.pop_front();
          r.fork.f1 = s1.back();
          r.fork.f2 = s2.front();
          break;
        }
        case Fork_result_loop_split: {
          r.tag = Fork_result_loop_split;
          r.loop_split.s1 = s1;
          r.loop_split.f = s2.front();
          r.loop_split.s2 = s2;
          r.loop_split.s2.pop_front();
          break;
        }
        default: {
          assert(false);
        }
      }
      return r;
    }
    
    using trace_tag_type = enum {
      Trace_push_back, Trace_pop_back, Trace_fork_mark,
      Trace_split_mark, Trace_nil
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
      struct {
        std::shared_ptr<struct trace_struct> k1;
        std::shared_ptr<struct trace_struct> k2;
      } fork_mark;
      struct {
        std::shared_ptr<struct trace_struct> k11;
        std::shared_ptr<struct trace_struct> k12;
        std::shared_ptr<struct trace_struct> k2;
        std::shared_ptr<struct trace_struct> k;
      } split_mark;
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
    
    std::shared_ptr<trace_type> mk_split_mark() {
      trace_type t;
      t.tag = Trace_split_mark;
      return std::make_shared<trace_type>(t);
    }
    
    void print_trace(std::ostream& out, std::shared_ptr<trace_type> t, const std::string& prefix, bool is_tail) {
      out << (prefix + (is_tail ? "└── " : "├── "));
      switch (t->tag) {
        case Trace_fork_mark: {
          out << "*" << std::endl;
          if (t->fork_mark.k1) {
            print_trace(out, t->fork_mark.k1, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (t->fork_mark.k2) {
            print_trace(out, t->fork_mark.k2, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_split_mark: {
          out << "^" << std::endl;
          if (t->split_mark.k11) {
            print_trace(out, t->split_mark.k11, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (t->split_mark.k12) {
            print_trace(out, t->split_mark.k12, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (t->split_mark.k2) {
            print_trace(out, t->split_mark.k2, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (t->split_mark.k) {
            print_trace(out, t->split_mark.k, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_push_back: {
          auto plt = t->push_back.f.s.plt;
          auto plt_s = (plt == Parent_link_async ? "A" : "S");
          frame& f = t->push_back.f;
          auto sv = (f.s.p != nullptr) ? f.s.p->v : f.s.v;
          out << "+{p.v=" << f.p.v << ", s.v=" << sv << ", ty=" << plt_s << ", nb=" << f.p.nb_iters() << "}" << std::endl;
          if (t->push_back.k) {
            print_trace(out, t->push_back.k, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Trace_pop_back: {
          out << "-{}" << std::endl;
          if (t->pop_back.k) {
            print_trace(out, t->pop_back.k, prefix + (is_tail ? "    " : "│   "), true);
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
    
    void print_trace(std::ostream& out, std::shared_ptr<trace_type> t) {
      print_trace(out, t, "", true);
    }
    
    std::shared_ptr<trace_type> gen_random_trace(const std::deque<frame>& prefix, int d) {
      std::shared_ptr<trace_type> r;
      auto np = prefix.size();
      if (np == 0) {
        return r;
      }
      auto _p = fork_mark_status(prefix);
      fork_result_tag ft = _p.first;
      size_t k = _p.second;
      if ((ft == Fork_result_fork) && (quickcheck::generateInRange(0, d - 1) == 0)) {
        r = mk_fork_mark();
        std::deque<frame> prefix1(prefix.begin(), prefix.begin() + k);
        std::deque<frame> prefix2(prefix.begin() + k, prefix.end());
        r->fork_mark.k1 = gen_random_trace(prefix1, d + 1);
        r->fork_mark.k2 = gen_random_trace(prefix2, d + 1);
      } else if ((ft == Fork_result_loop_split) && (quickcheck::generateInRange(0, d - 1) == 0)) {
        r = mk_split_mark();
        auto pk = ((k + 1 == prefix.size()) ? prefix.end() : prefix.begin() + (k + 1));
        std::deque<frame> prefix1(prefix.begin(), pk);
        std::deque<frame> prefix2(pk, prefix.end());
        r->split_mark.k11 = gen_random_trace({ }, d + 1);
        r->split_mark.k12 = gen_random_trace(prefix2, d + 1);
        r->split_mark.k2 = gen_random_trace({ }, d + 1);
        r->split_mark.k = gen_random_trace(prefix1, d + 1);
      } else if (quickcheck::generateInRange(0, (2 + (1 << np)) - 1) < 3) {
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
      r->push_back.k = gen_random_trace(prefix, 2);
      return r;
    }
    
    using thread_config_type = struct {
      std::shared_ptr<trace_type> t;
      reference_stack_type rs;
      machine_stack_type ms;
    };
    
    thread_config_type mk_thread_config(std::shared_ptr<trace_type> t) {
      thread_config_type tc;
      tc.t = t;
      tc.ms = create_stack();
      return tc;
    }
    
    thread_config_type gen_random_thread_config() {
      return mk_thread_config(gen_random_trace());
    }
    
    using machine_tag_type = enum {
      Machine_fork_mark, Machine_split_mark,
      Machine_thread, Machine_stuck
    };
    
    using machine_config_type = struct machine_config_struct {
      machine_tag_type tag;
      struct {
        std::shared_ptr<struct machine_config_struct> m1;
        std::shared_ptr<struct machine_config_struct> m2;
      } fork_mark;
      struct {
        std::shared_ptr<struct machine_config_struct> m11;
        std::shared_ptr<struct machine_config_struct> m12;
        std::shared_ptr<struct machine_config_struct> m2;
        std::shared_ptr<struct machine_config_struct> k;
      } split_mark;
      thread_config_type thread;
    };
    
    std::shared_ptr<machine_config_type> mk_mc_thread() {
      machine_config_type m;
      m.tag = Machine_thread;
      return std::make_shared<machine_config_type>(m);
    }
    
    std::shared_ptr<machine_config_type> mk_mc_thread(thread_config_type tc) {
      auto m = mk_mc_thread();
      m->thread = tc;
      return m;
    }
    
    void generate(size_t, machine_config_struct& m) {
      new (&m) machine_config_type(*mk_mc_thread(gen_random_thread_config()));
    }
    
    reference_stack_type all_frames(frame_header_type*, frame_header_type*);
    reference_stack_type marked_frames_fwd(frame_header_type*);
    reference_stack_type marked_frames_bkw(frame_header_type*);
    reference_stack_type marked_frames_of(const reference_stack_type&);
    bool equals(const reference_stack_type&, const reference_stack_type&);
    
    void print_stack_consistency_result(std::ostream& out, const thread_config_type& tc) {
      auto ms = all_frames(tc.ms.fp, tc.ms.sp);
      auto ms_mf = marked_frames_fwd(tc.ms.mhd);
      auto ms_mb = marked_frames_bkw(tc.ms.mtl);
      auto rmkd = marked_frames_of(tc.rs);
      if (! equals(tc.rs, ms)) {
        out << "TC-A{rs=" << tc.rs << ", ms=" << ms << "}";
      } else if (! equals(rmkd, ms_mf)) {
        out << "TC-F{rs=" << rmkd << ", ms=" << ms_mf << "}";
      } else if (! equals(rmkd, ms_mb)) {
        out << "TC-B{rs=" << rmkd << ", ms=" << ms_mb << "}";
      } else {
        out << "TC{}";
      }
      out << std::endl;
    }
    
    void print_machine_config(std::ostream& out,
                              const machine_config_type& mc,
                              const std::string& prefix,
                              bool is_tail) {
      out << (prefix + (is_tail ? "└── " : "├── "));
      switch (mc.tag) {
        case Machine_fork_mark: {
          out << "*" << std::endl;
          if (mc.fork_mark.m1) {
            print_machine_config(out, *mc.fork_mark.m1, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (mc.fork_mark.m2) {
            print_machine_config(out, *mc.fork_mark.m2, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Machine_split_mark: {
          out << "^" << std::endl;
          if (mc.split_mark.m11) {
            print_machine_config(out, *mc.split_mark.m11, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (mc.split_mark.m12) {
            print_machine_config(out, *mc.split_mark.m12, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (mc.split_mark.m2) {
            print_machine_config(out, *mc.split_mark.m2, prefix + (is_tail ? "    " : "│   "), false);
          }
          if (mc.split_mark.k) {
            print_machine_config(out, *mc.split_mark.k, prefix + (is_tail ? "    " : "│   "), true);
          }
          break;
        }
        case Machine_thread: {
          print_stack_consistency_result(out, mc.thread);
          break;
        }
        case Machine_stuck: {
          out << "Stuck" << std::endl;
          break;
        }
        default: {
          assert(false);
        }
      }
    }
    
    void print_machine_config(std::ostream& out, const machine_config_type& mc) {
      print_machine_config(out, mc, "", true);
    }
    
    std::ostream& operator<<(std::ostream& out, const struct machine_config_struct& mc) {
      out << std::endl; out << std::endl;
      print_trace(out, mc.thread.t);
      return out;
    }
    
    std::shared_ptr<machine_config_type> gen_random_machine_config() {
      return mk_mc_thread(gen_random_thread_config());
    }
    
    bool is_finished(std::shared_ptr<machine_config_type>);
    
    std::shared_ptr<machine_config_type> step(std::shared_ptr<machine_config_type> m) {
      machine_config_type n;
      n.tag = Machine_stuck;
      switch (m->tag) {
        case Machine_fork_mark: {
          n.tag = Machine_fork_mark;
          if (flip_coin()) {
            n.fork_mark.m1 = step(m->fork_mark.m1);
            n.fork_mark.m2 = m->fork_mark.m2;
          } else {
            n.fork_mark.m1 = m->fork_mark.m1;
            n.fork_mark.m2 = step(m->fork_mark.m2);
          }
          break;
        }
        case Machine_split_mark: {
          n.tag = Machine_split_mark;
          bool finished_m11 = is_finished(m->split_mark.m11);
          bool finished_m12 = is_finished(m->split_mark.m12);
          bool finished_m2 = is_finished(m->split_mark.m2);
          if (finished_m11 && finished_m12 && finished_m2) {
            n.split_mark.m11 = m->split_mark.m11;
            n.split_mark.m12 = m->split_mark.m12;
            n.split_mark.m2 = m->split_mark.m2;
            n.split_mark.k = step(m->split_mark.k);
          } else if (flip_coin()) {
            if (finished_m12) {
              n.split_mark.m11 = step(m->split_mark.m11);
              n.split_mark.m12 = m->split_mark.m12;
            } else {
              n.split_mark.m11 = m->split_mark.m11;
              n.split_mark.m12 = step(m->split_mark.m12);
            }
            n.split_mark.m2 = m->split_mark.m2;
            n.split_mark.k = m->split_mark.k;
          } else {
            n.split_mark.m11 = m->split_mark.m11;
            n.split_mark.m12 = m->split_mark.m12;
            n.split_mark.m2 = step(m->split_mark.m2);
            n.split_mark.k = m->split_mark.k;
          }
          break;
        }
        case Machine_thread: {
          thread_config_type& tc_m = m->thread;
          thread_config_type& tc_n = n.thread;
          if (tc_m.t.get() == nullptr) {
            return m;
          }
          auto is_splittable_fn = [] (char* _fp) {
            frame* fp = (frame*)_fp;
            return is_splittable(fp->p);
          };
          switch (tc_m.t->tag) {
            case Trace_fork_mark: {
              auto fr = fork_mark(tc_m.rs);
              switch (fr.tag) {
                case Fork_result_fork:
                case Fork_result_none: {
                  auto mp = fork_mark(tc_m.ms, is_splittable_fn);
                  reference_stack_type rs1, rs2;
                  n.tag = Machine_fork_mark;
                  if (fr.tag == Fork_result_fork) {
                    rs1 = fr.fork.s1;
                    rs1.push_back(fr.fork.f1);
                    rs2 = fr.fork.s2;
                    rs2.push_front(fr.fork.f2);
                  } else if (fr.tag == Fork_result_none) {
                    rs1 = tc_m.rs;
                  }
                  n.fork_mark.m1 = mk_mc_thread();
                  n.fork_mark.m2 = mk_mc_thread();
                  thread_config_type& n1 = n.fork_mark.m1->thread;
                  thread_config_type& n2 = n.fork_mark.m2->thread;
                  n1.t = tc_m.t->fork_mark.k1;
                  n1.ms = mp.first;
                  n1.rs = rs1;
                  n2.t = tc_m.t->fork_mark.k2;
                  n2.ms = mp.second;
                  n2.rs = rs2;
                  break;
                }
                case Fork_result_loop_split: {
                  assert(false);
                  break;
                }
                default: {
                  assert(false);
                }
              }
              break;
            }
            case Trace_split_mark: {
              auto fr = fork_mark(tc_m.rs);
              switch (fr.tag) {
                case Fork_result_fork: {
                  assert(false);
                  break;
                }
                case Fork_result_loop_split: {
                  n.tag = Machine_split_mark;
                  n.split_mark.m11 = mk_mc_thread();
                  n.split_mark.m12 = mk_mc_thread();
                  n.split_mark.m2 = mk_mc_thread();
                  n.split_mark.k = mk_mc_thread();
                  thread_config_type& tc11 = n.split_mark.m11->thread;
                  thread_config_type& tc12 = n.split_mark.m12->thread;
                  thread_config_type& tc2 = n.split_mark.m2->thread;
                  thread_config_type& k = n.split_mark.k->thread;
                  k.rs = fr.loop_split.s1;
                  k.rs.push_back(fr.loop_split.f);
                  frame& rf = k.rs.back();
                  tc11.rs.push_back(frame(rf));
                  frame& rf11 = tc11.rs.back();
                  rf11.s.p = &(rf.s);
                  rf11.p = rf.p.split(&(rf.s), rf.p.nb_iters());
                  tc12.rs = fr.loop_split.s2;
                  tc2.rs.push_back(frame(rf));
                  frame& rf2 = tc2.rs.back();
                  rf2.s.p = &(rf.s);
                  rf2.p = rf11.p.split(&(rf.s), rf11.p.nb_iters() / 2);
                  auto ss = split_mark(tc_m.ms, is_splittable_fn);
                  stack_type s1 = ss.first;
                  stack_type s2 = ss.second;
                  k.ms = s1;
                  frame* mf = nullptr;
                  shared_frame_type sft;
                  call_link_type clt;
                  peek_back(s1, [&] (shared_frame_type _sft, call_link_type _clt, char* _fp) {
                    mf = (frame*)_fp;
                    sft = _sft;
                    clt = _clt;
                  });
                  parent_link_type plt = ((clt == Call_link_async) ? Parent_link_async : Parent_link_sync);
                  frame* mf11 = nullptr;
                  tc11.ms = create_stack<sizeof(frame)>(plt, [&] (char* _fp) {
                    mf11 = (frame*)_fp;
                    new (mf11) frame;
                    mf11->s.p = &(mf->s);
                    mf11->p = mf->p.split(&(mf11->s), mf->p.nb_iters());
                  }, is_splittable_fn);
                  tc12.ms = s2;
                  tc2.ms = create_stack<sizeof(frame)>(plt, [&] (char* _fp) {
                    frame* mf2 = (frame*)_fp;
                    new (mf2) frame;
                    mf2->s.p = &(mf->s);
                    mf2->p = mf11->p.split(&(mf11->s), mf11->p.nb_iters() / 2);
                  }, is_splittable_fn);
                  tc11.ms = update_marks_fwd(tc11.ms, is_splittable_fn);
                  tc12.ms = update_marks_fwd(tc12.ms, is_splittable_fn);
                  k.ms = update_marks_fwd(k.ms, is_splittable_fn);
                  tc11.t = tc_m.t->split_mark.k11;
                  tc12.t = tc_m.t->split_mark.k12;
                  tc2.t = tc_m.t->split_mark.k2;
                  k.t = tc_m.t->split_mark.k;
                  break;
                }
                case Fork_result_none: {
                  break;
                }
                default: {
                  assert(false);
                }
              }
              break;
            }
            case Trace_push_back: {
              frame f = tc_m.t->push_back.f;
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.push_back(f);
              tc_n.ms = push_back<sizeof(frame)>(tc_m.ms, f.s.plt, [&] (char* p) {
                new ((frame*)p) frame(f);
              }, [&] (char* _fp) {
                frame* fp = (frame*)_fp;
                return is_splittable(fp->p);
              });
              tc_n.t = tc_m.t->push_back.k;
              break;
            }
            case Trace_pop_back: {
              n.tag = Machine_thread;
              tc_n.rs = tc_m.rs;
              tc_n.rs.pop_back();
              tc_n.ms = pop_back(tc_m.ms, [&] (char* p) {
                return is_splittable(((frame*)p)->p);
              }, [&] (char* p) {
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
          assert(false);
          break;
        }
        default: {
          assert(false);
        }
      }
      return std::make_shared<machine_config_type>(n);
    }
    
    /* Trace */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Stack-frame extraction */
    
    using frame_addr_rng = std::pair<frame_header_type*, frame_header_type*>;
    
    std::deque<frame_addr_rng> frame_addrs(frame_header_type* fp,
                                           frame_header_type* sp) {
      std::deque<frame_addr_rng> r;
      if (fp == nullptr) {
        // nothing to do
      } else if ((fp != nullptr) && (chunk_of(fp) == chunk_of(fp->pred))) {
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
        if (is_splittable(frame_data<frame>(mhd)->p) ||
            (mhd->ext.clt == Call_link_async)) {
          r.push_front(mhd);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frames_fwd(frame_header_type* mhd) {
      reference_stack_type r;
      for (auto p : marked_frame_ptrs_fwd(mhd)) {
        r.push_back(*(frame_data<frame>(p)));
      }
      return r;
    }
    
    std::deque<frame_header_type*> marked_frame_ptrs_bkw(frame_header_type* mtl) {
      std::deque<frame_header_type*> r;
      if (mtl != nullptr) {
        r = marked_frame_ptrs_bkw(mtl->ext.pred);
        if (is_splittable(frame_data<frame>(mtl)->p) ||
            (mtl->ext.clt == Call_link_async)) {
          r.push_back(mtl);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frames_bkw(frame_header_type* mtl) {
      reference_stack_type r;
      for (auto p : marked_frame_ptrs_bkw(mtl)) {
        r.push_back(*(frame_data<frame>(p)));
      }
      return r;
    }
    
    bool equals(frame f1, frame f2) {
      if (f1.p.v != f2.p.v) {
        return false;
      }
      if (f1.s.p == nullptr) {
        if (f2.s.p != nullptr) {
          return false;
        }
        return (f1.s.v == f2.s.v);
      }
      return f1.s.p->v == f2.s.p->v;
    }
    
    bool equals(const reference_stack_type& fs1,
                const reference_stack_type& fs2) {
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
    std::deque<T> filter(const F& f, const std::deque<T>& d) {
      std::deque<T> r;
      for (auto v : d) {
        if (f(v)) {
          r.push_back(v);
        }
      }
      return r;
    }
    
    reference_stack_type marked_frames_of(const reference_stack_type& d) {
      return filter([] (frame& f) {
        return is_marked(f);
      }, d);
    }
    
    std::set<chunk_type*> chunks_of_stack(stack_type s) {
      std::set<chunk_type*> r;
      for (auto fp : all_frame_ptrs(s.fp, s.sp)) {
        r.insert(chunk_of(fp));
      }
      return r;
    }
    
    size_t refcount_of_stack(stack_type s, chunk_type* c) {
      auto cs = chunks_of_stack(s);
      bool found = (cs.find(c) != cs.end());
      return found ? 1 : 0;
    }
    
    std::deque<stack_type> stacks_of(std::shared_ptr<machine_config_type> mc) {
      std::deque<stack_type> r;
      if (mc.get() == nullptr) {
        return r;
      }
      switch (mc->tag) {
        case Machine_fork_mark: {
          r = stacks_of(mc->fork_mark.m1);
          for (auto s : stacks_of(mc->fork_mark.m2)) {
            r.push_back(s);
          }
          break;
        }
        case Machine_split_mark: {
          r = stacks_of(mc->split_mark.m11);
          for (auto s : stacks_of(mc->split_mark.m12)) {
            r.push_back(s);
          }
          for (auto s : stacks_of(mc->split_mark.m2)) {
            r.push_back(s);
          }
          for (auto s : stacks_of(mc->split_mark.k)) {
            r.push_back(s);
          }
          break;
        }
        case Machine_thread: {
          r.push_back(mc->thread.ms);
          break;
        }
        case Machine_stuck: {
          assert(false);
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
      return r;
    }
    
    size_t refcount_of_machine(std::shared_ptr<machine_config_type> mc, chunk_type* c) {
      size_t r = 0;
      for (auto s : stacks_of(mc)) {
        r += refcount_of_stack(s, c);
      }
      return r;
    }
    
    std::set<chunk_type*> live_chunks_of(std::shared_ptr<machine_config_type> mc) {
      std::set<chunk_type*> r;
      for (auto s : stacks_of(mc)) {
        for (auto fp : all_frame_ptrs(s.fp, s.sp)) {
          r.insert(chunk_of(fp));
        }
      }
      return r;
    }
    
    std::set<std::pair<void*, void*>> addr_ranges_of_alloc_reg(frame_header_type* fp,
                                                               frame_header_type* sp,
                                                               frame_header_type* lp) {
      std::set<std::pair<void*, void*>> r;
      if (fp == nullptr) {
        return r;
      }
      auto ins = [&] {
        if ((sp != nullptr) && (sp != lp)) {
          r.insert(std::make_pair(sp, lp));
        }
      };
      chunk_type* c_fp = chunk_of(fp);
      auto pred = fp->pred;
      chunk_type* c_pred = chunk_of(pred);
      if (c_fp == c_pred) {
        r = addr_ranges_of_alloc_reg(pred, fp, lp);
      } else {
        r = addr_ranges_of_alloc_reg(pred, c_fp->hdr.sp, c_fp->hdr.lp);
      }
      ins();
      return r;
    }
    
    /* Stack-frame extraction */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Predicates */
    
    bool is_consistent(thread_config_type& tc) {
      bool r = true;
      auto af_r = tc.rs;
      auto af_m = all_frames(tc.ms.fp, tc.ms.sp);
      r = r && equals(af_r, af_m);
      auto mf_r = marked_frames_of(tc.rs);
      auto mff_m = marked_frames_fwd(tc.ms.mhd);
      r = r && equals(mf_r, mff_m);
      auto mfb_m = marked_frames_bkw(tc.ms.mtl);
      r = r && equals(mf_r, mfb_m);
      return r;
    }
    
    bool is_consistent(std::shared_ptr<machine_config_type> mc) {
      bool r = true;
      switch (mc->tag) {
        case Machine_fork_mark: {
          r = r && is_consistent(mc->fork_mark.m1);
          r = r && is_consistent(mc->fork_mark.m2);
          break;
        }
        case Machine_thread: {
          r = is_consistent(mc->thread);
          break;
        }
        case Machine_split_mark: {
          r = r && is_consistent(mc->split_mark.m11);
          r = r && is_consistent(mc->split_mark.m12);
          r = r && is_consistent(mc->split_mark.m2);
          r = r && is_consistent(mc->split_mark.k);
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
    
    bool is_tail(thread_config_type& tc) {
      return tc.t.get() == nullptr;
    }
    
    bool is_finished(std::shared_ptr<machine_config_type> mc) {
      bool r = true;
      switch (mc->tag) {
        case Machine_fork_mark: {
          r = r && is_finished(mc->fork_mark.m1);
          r = r && is_finished(mc->fork_mark.m2);
          break;
        }
        case Machine_split_mark: {
          r = r && is_finished(mc->split_mark.m11);
          r = r && is_finished(mc->split_mark.m12);
          r = r && is_finished(mc->split_mark.m2);
          r = r && is_finished(mc->split_mark.k);
          break;
        }
        case Machine_thread: {
          r = is_tail(mc->thread);
          break;
        }
        case Machine_stuck: {
          assert(false);
          break;
        }
        default: {
          assert(false);
        }
      }
      return r;
    }
    
    bool is_memory_safe(std::shared_ptr<machine_config_type> mc) {
      for (auto c : live_chunks_of(mc)) {
        auto true_refcount = refcount_of_machine(mc, c);
        auto actual_refcount = c->hdr.refcount.load();
        if (true_refcount != actual_refcount) {
          return false;
        }
      }
      return true;
    }
    
    bool overlapping_pointer_ranges(void* a1, void* a2, void* b1, void* b2) {
      return (a1 < b2) && (b1 < a2);
    }
    
    bool overlapping_addr_ranges(std::pair<void*, void*> r1, std::pair<void*, void*> r2) {
      return overlapping_pointer_ranges(r1.first, r1.second, r2.first, r2.second);
    }
    
    std::set<std::pair<void*, void*>> frame_addr_ranges(frame_header_type* fp, frame_header_type* sp) {
      std::set<std::pair<void*, void*>> r;
      for (auto p : frame_addrs(fp, sp)) {
        auto q = r.insert(p);
        assert(q.second);
      }
      return r;
    }
    
    template <class T>
    std::set<T> merge(const std::set<T>& s1, const std::set<T>& s2) {
      std::set<T> r = s1;
      for (auto x : s2) {
        r.insert(x);
      }
      return r;
    }
    
    bool is_pairwise_compatible(stack_type s1, stack_type s2) {
      auto rs1 = merge(frame_addr_ranges(s1.fp, s1.sp),
                       addr_ranges_of_alloc_reg(s1.fp, s1.sp, s1.lp));
      auto rs2 = merge(frame_addr_ranges(s2.fp, s2.sp),
                       addr_ranges_of_alloc_reg(s2.fp, s2.sp, s2.lp));
      for (auto r1 : rs1) {
        for (auto r2 : rs2) {
          if (overlapping_addr_ranges(r1, r2)) {
            return false;
          }
        }
      }
      return true;
    }
    
    bool is_pairwise_compatible(std::shared_ptr<machine_config_type> mc) {
      auto ss = stacks_of(mc);
      for (auto i = 0; i < ss.size(); i++) {
        for (auto j = 0; j < ss.size(); j++) {
          if (i != j) {
            if (! is_pairwise_compatible(ss[i], ss[j])) {
              return false;
            }
          }
        }
      }
      return true;
    }
    
    /* Predicates */
    /*------------------------------*/
    
    /*------------------------------*/
    /* Quickcheck properties */
    
    class property_consitent_machine_config
    : public quickcheck::Property<machine_config_type> {
    public:
      
      bool holdsFor(const machine_config_type& _mc) {
        auto mc = std::make_shared<machine_config_type>(_mc);
        while (! is_finished(mc)) {
          if (! is_consistent(mc)) {
            std::cout << "Extraction of stacks from bogus thread configuration:" << std::endl;
            print_machine_config(std::cout, *mc);
            return false;
          }
          mc = step(mc);
        }
        return true;
      }
      
    };
    
    class property_correct_refcounts
    : public quickcheck::Property<machine_config_type> {
    public:
      
      bool holdsFor(const machine_config_type& _mc) {
        auto mc = std::make_shared<machine_config_type>(_mc);
        while (! is_finished(mc)) {
          if (! is_memory_safe(mc)) {
            std::cout << "Extraction of stacks from bogus thread configuration:" << std::endl;
            print_machine_config(std::cout, *mc);
            return false;
          }
          mc = step(mc);
        }
        return true;
      }
      
    };
    
    class property_pairwise_compatible
    : public quickcheck::Property<machine_config_type> {
    public:
      
      bool holdsFor(const machine_config_type& _mc) {
        auto mc = std::make_shared<machine_config_type>(_mc);
        while (! is_finished(mc)) {
          if (! is_pairwise_compatible(mc)) {
            std::cout << "Extraction of stacks from bogus thread configuration:" << std::endl;
            print_machine_config(std::cout, *mc);
            return false;
          }
          mc = step(mc);
        }
        return true;
      }
      
    };
    
    /* Quickcheck properties */
    /*------------------------------*/
    
    void check_consistency(int nb_tests) {
      using prop = property_consitent_machine_config;
      auto msg = "cactus stack is consistent with the spec";
      quickcheck::check<prop>(msg, nb_tests);
    }
    
    void check_refcounts(int nb_tests) {
      using prop = property_correct_refcounts;
      auto msg = "cactus stack has correct refcounts";
      quickcheck::check<prop>(msg, nb_tests);
    }
    
    void check_pairwise_compatible(int nb_tests) {
      using prop = property_pairwise_compatible;
      auto msg = "cactus stack properly separates memory between threads";
      quickcheck::check<prop>(msg, nb_tests);
    }
    
  } // end namespace
} // end namespace

time_t xxx;

int main(int argc, const char * argv[]) {
  xxx = time(nullptr);
  srand(1491220989);
  //srand((unsigned int)xxx);
  int nb_tests = (argc == 2) ? std::stoi(argv[1]) : 1024;
  cactus_stack::plus::check_refcounts(nb_tests);
//  cactus_stack::plus::check_consistency(nb_tests);
  //cactus_stack::plus::check_pairwise_compatible(nb_tests);
  //cactus_stack::plus::check_refcounts(nb_tests);
  return 0;
}

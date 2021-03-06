// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#include "BCP_timeout.hpp"
#include "BCP_node_change.hpp"
#include "BCP_tm.hpp"
#include "BCP_lp.hpp"
#include "BCP_tm_functions.hpp"
#include "BCP_problem_core.hpp"
#include "BCP_solution.hpp"
#include "BCP_tm_user.hpp"

static inline void
BCP_tm_send_process_type_and_params(BCP_tm_prob& p,
 				    BCP_vec<BCP_proc_id*>::const_iterator beg,
				    BCP_vec<BCP_proc_id*>::const_iterator end,
				    BCP_process_t ptype);

static void
BCP_tm_distribute_info_to_new_procs(BCP_tm_prob& p,
				    BCP_vec<BCP_proc_id*>& procs,
				    BCP_process_t ptype);

static void
BCP_tm_change_config(BCP_tm_prob& p, BCP_buffer& buf);

//#############################################################################

void BCP_tm_idle_processes(BCP_tm_prob& p)
{
   p.msg_env->multicast(p.slaves.all, BCP_Msg_FinishedBCP);
}

//#############################################################################

void BCP_tm_stop_processes(BCP_tm_prob& p)
{
   p.msg_env->multicast(p.slaves.all, BCP_Msg_FinishedBCP);
}

//#############################################################################

void BCP_tm_start_processes(BCP_tm_prob& p)
{
   const BCP_string& exe = p.param(BCP_tm_par::ExecutableName);

   p.slaves.all = new BCP_proc_array;
   

   if (p.param(BCP_tm_par::LpProcessNum) > 0) {
      const bool debug = p.param(BCP_tm_par::DebugLpProcesses) != 0;
      const int  num = p.param(BCP_tm_par::LpProcessNum);
      const BCP_vec<BCP_string>& machines = p.param(BCP_tm_par::LpMachines);
      if (machines.size() == 0) {
	 p.slaves.lp = p.msg_env->start_processes(exe, num, debug);
      } else {
	 p.slaves.lp = p.msg_env->start_processes(exe, num, machines, debug);
      }
      p.slaves.all->add_procs(p.slaves.lp->procs().begin(),
			      p.slaves.lp->procs().end());
      p.active_nodes.insert(p.active_nodes.end(), num, 0);
   }

   if (p.param(BCP_tm_par::CgProcessNum) > 0) {
      const bool debug = p.param(BCP_tm_par::DebugCgProcesses) != 0;
      const int  num = p.param(BCP_tm_par::CgProcessNum);
      const BCP_vec<BCP_string>& machines = p.param(BCP_tm_par::CgMachines);
      if (machines.size() == 0) {
	 p.slaves.cg = p.msg_env->start_processes(exe, num, debug);
      } else {
	 p.slaves.cg = p.msg_env->start_processes(exe, num, machines, debug);
      }
      p.slaves.all->add_procs(p.slaves.cg->procs().begin(),
			      p.slaves.cg->procs().end());
   }

   if (p.param(BCP_tm_par::VgProcessNum) > 0) {
      const bool debug = p.param(BCP_tm_par::DebugVgProcesses) != 0;
      const int  num = p.param(BCP_tm_par::VgProcessNum);
      const BCP_vec<BCP_string>& machines = p.param(BCP_tm_par::VgMachines);
      if (machines.size() == 0) {
	 p.slaves.vg = p.msg_env->start_processes(exe, num, debug);
      } else {
	 p.slaves.vg = p.msg_env->start_processes(exe, num, machines, debug);
      }
      p.slaves.all->add_procs(p.slaves.vg->procs().begin(),
			      p.slaves.vg->procs().end());
   }

   if (p.param(BCP_tm_par::CpProcessNum) > 0) {
      const bool debug = p.param(BCP_tm_par::DebugCpProcesses) != 0;
      const int  num = p.param(BCP_tm_par::CpProcessNum);
      const BCP_vec<BCP_string>& machines = p.param(BCP_tm_par::CpMachines);
      if (machines.size() == 0) {
	 p.slaves.cp = p.msg_env->start_processes(exe, num, debug);
      } else {
	 p.slaves.cp = p.msg_env->start_processes(exe, num, machines, debug);
      }
      p.slaves.all->add_procs(p.slaves.cp->procs().begin(),
			      p.slaves.cp->procs().end());
   }

   if (p.param(BCP_tm_par::VpProcessNum) > 0) {
      const bool debug = p.param(BCP_tm_par::DebugVpProcesses) != 0;
      const int  num = p.param(BCP_tm_par::VpProcessNum);
      const BCP_vec<BCP_string>& machines = p.param(BCP_tm_par::VpMachines);
      if (machines.size() == 0) {
	 p.slaves.vp = p.msg_env->start_processes(exe, num, debug);
      } else {
	 p.slaves.vp = p.msg_env->start_processes(exe, num, machines, debug);
      }
      p.slaves.all->add_procs(p.slaves.vp->procs().begin(),
			      p.slaves.vp->procs().end());
   }
}

//#############################################################################

void BCP_tm_notify_about_new_phase(BCP_tm_prob& p)
{
   p.msg_env->multicast(p.slaves.all, BCP_Msg_NextPhaseStarts);
}

//#############################################################################

static inline void
BCP_tm_send_process_type_and_params(BCP_tm_prob& p,
				    BCP_vec<BCP_proc_id*>::const_iterator beg,
				    BCP_vec<BCP_proc_id*>::const_iterator end,
				    BCP_process_t ptype)
{
   if (beg != end) {
      p.msg_buf.clear();
      p.msg_buf.pack(ptype);
      p.msg_env->multicast(beg, end, BCP_Msg_ProcessType, p.msg_buf);

      p.msg_buf.clear();
      switch (ptype) {
       case BCP_ProcessType_LP:
	 p.slave_pars.lp.pack(p.msg_buf);
	 p.msg_buf.pack(p.ub());
	 break;
       case BCP_ProcessType_CP:
 	 // p.slave_pars.cp.pack(p.msg_buf);
 	 break;
       case BCP_ProcessType_VP:
 	 // p.slave_pars.vp.pack(p.msg_buf);
 	 break;
       case BCP_ProcessType_CG:
	 p.slave_pars.cg.pack(p.msg_buf);
	 break;
       case BCP_ProcessType_VG:
	 p.slave_pars.vg.pack(p.msg_buf);
	 break;
      case BCP_ProcessType_Any:
	throw BCP_fatal_error("\
Trying to send process params to BCP_ProcessType_Any!\n");
      case BCP_ProcessType_TM:
	throw BCP_fatal_error("\
Trying to send process params to BCP_ProcessType_TM!\n");
      }
      p.msg_env->multicast(beg, end, BCP_Msg_ProcessParameters, p.msg_buf);
   }
}

//#############################################################################

void BCP_tm_notify_processes(BCP_tm_prob& p)
{
   // Notify the processes what kind of process they are and also send over
   // the appropriate parameters
   BCP_tm_send_process_type_and_params(p,
				       p.slaves.lp->procs().begin(),
				       p.slaves.lp->procs().end(),
				       BCP_ProcessType_LP);
   BCP_tm_send_process_type_and_params(p,
				       p.slaves.cg->procs().begin(),
				       p.slaves.cg->procs().end(),
				       BCP_ProcessType_CG);
   BCP_tm_send_process_type_and_params(p,
				       p.slaves.vg->procs().begin(),
				       p.slaves.vg->procs().end(),
				       BCP_ProcessType_VG);
   BCP_tm_send_process_type_and_params(p,
				       p.slaves.cp->procs().begin(),
				       p.slaves.cp->procs().end(),
				       BCP_ProcessType_CP);
   BCP_tm_send_process_type_and_params(p,
				       p.slaves.vp->procs().begin(),
				       p.slaves.vp->procs().end(),
				       BCP_ProcessType_VP);
}
   
//#############################################################################

void BCP_tm_distribute_core(BCP_tm_prob& p)
{
   p.msg_buf.clear();
   p.core->pack(p.msg_buf);
   p.msg_env->multicast(p.slaves.all, BCP_Msg_CoreDescription, p.msg_buf);
}

//#############################################################################

void BCP_tm_distribute_user_info(BCP_tm_prob& p)
{
   if (p.slaves.lp->size()) {
      p.msg_buf.clear();
      p.user->pack_module_data(p.msg_buf, BCP_ProcessType_LP);
      p.msg_env->multicast(p.slaves.lp, BCP_Msg_InitialUserInfo, p.msg_buf);
   }
   if (p.slaves.cg) {
      p.msg_buf.clear();
      p.user->pack_module_data(p.msg_buf, BCP_ProcessType_CG);
      p.msg_env->multicast(p.slaves.cg, BCP_Msg_InitialUserInfo, p.msg_buf);
   }
   if (p.slaves.cp) {
      p.msg_buf.clear();
      p.user->pack_module_data(p.msg_buf, BCP_ProcessType_CP);
      p.msg_env->multicast(p.slaves.cp, BCP_Msg_InitialUserInfo, p.msg_buf);
   }
   if (p.slaves.vg) {
      p.msg_buf.clear();
      p.user->pack_module_data(p.msg_buf, BCP_ProcessType_VG);
      p.msg_env->multicast(p.slaves.vg, BCP_Msg_InitialUserInfo, p.msg_buf);
   }
   if (p.slaves.vp) {
      p.msg_buf.clear();
      p.user->pack_module_data(p.msg_buf, BCP_ProcessType_VP);
      p.msg_env->multicast(p.slaves.vp, BCP_Msg_InitialUserInfo, p.msg_buf);
   }
}

//#############################################################################
// This function returns T/F depending on whether an LP process was freed up
// or not.

void
BCP_tm_prob::process_message()
{
   BCP_tm_node* node;
   BCP_proc_id * sender;

   // msg_counter counts the number of incoming messages. Every so often
   // (always when there's no message in the buffer) we check that every
   // processor is alive and well. If any has died then appropriate process
   // list gets shrunk, tree nodes put back on the list, etc.
   // Of course, this makes sense only if the computing environment in NOT
   // serial.
   static int msg_count = 0;
   static const int test_frequency = 1;

   ++msg_count;

   switch (msg_buf.msgtag()){
    case BCP_Msg_NoMessage:
      msg_count = 0;
      break;

    case BCP_Msg_UpperBound:
      throw BCP_fatal_error("TM: Got BCP_Msg_UpperBound message!\n");
      break;

    case BCP_Msg_PricedRoot:
      BCP_tm_unpack_priced_root(*this, msg_buf);
      break;

    case BCP_Msg_NodeDescription_OverUB:
      node = BCP_tm_unpack_node_no_branching_info(*this, msg_buf);
      next_phase_nodes.push_back(node);
      node->status = BCP_NextPhaseNode_OverUB;
      break;

    case BCP_Msg_NodeDescription_Infeas:
      node = BCP_tm_unpack_node_no_branching_info(*this, msg_buf);
      next_phase_nodes.push_back(node);
      node->status = BCP_NextPhaseNode_Infeas;
      break;

    case BCP_Msg_NodeDescription_Discarded:
    case BCP_Msg_NodeDescription_OverUB_Pruned:
    case BCP_Msg_NodeDescription_Infeas_Pruned:
      node = BCP_tm_unpack_node_no_branching_info(*this, msg_buf);
      if (msg_buf.msgtag() == BCP_Msg_NodeDescription_OverUB_Pruned) {
	 node->status = BCP_PrunedNode_OverUB;
      } else if (msg_buf.msgtag() == BCP_Msg_NodeDescription_Infeas_Pruned) {
	 node->status = BCP_PrunedNode_Infeas;
      } else {
	 node->status = BCP_PrunedNode_Discarded;
      }
      if (node->cp) {
	 BCP_vec< std::pair<BCP_proc_id*, int> >::iterator proc =
	    BCP_tm_identify_process(leaves_per_cp, node->cp);
#ifdef BCP_DEBUG
	 if (proc == leaves_per_cp.end())
	    throw BCP_fatal_error("\
TM: non-existing CP was assigned to a just pruned node.\n");
#endif
	 if (--proc->second == 0)
	    slaves.cp->set_proc_free(proc->first->clone());
      }
      if (node->vp) {
	 BCP_vec< std::pair<BCP_proc_id*, int> >::iterator proc =
	    BCP_tm_identify_process(leaves_per_vp, node->vp);
#ifdef BCP_DEBUG
	 if (proc == leaves_per_vp.end())
	    throw BCP_fatal_error("\
TM: non-existing VP was assigned to a just pruned node.\n");
#endif
	 if (--proc->second == 0)
	    slaves.vp->set_proc_free(proc->first->clone());
      }
      break;

    case BCP_Msg_NodeDescriptionWithBranchingInfo:
      BCP_tm_unpack_node_with_branching_info(*this, msg_buf);
      break;

    case BCP_Msg_FeasibleSolution:
      {
	BCP_solution *new_sol = user->unpack_feasible_solution(msg_buf);
	if (new_sol) {
	  const bool allval =
	    param(BCP_tm_par::TmVerb_AllFeasibleSolutionValue);
	  const bool allsol =
	    param(BCP_tm_par::TmVerb_AllFeasibleSolution);
	  const bool betterval =
	    param(BCP_tm_par::TmVerb_BetterFeasibleSolutionValue);
	  const bool bettersol =
	    param(BCP_tm_par::TmVerb_BetterFeasibleSolution);
	  bool better;
	  if (upper_bound > new_sol->objective_value() + 1e-6) {
	     better = true;
	  } else if (upper_bound > new_sol->objective_value() - 1e-6) {
	     better = user->replace_solution(feas_sol, new_sol);
	  } else {
	     better = false;
	  }

	  if (allval || allsol || ((betterval || bettersol) && better)) {
	    if (param(BCP_tm_par::TmVerb_TimeOfImprovingSolution)) {
	      printf("TM: Solution found at %.3f sec.\n",
		     BCP_time_since_epoch() - start_time);
	    }
	    if (upper_bound > DBL_MAX/2) {
	       printf("\
TM: Solution value: %f (best solution value so far: infinity)\n",
		      new_sol->objective_value());
	    } else {
	       printf("\
TM: Solution value: %f (best solution value so far: %f)\n",
		      new_sol->objective_value(), upper_bound);
	    }
	    if (allsol || (bettersol && better)) {
	      user->display_feasible_solution(new_sol);
	    }
	  }
	  if (better) {
	    ub(new_sol->objective_value());
	    delete feas_sol;
	    feas_sol = new_sol;
	    msg_buf.clear();
	    msg_buf.pack(new_sol->objective_value());
	    msg_env->multicast(slaves.all, BCP_Msg_UpperBound, msg_buf);
	  } else {
	    delete new_sol;
	  }
	}
      }
      break;

    case BCP_Msg_RequestCutIndexSet:
      sender = msg_buf.sender()->clone();
      msg_buf.clear();
      msg_buf.pack(next_cut_index_set_start);
      next_cut_index_set_start += 10000;
      msg_buf.pack(next_cut_index_set_start);
      msg_env->send(sender, BCP_Msg_CutIndexSet, msg_buf);
      delete sender;
      break;
      
    case BCP_Msg_RequestVarIndexSet:
      sender = msg_buf.sender()->clone();
      msg_buf.clear();
      msg_buf.pack(next_var_index_set_start);
      next_var_index_set_start += 10000;
      msg_buf.pack(next_var_index_set_start);
      msg_env->send(sender, BCP_Msg_VarIndexSet, msg_buf);
      delete sender;
      break;

    case BCP_Msg_LpStatistics:
      throw BCP_fatal_error("\
Unexpected BCP_Msg_LpStatistics message in BCP_tm_prob::process_message.\n");

    case BCP_Msg_SomethingDied:
      // *FIXME-NOW* : what to do when something has died?
      break;

    case BCP_ARE_YOU_TREEMANAGER:
      msg_env->send(msg_buf.sender(), BCP_I_AM_TREEMANAGER);
      break;

    case BCP_CONFIG_CHANGE:
      BCP_tm_change_config(*this, msg_buf);
      break;

    default:
      throw BCP_fatal_error("\
Unknown message in BCP_tm_prob::process_message.\n");
   }
   msg_buf.clear();

   const double t = BCP_time_since_epoch() - start_time;
   const bool time_is_over = t > param(BCP_tm_par::MaxRunTime);

   if (! param(BCP_tm_par::MessagePassingIsSerial) &&
       msg_count % test_frequency == 0) {
      // run the machine testing while it returns false (i.e., it did not
      // check out, it had to delete something)
      while (! BCP_tm_test_machine(*this));
      if (slaves.lp->size() == 0)
	 throw BCP_fatal_error("TM: No LP process left to compute with...\n");
   }

   if (time_is_over) {
     const double lb = search_tree.true_lower_bound(search_tree.root());
     printf("TM: Time has ran out... %.3f secs\n", t);
     printf("TM: Best lower bound in this phase: %f\n", lb);
     BCP_lp_statistics dummy_stat;
     user->display_final_information(dummy_stat);
     fflush(0);
     BCP_tm_save_root_cuts(this);
     BCP_tm_stop_processes(*this);
     exit(0);
   }
}

//#############################################################################

bool
BCP_tm_test_machine(BCP_tm_prob& p)
{
   BCP_vec<BCP_proc_id*>::iterator dead_process_i =
      p.msg_env->alive(*p.slaves.all);

   if (dead_process_i == p.slaves.all->procs().end())
      // everything is fine
      return true;

   BCP_proc_id* dead_process = *dead_process_i;
   int i;
   bool continue_testing = true;
   // Oops, something has died, must write it off.

   if (continue_testing) {
      i = p.slaves.lp->index_of_proc(dead_process);
      if (i >= 0) {
	 printf("TM: The %i-th LP is dead. Removing the LP/CG/VG/triplet.\n",
		i);
	 BCP_tm_remove_lp(p, i);
	 continue_testing = false;
      }
   }

   if (continue_testing && p.slaves.cg) {
      i = p.slaves.cg->index_of_proc(dead_process);
      if (i >= 0) {
	 printf("TM: The %i-th CG is dead. Removing the LP/CG/VG/triplet.\n",
		i);
	 BCP_tm_remove_cg(p, i);
	 continue_testing = false;
      }
   }

   if (continue_testing && p.slaves.vg) {
      i = p.slaves.vg->index_of_proc(dead_process);
      if (i >= 0) {
	 printf("TM: The %i-th VG is dead. Removing the LP/CG/VG/triplet.\n",
		i);
	 BCP_tm_remove_vg(p, i);
	 continue_testing = false;
      }
   }

   p.slaves.all->delete_proc(p.slaves.all->index_of_proc(dead_process));
   delete dead_process;

   return false;
}

//#############################################################################

void
BCP_tm_remove_lp(BCP_tm_prob& p, const int index)
{
   BCP_tm_node* node = p.active_nodes[index];
   if (node) {
      if (! node->lp->is_same_process(p.slaves.lp->procs()[index]))
	 throw BCP_fatal_error("TM: messed up active nodes... :-(.\n");
      // Got to put back the search tree node onto the candidate list.
      // fix the CP/VP fields
      if (node->cp) {
	 BCP_vec< std::pair<BCP_proc_id*, int> >::iterator proc =
	    BCP_tm_identify_process(p.leaves_per_cp, node->cp);
	 if (proc == p.leaves_per_cp.end()) {
	    node->cp = 0; 
	 }
      }
      if (node->vp) {
	 BCP_vec< std::pair<BCP_proc_id*, int> >::iterator proc =
	    BCP_tm_identify_process(p.leaves_per_vp, node->vp);
	 if (proc == p.leaves_per_vp.end()) {
	    node->vp = 0; 
	 }
      }
      p.active_nodes.erase(p.active_nodes.entry(index));

      node->lp = node->cg = node->vg = 0;
      node->status = BCP_CandidateNode;
      p.candidates.insert(node);
   }

   BCP_proc_id * proc;
   if (p.slaves.cg) {
      proc = p.slaves.cg->procs()[index];
      p.slaves.cg->delete_proc(index);
      p.slaves.all->delete_proc(p.slaves.all->index_of_proc(proc));
   }
   if (p.slaves.vg) {
      proc = p.slaves.vg->procs()[index];
      p.slaves.vg->delete_proc(index);
      p.slaves.all->delete_proc(p.slaves.all->index_of_proc(proc));
   }

   proc = p.slaves.lp->procs()[index];
   p.slaves.lp->delete_proc(index);
   p.slaves.all->delete_proc(p.slaves.all->index_of_proc(proc));
}

void
BCP_tm_remove_cg(BCP_tm_prob& p, const int index)
{
   // for now this just removes the lp
   BCP_tm_remove_lp(p, index);
}

void
BCP_tm_remove_vg(BCP_tm_prob& p, const int index)
{
   // for now this just removes the lp
   BCP_tm_remove_lp(p, index);
}


//#############################################################################

void
BCP_tm_unpack_priced_root(BCP_tm_prob& p, BCP_buffer& buf)
{
   // only a BCP_named_pricing_list is sent over, unpack it.
   BCP_tm_node* root = p.search_tree.root();

   root->_desc->indexed_pricing.unpack(buf);
   p.flags.root_pricing_unpacked = true;

   BCP_tm_free_procs_of_node(p, root);
   p.active_nodes[p.slaves.lp->index_of_proc(root->lp)] = 0;
}

//#############################################################################

void
BCP_tm_free_procs_of_node(BCP_tm_prob& p, BCP_tm_node* node)
{
   p.slaves.lp->set_proc_free(node->lp);
   if (node->cg)
      p.slaves.cg->set_proc_free(node->cg);
   if (node->vg)
      p.slaves.vg->set_proc_free(node->vg);
   // node's pointers are 0'd out (not deleted!)
   node->lp = node->cg = node->vg = 0;
}

//#############################################################################

static void
BCP_tm_distribute_info_to_new_procs(BCP_tm_prob& p,
				    BCP_vec<BCP_proc_id*>& procs,
				    BCP_process_t ptype)
{
   p.slaves.all->add_procs(procs.begin(), procs.end());
   BCP_tm_send_process_type_and_params(p, procs.begin(), procs.end(), ptype);
   p.msg_buf.clear();
   p.core->pack(p.msg_buf);
   p.msg_env->multicast(procs.begin(), procs.end(),
			BCP_Msg_CoreDescription, p.msg_buf);
   p.msg_buf.clear();
   p.user->pack_module_data(p.msg_buf, ptype);
   p.msg_env->multicast(procs.begin(), procs.end(),
			BCP_Msg_InitialUserInfo, p.msg_buf);
}

//#############################################################################

static void
BCP_tm_change_config(BCP_tm_prob& p, BCP_buffer& buf)
{
   int i;
   BCP_proc_id * sender = buf.sender()->clone();

   int lp_num = 0;
   buf.unpack(lp_num);
   BCP_vec<BCP_string> lp(lp_num, "");
   for (i = 0; i < lp_num; ++i)
      buf.unpack(lp[i]);

   int cg_num = 0;
   buf.unpack(cg_num);
   BCP_vec<BCP_string> cg(cg_num, "");
   for (i = 0; i < cg_num; ++i)
      buf.unpack(cg[i]);

   int vg_num = 0;
   buf.unpack(vg_num);
   BCP_vec<BCP_string> vg(vg_num, "");
   for (i = 0; i < vg_num; ++i)
      buf.unpack(vg[i]);

   int cp_num = 0;
   buf.unpack(cp_num);
   BCP_vec<BCP_string> cp(cp_num, "");
   for (i = 0; i < cp_num; ++i)
      buf.unpack(cp[i]);

   int vp_num = 0;
   buf.unpack(vp_num);
   BCP_vec<BCP_string> vp(vp_num, "");
   for (i = 0; i < vp_num; ++i)
      buf.unpack(vp[i]);

   // Check that everything works out fine

   // At least for now cg_num/vg_num must be the same as lp_num or must be 0
   if (p.slaves.cg->size() > 0) {
      if (cg_num != lp_num) {
	 printf("\
BCP: config change: CG's are running thus the number of new CG's must be\
     the same as the number of new LP's. Not changing anything.\n");
	 p.msg_env->send(sender, BCP_CONFIG_ERROR);
	 return;
      }
   } else {
      if (cg_num != 0) {
	 printf("\
BCP: config change: CG's are not running thus no new CG's can be started.\
     Not changing anything.\n");
	 p.msg_env->send(sender, BCP_CONFIG_ERROR);
	 return;
      }
   }
   if (p.slaves.vg->size() > 0) {
      if (vg_num != lp_num) {
	 printf("\
BCP: config change: VG's are running thus the number of new VG's must be\
     the same as the number of new LP's. Not changing anything.\n");
	 p.msg_env->send(sender, BCP_CONFIG_ERROR);
	 return;
      }
   } else {
      if (vg_num != 0) {
	 printf("\
BCP: config change: VG's are not running thus no new VG's can be started.\
     Not changing anything.\n");
	 p.msg_env->send(sender, BCP_CONFIG_ERROR);
	 return;
      }
   }

   // Spawn the jobs
   const BCP_string& exe = p.param(BCP_tm_par::ExecutableName);
   if (lp_num > 0) {
      const bool debug = p.param(BCP_tm_par::DebugLpProcesses) != 0;
      BCP_proc_array* new_lps =
	 p.msg_env->start_processes(exe, lp_num, lp, debug);
      p.slaves.lp->add_procs(new_lps->procs().begin(),
			     new_lps->procs().end());
      p.active_nodes.insert(p.active_nodes.end(), lp_num, 0);
      BCP_tm_distribute_info_to_new_procs(p, new_lps->procs(),
					  BCP_ProcessType_LP);
   }
   if (cg_num > 0) {
      const bool debug = p.param(BCP_tm_par::DebugCgProcesses) != 0;
      BCP_proc_array* new_cgs =
	 p.msg_env->start_processes(exe, cg_num, cg, debug);
      p.slaves.cg->add_procs(new_cgs->procs().begin(),
			     new_cgs->procs().end());
      BCP_tm_distribute_info_to_new_procs(p, new_cgs->procs(),
					  BCP_ProcessType_CG);
   }
   if (vg_num > 0) {
      const bool debug = p.param(BCP_tm_par::DebugVgProcesses) != 0;
      BCP_proc_array* new_vgs =
	 p.msg_env->start_processes(exe, vg_num, vg, debug);
      p.slaves.vg->add_procs(new_vgs->procs().begin(),
			     new_vgs->procs().end());
      BCP_tm_distribute_info_to_new_procs(p, new_vgs->procs(),
					  BCP_ProcessType_VG);
   }
   if (cp_num > 0) {
      const bool debug = p.param(BCP_tm_par::DebugCpProcesses) != 0;
      BCP_proc_array* new_cps =
	 p.msg_env->start_processes(exe, cp_num, cp, debug);
      p.slaves.cp->add_procs(new_cps->procs().begin(),
			     new_cps->procs().end());
      BCP_tm_distribute_info_to_new_procs(p, new_cps->procs(),
					  BCP_ProcessType_CP);
   }
   if (vp_num > 0) {
      const bool debug = p.param(BCP_tm_par::DebugVpProcesses) != 0;
      BCP_proc_array* new_vps =
	 p.msg_env->start_processes(exe, vp_num, vp, debug);
      p.slaves.vp->add_procs(new_vps->procs().begin(),
			     new_vps->procs().end());
      BCP_tm_distribute_info_to_new_procs(p, new_vps->procs(),
					  BCP_ProcessType_VP);
   }

   // Signal back that everything is OK.
   p.msg_env->send(sender, BCP_CONFIG_OK);

   delete sender;
}

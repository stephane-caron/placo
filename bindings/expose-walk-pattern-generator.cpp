#include <pinocchio/fwd.hpp>

#include "expose-utils.hpp"
#include "module.h"
#include "placo/planning/walk_pattern_generator.h"
#include "placo/control/kinematics_solver.h"
#include "placo/footsteps/footsteps_planner.h"
#include "placo/planning/solver_task_holder.h"
#include "placo/planning/swing_foot_quintic.h"
#include <Eigen/Dense>
#include <boost/python.hpp>

using namespace boost::python;
using namespace placo;

void exposeWalkPatternGenerator()
{
  class_<WalkPatternGenerator::Trajectory>("WalkTrajectory")
      .add_property("supports", &WalkPatternGenerator::Trajectory::supports)
      .add_property("com", &WalkPatternGenerator::Trajectory::com)
      .add_property("duration", &WalkPatternGenerator::Trajectory::duration)
      .add_property("jerk_planner_steps", &WalkPatternGenerator::Trajectory::jerk_planner_steps)
      .def("get_T_world_left", &WalkPatternGenerator::Trajectory::get_T_world_left)
      .def("get_T_world_right", &WalkPatternGenerator::Trajectory::get_T_world_right)
      .def("get_CoM_world", &WalkPatternGenerator::Trajectory::get_CoM_world)
      .def("get_R_world_trunk", &WalkPatternGenerator::Trajectory::get_R_world_trunk)
      .def("support_side", &WalkPatternGenerator::Trajectory::support_side)
      .def("get_last_footstep_frame", &WalkPatternGenerator::Trajectory::get_last_footstep_frame);

  class_<WalkPatternGenerator>("WalkPatternGenerator", init<HumanoidRobot&, FootstepsPlanner&, HumanoidParameters&>())
      .def("plan", &WalkPatternGenerator::plan)
      .def("replan", &WalkPatternGenerator::replan)
      .def("plan_kick", &WalkPatternGenerator::plan_kick)
      .def("plan_one_foot_balance", &WalkPatternGenerator::plan_one_foot_balance);

  class_<SolverTaskHolder>("SolverTaskHolder", init<HumanoidRobot&, KinematicsSolver&>())
      .def("update_tasks", &SolverTaskHolder::update_tasks);

  class_<SwingFoot>("SwingFoot", init<>()).def("make_trajectory", &SwingFoot::make_trajectory);

  class_<SwingFoot::Trajectory>("SwingFootTrajectory", init<>())
      .def("pos", &SwingFoot::Trajectory::pos)
      .def("vel", &SwingFoot::Trajectory::vel);

  class_<SwingFootQuintic>("SwingFootQuintic", init<>()).def("make_trajectory", &SwingFootQuintic::make_trajectory);

  class_<SwingFootQuintic::Trajectory>("SwingFootQuinticTrajectory", init<>())
      .def("pos", &SwingFootQuintic::Trajectory::pos)
      .def("vel", &SwingFootQuintic::Trajectory::vel);
}
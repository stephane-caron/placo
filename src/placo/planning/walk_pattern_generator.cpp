#include "placo/planning/walk_pattern_generator.h"
#include "placo/footsteps/footsteps_planner.h"
#include "placo/utils.h"

namespace placo
{
WalkPatternGenerator::WalkPatternGenerator(HumanoidRobot& robot, HumanoidParameters& parameters)
  : robot(robot), parameters(parameters)
{
}

static Eigen::Affine3d _buildFrame(Eigen::Vector3d position, double orientation)
{
  Eigen::Affine3d frame = Eigen::Affine3d::Identity();

  frame.translation() = position;
  frame.linear() = Eigen::AngleAxisd(orientation, Eigen::Vector3d::UnitZ()).matrix();

  return frame;
}

static WalkPatternGenerator::TrajectoryPart& _findPart(std::vector<WalkPatternGenerator::TrajectoryPart>& parts,
                                                       double t)
{
  int low = 0;
  int high = parts.size() - 1;

  while (low != high)
  {
    int mid = (low + high) / 2;

    WalkPatternGenerator::TrajectoryPart& part = parts[mid];

    if (t < part.t_start)
    {
      high = mid;
    }
    else if (t > part.t_end)
    {
      low = mid + 1;
    }
    else
    {
      return part;
    }
  }

  return parts[low];
}

Eigen::Affine3d WalkPatternGenerator::Trajectory::get_T_world_left(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);

  if (part.support.side() == HumanoidRobot::Right)
  {
    return _buildFrame(part.swing_trajectory.pos(t), left_foot_yaw.get(t));
  }
  else
  {
    return _buildFrame(part.support.footstep_frame(HumanoidRobot::Left).translation(), left_foot_yaw.get(t));
  }
}

Eigen::Affine3d WalkPatternGenerator::Trajectory::get_T_world_right(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);

  if (part.support.side() == HumanoidRobot::Left)
  {
    return _buildFrame(part.swing_trajectory.pos(t), right_foot_yaw.get(t));
  }
  else
  {
    return _buildFrame(part.support.footstep_frame(HumanoidRobot::Right).translation(), right_foot_yaw.get(t));
  }
}

Eigen::Vector3d WalkPatternGenerator::Trajectory::get_v_world_left(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);

  if (part.support.side() == HumanoidRobot::Right)
  {
    return part.swing_trajectory.vel(t);
  }
  return Eigen::Vector3d::Zero();
}

Eigen::Vector3d WalkPatternGenerator::Trajectory::get_v_world_right(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);

  if (part.support.side() == HumanoidRobot::Left)
  {
    return part.swing_trajectory.vel(t);
  }
  return Eigen::Vector3d::Zero();
}

Eigen::Vector3d WalkPatternGenerator::Trajectory::get_p_world_CoM(double t)
{
  auto pos = com.pos(t);

  return Eigen::Vector3d(pos.x(), pos.y(), com_height);
}

Eigen::Matrix3d WalkPatternGenerator::Trajectory::get_R_world_trunk(double t)
{
  t += time_offset;
  return Eigen::AngleAxisd(trunk_yaw.get(t), Eigen::Vector3d::UnitZ()).matrix() *
         Eigen::AngleAxisd(trunk_pitch, Eigen::Vector3d::UnitY()).matrix();
}

HumanoidRobot::Side WalkPatternGenerator::Trajectory::support_side(double t)
{
  t += time_offset;
  return _findPart(parts, t).support.side();
}

rhoban_utils::PolySpline& WalkPatternGenerator::Trajectory::yaw(HumanoidRobot::Side side)
{
  if (side == HumanoidRobot::Left)
  {
    return left_foot_yaw;
  }
  else
  {
    return right_foot_yaw;
  }
}

FootstepsPlanner::Support WalkPatternGenerator::Trajectory::get_support(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);
  return part.support;
}

FootstepsPlanner::Support WalkPatternGenerator::Trajectory::get_next_support(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);
  TrajectoryPart& next_part = _findPart(parts, part.t_end + 1e-4);
  return next_part.support;
}

FootstepsPlanner::Support WalkPatternGenerator::Trajectory::get_prev_support(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);
  TrajectoryPart& prev_part = _findPart(parts, part.t_start - 1e-4);
  return prev_part.support;
}

double WalkPatternGenerator::Trajectory::get_phase_t_start(double t)
{
  t += time_offset;
  TrajectoryPart& part = _findPart(parts, t);
  return part.t_start;
}

static void _addSupports(WalkPatternGenerator::Trajectory& trajectory, double t, FootstepsPlanner::Support& support)
{
  for (auto footstep : support.footsteps)
  {
    auto T_world_foot = footstep.frame;
    trajectory.yaw(footstep.side).addPoint(t, frame_yaw(T_world_foot.rotation()), 0);
  }
}

void WalkPatternGenerator::planCoM(Trajectory& trajectory, Eigen::Vector2d initial_vel, Eigen::Vector2d initial_acc)
{
  // Computing how many steps are required
  int ssp_dt = std::round(parameters.single_support_duration / parameters.dt);
  int dsp_dt = std::round(parameters.double_support_duration / parameters.dt);
  int se_dsp_dt = std::round(parameters.startend_double_support_duration / parameters.dt);

  int total_dt = -std::round(trajectory.time_offset / parameters.dt);
  int current_step_dt = total_dt;
  int current_step_remaining_dt = -1;
  int nb_elapsed_supports = 0;

  for (int i = 0; i < trajectory.supports.size(); i++)
  {
    if (trajectory.supports[i].footsteps.size() == 1)
    {
      total_dt += ssp_dt;
    }
    else
    {
      if (trajectory.supports[i].start || trajectory.supports[i].end)
      {
        total_dt += se_dsp_dt;
      }
      else
      {
        total_dt += dsp_dt;
      }
    }

    if (total_dt <= 0)
    {
      nb_elapsed_supports += 1;
      current_step_dt = total_dt;
    }
    else if (current_step_remaining_dt == -1)
    {
      current_step_remaining_dt = total_dt;
      current_step_dt = total_dt - current_step_dt;
    }

    if (total_dt >= parameters.planned_dt)
    {
      break;
    }
  }

  trajectory.jerk_planner_nb_dt = total_dt;

  trajectory.are_supports_updatable = false;
  if (current_step_remaining_dt > parameters.replan_frequency && current_step_remaining_dt != current_step_dt)
  {
    trajectory.are_supports_updatable = true;
  }

  // Creating the planner
  auto com_world = robot.com_world();
  JerkPlanner planner(trajectory.jerk_planner_nb_dt, Eigen::Vector2d(com_world.x(), com_world.y()), initial_vel,
                      initial_acc, parameters.dt, parameters.omega());

  auto current_support = trajectory.supports[nb_elapsed_supports];

  // std::cout << "----------------------------------- " << std::endl;
  // std::cout << "nb_elapsed_supports : " << nb_elapsed_supports << std::endl;
  // std::cout << "current_support_side : " << current_support.side() << std::endl;
  // std::cout << "current_step_dt : " << current_step_dt << std::endl;
  // std::cout << "current_step_remaining_dt : " << current_step_remaining_dt << std::endl;
  // std::cout << "total_dt : " << total_dt << std::endl;
  // std::cout << "----------------------------------- " << std::endl;

  // If it's the beginning of a new step, a dt where the ZMP is constraint in a double support
  // is added to allow it to reach the other foot in the case where there isn't double support phases
  if (current_support.footsteps.size() == 1 && current_step_dt == current_step_remaining_dt && !current_support.start)
  {
    FootstepsPlanner::Support double_support;
    double_support.footsteps = current_support.footsteps;
    for (auto footstep : trajectory.supports[nb_elapsed_supports - 1].footsteps)
    {
      double_support.footsteps.push_back(footstep);
    }
    planner.add_polygon_constraint(0, double_support.support_polygon(), JerkPlanner::ZMP, parameters.zmp_margin);

    for (int k = 1; k < current_step_remaining_dt; k++)
    {
      planner.add_polygon_constraint(k, current_support.support_polygon(), JerkPlanner::ZMP, parameters.zmp_margin);
    }
  }
  // In every other cases the ZMP is constraint to be above the support feet the whole support phase
  else
  {
    for (int k = 0; k < current_step_remaining_dt; k++)
    {
      planner.add_polygon_constraint(k, current_support.support_polygon(), JerkPlanner::ZMP, parameters.zmp_margin);
    }
  }

  // The ZMP is constraint the same way for the following steps
  int dt = current_step_remaining_dt;
  for (int i = nb_elapsed_supports + 1; i < trajectory.supports.size(); i++)
  {
    current_support = trajectory.supports[i];

    if (current_support.footsteps.size() == 1)
    {
      // Add a dt where the ZMP is constraint in a double support to allow it to reach
      // the other support foot in the case where there is no double support phases
      FootstepsPlanner::Support double_support;
      double_support.footsteps = current_support.footsteps;
      for (auto footstep : trajectory.supports[i - 1].footsteps)
      {
        double_support.footsteps.push_back(footstep);
      }
      planner.add_polygon_constraint(dt, double_support.support_polygon(), JerkPlanner::ZMP, parameters.zmp_margin);

      // Constraint the ZMP to be above the support foot
      for (int k = 1; k < ssp_dt; k++)
      {
        planner.add_polygon_constraint(dt + k, current_support.support_polygon(), JerkPlanner::ZMP,
                                       parameters.zmp_margin);
      }
      dt += ssp_dt;
    }

    else
    {
      if (current_support.end)
      {
        // Constraint the ZMP to be above the support feet
        for (int k = 0; k < se_dsp_dt; k++)
        {
          planner.add_polygon_constraint(dt + k, current_support.support_polygon(), JerkPlanner::ZMP,
                                         parameters.zmp_margin);
        }
        dt += se_dsp_dt;
      }
      else
      {
        // Constraint the ZMP to be above the support feet
        for (int k = 0; k < dsp_dt; k++)
        {
          planner.add_polygon_constraint(dt + k, current_support.support_polygon(), JerkPlanner::ZMP,
                                         parameters.zmp_margin);
        }
        dt += dsp_dt;
      }
    }

    if (dt >= trajectory.jerk_planner_nb_dt)
    {
      break;
    }
  }

  // We reach the target with the given position, a null speed and a null acceleration
  if (current_support.end)
  {
    planner.add_equality_constraint(
        dt - 1, Eigen::Vector2d(current_support.frame().translation().x(), current_support.frame().translation().y()),
        JerkPlanner::Position);
    planner.add_equality_constraint(dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Velocity);
    planner.add_equality_constraint(dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Acceleration);
  }

  trajectory.com = planner.plan();
}

void WalkPatternGenerator::planFeetTrajectories(Trajectory& trajectory)
{
  double t = 0.0;

  // Add the initial position to the trajectory
  _addSupports(trajectory, 0., trajectory.supports[0]);
  trajectory.trunk_yaw.addPoint(0, frame_yaw(trajectory.supports[0].frame().rotation()), 0);

  for (int step = 0; step < trajectory.supports.size(); step++)
  {
    auto& support = trajectory.supports[step];

    TrajectoryPart part;
    part.support = support;
    part.t_start = t;

    if (support.footsteps.size() == 1)
    {
      // Single support, add the flying trajectory
      auto flying_side = HumanoidRobot::other_side(support.footsteps[0].side);

      // Computing the intermediary flying foot inflection target
      auto T_world_flyingTarget = trajectory.supports[step + 1].footstep_frame(flying_side);
      auto T_world_startTarget = trajectory.initial_T_world_flying_foot;
      if (!support.start)
      {
        T_world_startTarget = trajectory.supports[step - 1].footstep_frame(flying_side);
      }

      t += parameters.single_support_duration;

      // Flying foot reaching its position
      part.swing_trajectory =
          SwingFoot::make_trajectory(t - parameters.single_support_duration, t, parameters.walk_foot_height,
                                     T_world_startTarget.translation(), T_world_flyingTarget.translation());

      trajectory.yaw(flying_side).addPoint(t, frame_yaw(T_world_flyingTarget.rotation()), 0);

      // The trunk orientation follow the steps orientation if there isn't double support phases
      // If there is double support phases, it follow the double supports orientation
      if (parameters.double_support_duration < parameters.dt)
      {
        trajectory.trunk_yaw.addPoint(t, frame_yaw(T_world_flyingTarget.rotation()), 0);
      }

      // Support foot remaining steady
      _addSupports(trajectory, t, support);
    }

    else
    {
      // Double support, adding the support foot at the begining and at the end of the trajectory
      if (support.start || support.end)
      {
        t += parameters.startend_double_support_duration;
      }
      else
      {
        t += parameters.double_support_duration;
      }
      _addSupports(trajectory, t, support);
      trajectory.trunk_yaw.addPoint(t, frame_yaw(support.frame().rotation()), 0);
    }

    part.t_end = t;
    trajectory.parts.push_back(part);
  }

  trajectory.duration = t;
}

WalkPatternGenerator::Trajectory WalkPatternGenerator::plan(std::vector<FootstepsPlanner::Support>& supports)
{
  // Initialization of the trajectory
  Trajectory trajectory;
  trajectory.com_height = parameters.walk_com_height;
  trajectory.trunk_pitch = parameters.walk_trunk_pitch;
  trajectory.supports = supports;

  // Planning the center of mass trajectory
  planCoM(trajectory);

  // Planning the footsteps trajectories
  planFeetTrajectories(trajectory);

  trajectory.are_supports_updatable = false;
  return trajectory;
}

bool WalkPatternGenerator::replan(std::vector<FootstepsPlanner::Support>& supports,
                                  WalkPatternGenerator::Trajectory& trajectory, double elapsed)
{
  if (elapsed < parameters.replan_frequency * parameters.dt ||
      round((trajectory.duration - (elapsed + trajectory.time_offset)) / parameters.dt) <= 1)
  {
    return false;
  }

  // Initialization of the new trajectory
  Trajectory new_trajectory;
  new_trajectory.com_height = parameters.walk_com_height;
  new_trajectory.trunk_pitch = parameters.walk_trunk_pitch;
  new_trajectory.supports_update_offset = trajectory.supports_update_offset;
  new_trajectory.initial_T_world_flying_foot = trajectory.initial_T_world_flying_foot;
  new_trajectory.supports = supports;

  if (supports[0] == trajectory.supports[0])
  {
    new_trajectory.time_offset = trajectory.time_offset + elapsed;
  }
  else
  {
    new_trajectory.time_offset = trajectory.time_offset + elapsed - trajectory.supports_update_offset;
  }

  // Planning the center of mass trajectory
  planCoM(new_trajectory, trajectory.com.vel(elapsed), trajectory.com.acc(elapsed));

  if (trajectory.support_side(elapsed) == HumanoidRobot::Both)
  {
    new_trajectory.are_supports_updatable = false;
  }

  // Planning the footsteps trajectories
  planFeetTrajectories(new_trajectory);

  trajectory = new_trajectory;
  return true;
}

// std::vector<FootstepsPlanner::Support>
// WalkPatternGenerator::planSupportsKick(WalkPatternGenerator::Trajectory trajectory, HumanoidRobot::Side
// kicking_side,
//                                        Eigen::Affine3d T_world_left, Eigen::Affine3d T_world_right)
// {
//   FootstepsPlanner::Footstep first_footstep(parameters.foot_width, parameters.foot_length);
//   first_footstep.side = kicking_side;
//   first_footstep.frame = kicking_side == HumanoidRobot::Side::Left ? T_world_left : T_world_right;

//   FootstepsPlanner::Footstep second_footstep(parameters.foot_width, parameters.foot_length);
//   second_footstep.side = HumanoidRobot::other_side(kicking_side);
//   second_footstep.frame = kicking_side == HumanoidRobot::Side::Left ? T_world_right : T_world_left;

//   FootstepsPlanner::Footstep third_footstep(parameters.foot_width, parameters.foot_length);
//   third_footstep.side = kicking_side;
//   third_footstep.frame = kicking_side == HumanoidRobot::Side::Left ? T_world_right : T_world_left;
//   third_footstep.frame.translation().y() +=
//       kicking_side == HumanoidRobot::Side::Left ? parameters.feet_spacing : -parameters.feet_spacing;

//   std::vector<FootstepsPlanner::Support> supports;

//   // First support
//   FootstepsPlanner::Support support;
//   support.start_end = true;
//   support.footsteps = { first_footstep, second_footstep };
//   supports.push_back(support);

//   // Second support
//   support.start_end = false;
//   support.footsteps = { second_footstep };
//   supports.push_back(support);

//   // End support
//   support.start_end = true;
//   support.footsteps = { second_footstep, third_footstep };
//   supports.push_back(support);

//   return supports;
// }

// void WalkPatternGenerator::planCoMKick(Trajectory& trajectory, Eigen::Vector2d initial_vel, Eigen::Vector2d
// initial_acc)
// {
//   // Computing how many steps are required
//   int ssp_dt = std::round(parameters.single_support_duration / parameters.dt);
//   int dsp_dt = std::round(parameters.double_support_duration / parameters.dt);
//   int kick_dt = std::round(parameters.kick_duration / parameters.dt);

//   double total_dt = 3 * ssp_dt + kick_dt;
//   trajectory.jerk_planner_dt = total_dt;

//   // Creating the planner
//   auto com_world = robot.com_world();
//   auto target = trajectory.supports[1].frame().translation();
//   JerkPlanner planner(total_dt, Eigen::Vector2d(com_world.x(), com_world.y()), initial_vel, initial_acc,
//                       parameters.dt, parameters.omega());

//   planner.add_equality_constraint(ssp_dt / 2, Eigen::Vector2d(target.x(), target.y()), JerkPlanner::ZMP);

//   // Constraint the ZMP to be in the support polygon during the kick
//   for (int i = ssp_dt / 2 + 1; i < total_dt - ssp_dt / 2; i++)
//   {
//     planner.add_polygon_constraint(i, trajectory.supports[1].support_polygon(), JerkPlanner::ZMP, 0.02);
//   }

//   planner.add_equality_constraint(total_dt - ssp_dt / 2 + 1, Eigen::Vector2d(target.x(), target.y()),
//                                   JerkPlanner::ZMP);

//   // The CoM has to be stopped between the 2 feet at the end
//   target = trajectory.supports[2].frame().translation();
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(target.x(), target.y()), JerkPlanner::Position);
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Velocity);
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Acceleration);

//   trajectory.com = planner.plan();
// }

// void WalkPatternGenerator::planCoMOneFoot(Trajectory& trajectory, Eigen::Vector2d initial_vel,
//                                           Eigen::Vector2d initial_acc)
// {
//   // Computing how many steps are required
//   int ssp_dt = std::round(parameters.single_support_duration / parameters.dt);

//   double total_dt = 2 * ssp_dt;
//   trajectory.jerk_planner_dt = total_dt;

//   // Creating the planner
//   auto com_world = robot.com_world();
//   auto target = trajectory.supports[1].frame().translation();
//   JerkPlanner planner(total_dt, Eigen::Vector2d(com_world.x(), com_world.y()), initial_vel, initial_acc,
//                       parameters.dt, parameters.omega());

//   planner.add_equality_constraint(ssp_dt / 2, Eigen::Vector2d(target.x(), target.y()), JerkPlanner::ZMP);

//   // Constraint the ZMP to be in the support polygon during the kick
//   for (int i = ssp_dt / 2 + 1; i < total_dt - 1; i++)
//   {
//     planner.add_polygon_constraint(i, trajectory.supports[1].support_polygon(), JerkPlanner::ZMP, 0.02);
//   }

//   // CoM constraints
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(target.x(), target.y()), JerkPlanner::Position);
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Velocity);
//   planner.add_equality_constraint(total_dt - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Acceleration);

//   trajectory.com = planner.plan();
// }

// void WalkPatternGenerator::planFeetKick(Trajectory& trajectory, HumanoidRobot::Side kicking_side,
//                                         Eigen::Affine3d T_world_target)
// {
//   double t = 0.0;

//   // Initial support
//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(0., frame_yaw(trajectory.supports[0].frame().rotation()), 0);

//   // Trajectory to move the CoM above the foot use as support during the kick
//   TrajectoryPart part;
//   part.support = trajectory.supports[0];
//   part.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;

//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[0].frame().rotation()), 0);
//   trajectory.parts.push_back(part);

//   // Trajectory to go to the targeted flying position
//   part.support = trajectory.supports[1];
//   part.t_start = t;
//   part.swing_trajectory.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;
//   part.swing_trajectory.t_end = t;

//   part.swing_trajectory.a = Eigen::Vector3d::Zero();
//   part.swing_trajectory.b = Eigen::Vector3d::Zero();
//   part.swing_trajectory.c = T_world_target.translation() - trajectory.supports[0].footsteps[0].frame.translation();
//   part.swing_trajectory.d = trajectory.supports[0].footsteps[0].frame.translation();

//   trajectory.yaw(kicking_side).addPoint(t, frame_yaw(T_world_target.rotation()), 0);
//   _addSupports(trajectory, t, trajectory.supports[1]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[1].frame().rotation()), 0);
//   trajectory.parts.push_back(part);

//   // Trajectory of the kick
//   part.support = trajectory.supports[1];
//   part.t_start = t;
//   part.swing_trajectory.t_start = t;
//   t += parameters.kick_duration;
//   part.t_end = t;
//   part.swing_trajectory.t_end = t;

//   auto T_world_kick_end = T_world_target;
//   T_world_kick_end.translation().x() += 0.2;
//   part.swing_trajectory.a = Eigen::Vector3d::Zero();
//   part.swing_trajectory.b = T_world_kick_end.translation() - T_world_target.translation();
//   part.swing_trajectory.c = Eigen::Vector3d::Zero();
//   part.swing_trajectory.d = T_world_target.translation();
//   trajectory.parts.push_back(part);

//   // Trajectory to go back to the ground
//   part.support = trajectory.supports[1];
//   part.t_start = t;
//   part.swing_trajectory.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;
//   part.swing_trajectory.t_end = t;

//   auto T_world_end = trajectory.supports[2].footstep_frame(kicking_side);
//   part.swing_trajectory.a = Eigen::Vector3d::Zero();
//   part.swing_trajectory.b = Eigen::Vector3d::Zero();
//   part.swing_trajectory.c = T_world_end.translation() - T_world_kick_end.translation();
//   part.swing_trajectory.d = T_world_kick_end.translation();

//   _addSupports(trajectory, t, trajectory.supports[2]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[2].frame().rotation()), 0);
//   trajectory.parts.push_back(part);

//   trajectory.duration = t;
// }

// void WalkPatternGenerator::planFeetOneFoot(Trajectory& trajectory, HumanoidRobot::Side kicking_side,
//                                            Eigen::Affine3d T_world_target)
// {
//   double t = 0.;

//   // Initial support
//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(0, frame_yaw(trajectory.supports[0].frame().rotation()), 0);

//   // Trajectory to move the CoM above the foot use as support during the kick
//   TrajectoryPart part;
//   part.support = trajectory.supports[0];
//   part.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;

//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[0].frame().rotation()), 0);
//   trajectory.parts.push_back(part);

//   // Trajectory to go to the targeted flying position
//   part.support = trajectory.supports[1];
//   part.t_start = t;
//   part.swing_trajectory.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;
//   part.swing_trajectory.t_end = t;

//   part.swing_trajectory.a = Eigen::Vector3d::Zero();
//   part.swing_trajectory.b = Eigen::Vector3d::Zero();
//   part.swing_trajectory.c = T_world_target.translation() - trajectory.supports[0].footsteps[0].frame.translation();
//   part.swing_trajectory.d = trajectory.supports[0].footsteps[0].frame.translation();

//   trajectory.yaw(kicking_side).addPoint(t, frame_yaw(T_world_target.rotation()), 0);
//   _addSupports(trajectory, t, trajectory.supports[1]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[1].frame().rotation()), 0);
//   trajectory.parts.push_back(part);

//   trajectory.duration = t;
// }

// WalkPatternGenerator::Trajectory WalkPatternGenerator::plan_kick(WalkPatternGenerator::Trajectory&
// previous_trajectory,
//                                                                  double elapsed_time, HumanoidRobot::Side
//                                                                  kicking_side, Eigen::Affine3d T_world_target)
// {
//   WalkPatternGenerator::Trajectory trajectory;

//   // Update the supports followed by the walk
//   trajectory.com_height = parameters.walk_com_height;
//   trajectory.trunk_pitch = parameters.walk_trunk_pitch;

//   auto T_world_left =
//       flatten_on_floor(previous_trajectory.get_last_footstep_frame(HumanoidRobot::Side::Left, elapsed_time));
//   auto T_world_right =
//       flatten_on_floor(previous_trajectory.get_last_footstep_frame(HumanoidRobot::Side::Right, elapsed_time));

//   // Planning the supports
//   trajectory.supports = planSupportsKick(trajectory, kicking_side, T_world_left, T_world_right);

//   // Planning the center of mass trajectory
//   planCoMKick(trajectory, previous_trajectory.com.vel(elapsed_time), previous_trajectory.com.acc(elapsed_time));

//   // Planning the feet trajectories
//   planFeetKick(trajectory, kicking_side, T_world_target);

//   return trajectory;
// }

// WalkPatternGenerator::Trajectory
// WalkPatternGenerator::plan_one_foot_balance(WalkPatternGenerator::Trajectory& previous_trajectory, double
// elapsed_time,
//                                             HumanoidRobot::Side flying_side, Eigen::Affine3d T_world_target)
// {
//   WalkPatternGenerator::Trajectory trajectory;

//   // Update the supports followed by the walk
//   trajectory.com_height = parameters.walk_com_height;
//   trajectory.trunk_pitch = parameters.walk_trunk_pitch;

//   auto T_world_left =
//       flatten_on_floor(previous_trajectory.get_last_footstep_frame(HumanoidRobot::Side::Left, elapsed_time));
//   auto T_world_right =
//       flatten_on_floor(previous_trajectory.get_last_footstep_frame(HumanoidRobot::Side::Right, elapsed_time));

//   // Planning the supports
//   auto supports = planSupportsKick(trajectory, flying_side, T_world_left, T_world_right);
//   trajectory.supports = { supports[0], supports[1] };

//   // Planning the center of mass trajectory
//   planCoMOneFoot(trajectory, previous_trajectory.com.vel(elapsed_time), previous_trajectory.com.acc(elapsed_time));

//   // Planning the feet trajectories
//   planFeetOneFoot(trajectory, flying_side, T_world_target);

//   return trajectory;
// }
}  // namespace placo
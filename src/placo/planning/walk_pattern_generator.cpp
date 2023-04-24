#include "placo/planning/walk_pattern_generator.h"
#include "placo/footsteps/footsteps_planner.h"
#include "placo/utils.h"

namespace placo
{
WalkPatternGenerator::Trajectory::Trajectory() : left_foot_yaw(true), right_foot_yaw(true), trunk_yaw(true)
{
}

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
  if (parts.size() == 0)
  {
    throw std::runtime_error("Can't find a part in a trajectory that has 0 parts");
  }

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

bool WalkPatternGenerator::Trajectory::is_flying(HumanoidRobot::Side side, double t)
{
  TrajectoryPart& part = _findPart(parts, t);

  return (!part.support.is_both() && part.support.side() == HumanoidRobot::other_side(side));
}

Eigen::Affine3d WalkPatternGenerator::Trajectory::get_T_world_left(double t)
{
  TrajectoryPart& part = _findPart(parts, t);

  if (is_flying(HumanoidRobot::Left, t))
  {
    return _buildFrame(part.swing_trajectory.pos(t), left_foot_yaw.pos(t));
  }
  else
  {
    return _buildFrame(part.support.footstep_frame(HumanoidRobot::Left).translation(), left_foot_yaw.pos(t));
  }
}

Eigen::Affine3d WalkPatternGenerator::Trajectory::get_T_world_right(double t)
{
  TrajectoryPart& part = _findPart(parts, t);

  if (is_flying(HumanoidRobot::Right, t))
  {
    return _buildFrame(part.swing_trajectory.pos(t), right_foot_yaw.pos(t));
  }
  else
  {
    return _buildFrame(part.support.footstep_frame(HumanoidRobot::Right).translation(), right_foot_yaw.pos(t));
  }
}

Eigen::Vector3d WalkPatternGenerator::Trajectory::get_v_world_left(double t)
{
  TrajectoryPart& part = _findPart(parts, t);

  if (part.support.side() == HumanoidRobot::Right)
  {
    return part.swing_trajectory.vel(t);
  }
  return Eigen::Vector3d::Zero();
}

Eigen::Vector3d WalkPatternGenerator::Trajectory::get_v_world_right(double t)
{
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
  return Eigen::AngleAxisd(trunk_yaw.pos(t), Eigen::Vector3d::UnitZ()).matrix() *
         Eigen::AngleAxisd(trunk_pitch, Eigen::Vector3d::UnitY()).matrix();
}

HumanoidRobot::Side WalkPatternGenerator::Trajectory::support_side(double t)
{
  return _findPart(parts, t).support.side();
}

bool WalkPatternGenerator::Trajectory::is_both_support(double t)
{
  return _findPart(parts, t).support.is_both();
}

placo::CubicSpline& WalkPatternGenerator::Trajectory::yaw(HumanoidRobot::Side side)
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
  TrajectoryPart& part = _findPart(parts, t);
  return part.support;
}

FootstepsPlanner::Support WalkPatternGenerator::Trajectory::get_next_support(double t)
{
  TrajectoryPart& part = _findPart(parts, t);
  TrajectoryPart& next_part = _findPart(parts, part.t_end + 1e-4);
  return next_part.support;
}

FootstepsPlanner::Support WalkPatternGenerator::Trajectory::get_prev_support(double t)
{
  TrajectoryPart& part = _findPart(parts, t);
  TrajectoryPart& prev_part = _findPart(parts, part.t_start - 1e-4);
  return prev_part.support;
}

double WalkPatternGenerator::Trajectory::get_part_t_start(double t)
{
  TrajectoryPart& part = _findPart(parts, t);
  return part.t_start;
}

int WalkPatternGenerator::support_dt(FootstepsPlanner::Support& support)
{
  if (support.footsteps.size() == 1)
  {
    return parameters.single_support_timesteps;
  }

  if (support.start || support.end)
  {
    return parameters.startend_double_support_timesteps();
  }
  else
  {
    return parameters.double_support_timesteps();
    ;
  }
}

void WalkPatternGenerator::planCoM(Trajectory& trajectory, Eigen::Vector2d initial_pos, Eigen::Vector2d initial_vel,
                                   Eigen::Vector2d initial_acc, Trajectory* old_trajectory, double t_replan)
{
  // Computing how many steps are required
  int timesteps = 0;

  for (size_t i = 0; i < trajectory.supports.size(); i++)
  {
    timesteps += support_dt(trajectory.supports[i]);

    if (timesteps >= parameters.planned_timesteps)
    {
      break;
    }
  }

  trajectory.jerk_planner_timesteps = timesteps;

  // How many timesteps should be kept from the former trajectory
  int kept_timesteps = std::round((t_replan - trajectory.t_start) / parameters.dt());

  // Creating the planner
  JerkPlanner planner(timesteps, initial_pos, initial_vel, initial_acc, parameters.dt(), parameters.omega());

  if (old_trajectory != nullptr)
  {
    for (int timestep = 0; timestep < kept_timesteps; timestep++)
    {
      planner.add_equality_constraint(timestep,
                                      old_trajectory->com.jerk(trajectory.t_start + timestep * parameters.dt() + 1e-6),
                                      JerkPlanner::Jerk);
    }
  }

  // Adding ZMP constraint and reference trajectory
  int constrained_timesteps = 0;
  FootstepsPlanner::Support current_support;
  for (size_t i = 0; i < trajectory.supports.size(); i++)
  {
    current_support = trajectory.supports[i];
    int step_timesteps = support_dt(current_support);

    for (int timestep = constrained_timesteps; timestep < constrained_timesteps + step_timesteps; timestep++)
    {
      if (timestep > kept_timesteps)
        planner.add_polygon_constraint(timestep, current_support.support_polygon(), JerkPlanner::ZMP,
                                       parameters.zmp_margin);

      double y_offset = 0.;

      if (!current_support.is_both())
      {
        if (current_support.side() == HumanoidRobot::Left)
        {
          y_offset = parameters.foot_zmp_target_y;
        }
        else
        {
          y_offset = -parameters.foot_zmp_target_y;
        }
      }
      Eigen::Vector3d zmp_target = current_support.frame() * Eigen::Vector3d(parameters.foot_zmp_target_x, y_offset, 0);

      planner.add_equality_constraint(timestep, Eigen::Vector2d(zmp_target.x(), zmp_target.y()), JerkPlanner::ZMP)
          .configure(JerkPlanner::Soft, 10.);
    }

    constrained_timesteps += step_timesteps;

    if (constrained_timesteps >= timesteps)
    {
      break;
    }
  }

  // We reach the target with the given position, a null speed and a null acceleration
  if (current_support.end)
  {
    planner.add_equality_constraint(
        timesteps - 1,
        Eigen::Vector2d(current_support.frame().translation().x(), current_support.frame().translation().y()),
        JerkPlanner::Position);
    planner.add_equality_constraint(timesteps - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Velocity);
    planner.add_equality_constraint(timesteps - 1, Eigen::Vector2d(0., 0.), JerkPlanner::Acceleration);
  }

  trajectory.com = planner.plan();
  trajectory.com.t_start = trajectory.t_start;
}

static void _addSupports(WalkPatternGenerator::Trajectory& trajectory, double t, FootstepsPlanner::Support& support)
{
  for (auto footstep : support.footsteps)
  {
    auto T_world_foot = footstep.frame;
    trajectory.yaw(footstep.side).add_point(t, frame_yaw(T_world_foot.rotation()), 0);
  }
}

void WalkPatternGenerator::planFeetTrajectories(Trajectory& trajectory, Trajectory* old_trajectory, double t_replan)
{
  double t = trajectory.t_start;

  // Add the initial position to the trajectory
  _addSupports(trajectory, t, trajectory.supports[0]);

  trajectory.trunk_yaw.add_point(t, frame_yaw(trajectory.supports[0].frame().rotation()), 0);

  if (!trajectory.supports[0].is_both())
  {
    if (old_trajectory == nullptr)
    {
      throw std::runtime_error("Can't replan a swing foot starting with a single support");
    }

    // Retrieving initial flying foot yaw from old trajectory
    HumanoidRobot::Side side = HumanoidRobot::other_side(trajectory.supports[0].side());
    trajectory.yaw(side).add_point(t, old_trajectory->yaw(side).pos(t), 0);
  }

  for (size_t step = 0; step < trajectory.supports.size(); step++)
  {
    auto& support = trajectory.supports[step];

    TrajectoryPart part;
    part.support = support;
    part.t_start = t;

    if (support.footsteps.size() == 1)
    {
      // Single support, add the flying trajectory
      HumanoidRobot::Side flying_side = HumanoidRobot::other_side(support.footsteps[0].side);

      // Computing the intermediary flying foot inflection target
      Eigen::Affine3d T_world_flyingTarget = trajectory.supports[step + 1].footstep_frame(flying_side);

      t += parameters.single_support_duration;

      if (support.start)
      {
        part.swing_trajectory = _findPart(old_trajectory->parts, t_replan).swing_trajectory;
      }
      else
      {
        Eigen::Affine3d T_world_startTarget = trajectory.supports[step - 1].footstep_frame(flying_side);

        // Flying foot reaching its position
        part.swing_trajectory =
            SwingFoot::make_trajectory(t - parameters.single_support_duration, t, parameters.walk_foot_height,
                                       T_world_startTarget.translation(), T_world_flyingTarget.translation());
      }

      trajectory.yaw(flying_side).add_point(t, frame_yaw(T_world_flyingTarget.rotation()), 0);

      // The trunk orientation follow the steps orientation if there isn't double support phases
      // If there is double support phases, it follow the double supports orientation
      if (parameters.double_support_duration() < parameters.dt())
      {
        trajectory.trunk_yaw.add_point(t, frame_yaw(T_world_flyingTarget.rotation()), 0);
      }

      // Support foot remaining steady
      _addSupports(trajectory, t, support);
    }

    else
    {
      // Double support, adding the support foot at the begining and at the end of the trajectory
      if (support.start || support.end)
      {
        t += parameters.startend_double_support_duration();
      }
      else
      {
        t += parameters.double_support_duration();
      }
      _addSupports(trajectory, t, support);
      trajectory.trunk_yaw.add_point(t, frame_yaw(support.frame().rotation()), 0);
    }

    part.t_end = t;
    trajectory.parts.push_back(part);
  }

  trajectory.t_end = t;
}

WalkPatternGenerator::Trajectory WalkPatternGenerator::plan(std::vector<FootstepsPlanner::Support>& supports,
                                                            double t_start)
{
  if (supports.size() == 0)
  {
    throw std::runtime_error("Trying to plan() with 0 supports");
  }

  // Initialization of the trajectory
  Trajectory trajectory;
  trajectory.t_start = t_start;
  trajectory.com_height = parameters.walk_com_height;
  trajectory.trunk_pitch = parameters.walk_trunk_pitch;
  trajectory.supports = supports;

  // Planning the center of mass trajectory
  auto com_world = robot.com_world();
  planCoM(trajectory, Eigen::Vector2d(com_world.x(), com_world.y()));

  // Planning the footsteps trajectories
  planFeetTrajectories(trajectory);

  return trajectory;
}

WalkPatternGenerator::Trajectory WalkPatternGenerator::replan(std::vector<FootstepsPlanner::Support>& supports,
                                                              WalkPatternGenerator::Trajectory& old_trajectory,
                                                              double t_replan)
{
  if (supports.size() == 0)
  {
    throw std::runtime_error("Trying to replan() with 0 supports");
  }

  // Initialization of the new trajectory
  Trajectory trajectory;
  trajectory.com_height = parameters.walk_com_height;
  trajectory.trunk_pitch = parameters.walk_trunk_pitch;
  trajectory.supports = supports;
  trajectory.t_start = old_trajectory.get_part_t_start(t_replan);

  // Planning the center of mass trajectory
  planCoM(trajectory, old_trajectory.com.pos(trajectory.t_start), old_trajectory.com.vel(trajectory.t_start),
          old_trajectory.com.acc(trajectory.t_start), &old_trajectory, t_replan);

  // Planning the footsteps trajectories
  planFeetTrajectories(trajectory, &old_trajectory, t_replan);

  return trajectory;
}

bool WalkPatternGenerator::can_replan_supports(Trajectory& trajectory, double t_replan)
{
  // We can't replan from an "end"
  if (trajectory.get_support(t_replan).end)
  {
    return false;
  }

  // We can't replan if the support and the next support are not both single sipports
  placo::FootstepsPlanner::Support current_support = trajectory.get_support(t_replan);
  placo::FootstepsPlanner::Support next_support = trajectory.get_next_support(t_replan);

  if (current_support.is_both() || next_support.is_both())
  {
    return false;
  }

  return true;
}

std::vector<FootstepsPlanner::Support> WalkPatternGenerator::replan_supports(FootstepsPlanner& planner,
                                                                             Trajectory& trajectory, double t_replan)
{
  if (!can_replan_supports(trajectory, t_replan))
  {
    throw std::runtime_error("This trajectory can't be replanned for supports (check can_replan_supports() before)");
  }

  placo::FootstepsPlanner::Support current_support = trajectory.get_support(t_replan);
  placo::FootstepsPlanner::Support next_support = trajectory.get_next_support(t_replan);

  placo::HumanoidRobot::Side flying_side = current_support.side();

  Eigen::Affine3d T_world_left = Eigen::Affine3d::Identity();
  Eigen::Affine3d T_world_right = Eigen::Affine3d::Identity();

  if (flying_side == placo::HumanoidRobot::Left)
  {
    T_world_left = current_support.footstep_frame(placo::HumanoidRobot::Left);
    T_world_right = next_support.footstep_frame(placo::HumanoidRobot::Right);
  }
  if (flying_side == placo::HumanoidRobot::Right)
  {
    T_world_left = next_support.footstep_frame(placo::HumanoidRobot::Left);
    T_world_right = current_support.footstep_frame(placo::HumanoidRobot::Right);
  }
  auto footsteps = planner.plan(flying_side, T_world_left, T_world_right);

  std::vector<FootstepsPlanner::Support> supports;

  // if (parameters.double_support_duration > parameters.dt)
  // {
  //   supports = placo::FootstepsPlanner::make_supports(footsteps, true, true, true);
  //   placo::FootstepsPlanner::add_first_support(supports, current_support);
  // }
  // else
  // {
  // }
  supports = placo::FootstepsPlanner::make_supports(footsteps, false, false, true);

  return supports;
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
//   trajectory.trunk_yaw.addPoint(0., frame_yaw(trajectory.supports[0].frame().rotation()), 0, true);

//   // Trajectory to move the CoM above the foot use as support during the kick
//   TrajectoryPart part;
//   part.support = trajectory.supports[0];
//   part.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;

//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[0].frame().rotation()), 0, true);
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

//   trajectory.yaw(kicking_side).addPoint(t, frame_yaw(T_world_target.rotation()), 0, true);
//   _addSupports(trajectory, t, trajectory.supports[1]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[1].frame().rotation()), 0, true);
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
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[2].frame().rotation()), 0, true);
//   trajectory.parts.push_back(part);

//   trajectory.duration = t;
// }

// void WalkPatternGenerator::planFeetOneFoot(Trajectory& trajectory, HumanoidRobot::Side kicking_side,
//                                            Eigen::Affine3d T_world_target)
// {
//   double t = 0.;

//   // Initial support
//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(0, frame_yaw(trajectory.supports[0].frame().rotation()), 0, true);

//   // Trajectory to move the CoM above the foot use as support during the kick
//   TrajectoryPart part;
//   part.support = trajectory.supports[0];
//   part.t_start = t;
//   t += parameters.single_support_duration;
//   part.t_end = t;

//   _addSupports(trajectory, t, trajectory.supports[0]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[0].frame().rotation()), 0, true);
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

//   trajectory.yaw(kicking_side).addPoint(t, frame_yaw(T_world_target.rotation()), 0, true);
//   _addSupports(trajectory, t, trajectory.supports[1]);
//   trajectory.trunk_yaw.addPoint(t, frame_yaw(trajectory.supports[1].frame().rotation()), 0, true);
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
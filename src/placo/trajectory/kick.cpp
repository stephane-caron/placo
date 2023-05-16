#include "placo/trajectory/kick.h"
#include "placo/utils.h"

namespace placo
{
Eigen::Vector3d Kick::KickTrajectory::pos(double t)
{
  return foot_trajectory.pos(t);
}

Eigen::Vector3d Kick::KickTrajectory::vel(double t)
{
  return foot_trajectory.vel(t);
}

Kick::KickTrajectory Kick::make_trajectory(HumanoidRobot::Side kicking_side, double t_start, double t_end,
                                           Eigen::Vector3d start, Eigen::Vector3d target,
                                           Eigen::Vector3d support_opposite, HumanoidParameters& parameters)
{
  KickTrajectory trajectory;
  trajectory.t_start = t_start;
  trajectory.t_end = t_end;

  double t_up = t_start + parameters.kick_up_duration();
  double t_shot = t_up + parameters.kick_shot_duration();
  double t_neutral = t_shot + parameters.kick_neutral_duration();

  Eigen::Vector3d flying_neutral_position = support_opposite;
  flying_neutral_position.z() = parameters.kicking_foot_height;
  Eigen::Vector3d flying_start_position = flying_neutral_position;
  Eigen::Vector3d flying_end_position = flying_neutral_position;
  flying_start_position.x() += -0.1;
  flying_end_position.x() += 0.1;

  trajectory.foot_trajectory.add_point(t_start, start, Eigen::Vector3d::Zero());
  trajectory.foot_trajectory.add_point(t_up, flying_start_position, Eigen::Vector3d::Zero());
  trajectory.foot_trajectory.add_point(t_shot, flying_end_position, Eigen::Vector3d::Zero());
  trajectory.foot_trajectory.add_point(t_neutral, flying_neutral_position, Eigen::Vector3d::Zero());
  trajectory.foot_trajectory.add_point(t_end, target, Eigen::Vector3d::Zero());

  return trajectory;
}
}  // namespace placo
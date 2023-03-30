#pragma once

#include <Eigen/Dense>
#include "placo/model/humanoid_parameters.h"

namespace placo
{
/**
 * @brief A cubic fitting of swing foot, see:
 * https://scaron.info/doc/pymanoid/walking-pattern-generation.html#pymanoid.swing_foot.SwingFoot
 */
class SwingFoot
{
public:
  struct Trajectory
  {
    Eigen::Vector3d pos(double t);
    Eigen::Vector3d vel(double t);

    // Computed polynom (ax^3 + bx^2 + cx + d)
    Eigen::Vector3d a, b, c, d;

    double t_start, t_end;
  };

  static Trajectory make_trajectory(double t_start, double t_end, double height, Eigen::Vector3d start,
                                    Eigen::Vector3d target);

  static Trajectory remake_trajectory(Trajectory &old_trajectory, double t, Eigen::Vector3d target);

  static Trajectory make_trajectory_from_initial_velocity(double t_start, double t_end, Eigen::Vector3d start,
                                                          Eigen::Vector3d target, Eigen::Vector3d start_vel);
};
}  // namespace placo
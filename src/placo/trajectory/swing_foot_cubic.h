#pragma once

#include <Eigen/Dense>
#include "placo/model/humanoid_parameters.h"
#include "placo/trajectory/cubic_spline.h"

namespace placo
{
/**
 * @brief Cubic swing foot
 */
class SwingFootCubic
{
public:
  struct Trajectory
  {
    Eigen::Vector3d pos(double t);
    Eigen::Vector3d vel(double t);

    CubicSpline x;
    CubicSpline y;
    CubicSpline z;

    double t_start, t_end;
  };

  static Trajectory make_trajectory(double t_start, double t_end, double height, double rise_ratio,
                                    Eigen::Vector3d start, Eigen::Vector3d target);
};
}  // namespace placo
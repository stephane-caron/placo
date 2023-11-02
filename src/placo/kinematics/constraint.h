#pragma once

#include "placo/problem/problem.h"
#include "placo/tools/prioritized.h"

namespace placo::kinematics
{
class KinematicsSolver;
class Constraint : public tools::Prioritized
{
public:
  /**
   * @brief Reference to the kinematics solver
   */
  KinematicsSolver* solver = nullptr;

  virtual void add_constraint(placo::problem::Problem& problem) = 0;
};
}  // namespace placo::kinematics
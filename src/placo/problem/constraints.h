#pragma once

#include "placo/problem/constraint.h"

namespace placo
{
/**
 * @brief This is a simple wrapper that allow to regroup multiple constraints in a vector and configure
 * them all at once
 */
class ProblemConstraints
{
public:
  void configure(std::string type, double weight);
  void configure(bool hard, double weight);

  std::vector<ProblemConstraint*> constraints;
};
};  // namespace placo
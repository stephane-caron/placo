#include "footsteps_planner_repetitive.h"
#include "placo/utils.h"

namespace placo
{
FootstepsPlannerRepetitive::FootstepsPlannerRepetitive(HumanoidParameters& parameters) : FootstepsPlanner(parameters)
{
}

void FootstepsPlannerRepetitive::plan_impl(std::vector<FootstepsPlanner::Footstep>& footsteps,
                                           HumanoidRobot::Side flying_side, Eigen::Affine3d T_world_left,
                                           Eigen::Affine3d T_world_right, bool replan)
{
  Footstep footstep = footsteps[1];

  if (nb_steps > 0)
  {
    for (int steps = 0; steps < nb_steps - 1; steps += 1)
    {
      footstep = clipped_opposite_footstep(footstep, d_x, d_y, d_theta);
      footsteps.push_back(footstep);
    }

    // Adding last footstep to go double support
    footsteps.push_back(clipped_opposite_footstep(footstep));
  }
}

void FootstepsPlannerRepetitive::configure(double x, double y, double theta, int steps)
{
  d_x = x;
  d_y = y;
  d_theta = theta;
  nb_steps = steps;
}
}  // namespace placo
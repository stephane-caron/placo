#include "placo/footsteps/footsteps_planner_naive.h"
#include "placo/utils.h"
#include "rhoban_utils/history/history.h"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/polygon.hpp>

BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)
typedef boost::tuple<double, double> b_point;
typedef boost::geometry::model::polygon<b_point> b_polygon;

/**
 * TODO: We always take at least two steps here even if the target is initially
 * where we are
 * TODO: The accessibility could be refined instead of relying on one hypercube
 * TODO: How can we make sure that legs doesn't collide aech other ?
 * TODO: Feet dimensions should come from the model
 */

namespace placo
{
FootstepsPlannerNaive::FootstepsPlannerNaive(HumanoidParameters& parameters) : FootstepsPlanner(parameters)
{
}

std::vector<FootstepsPlanner::Footstep> FootstepsPlannerNaive::plan(HumanoidRobot::Side flying_side,
                                                                    Eigen::Affine3d T_world_left,
                                                                    Eigen::Affine3d T_world_right)
{
  // std::cout << "PLAN" << std::endl;
  // std::cout << "Left : " << T_world_targetLeft.translation() << std::endl;
  // std::cout << "Right : " << T_world_targetRight.translation() << std::endl;

  std::vector<FootstepsPlanner::Footstep> footsteps;

  Eigen::Affine3d T_world_target = rhoban_utils::averageFrames(T_world_targetLeft, T_world_targetRight, 0.5);

  auto T_world_currentLeft = T_world_left;
  auto T_world_currentRight = T_world_right;
  auto current_support_side = flying_side;

  bool left_arrived = false;
  bool right_arrived = false;
  int steps = 0;

  // Including initial footsteps, which are current frames
  FootstepsPlanner::Footstep footstep(parameters.foot_width, parameters.foot_length);
  footstep.side = current_support_side;
  footstep.frame = current_support_side == HumanoidRobot::Side::Left ? T_world_left : T_world_right;
  footsteps.push_back(footstep);

  current_support_side = HumanoidRobot::other_side(current_support_side);
  footstep.side = current_support_side;
  footstep.frame = current_support_side == HumanoidRobot::Side::Left ? T_world_left : T_world_right;
  footsteps.push_back(footstep);

  while ((!left_arrived || !right_arrived) && steps < max_steps)
  {
    steps += 1;

    bool arrived = true;

    // The current support in the world
    Eigen::Affine3d T_world_support =
        (current_support_side == HumanoidRobot::Side::Left) ? T_world_currentLeft : T_world_currentRight;

    // Floating foot to current frame
    Eigen::Affine3d T_support_floatingIdle = Eigen::Affine3d::Identity();
    Eigen::Affine3d T_support_center = Eigen::Affine3d::Identity();

    // Expressing the target (for current flying foot) in the support foot
    Eigen::Affine3d T_support_target =
        T_world_support.inverse() *
        ((current_support_side == HumanoidRobot::Side::Left) ? T_world_targetRight : T_world_targetLeft);

    T_support_target.translation().z() = 0.;

    if (current_support_side == HumanoidRobot::Side::Left)
    {
      T_support_floatingIdle.translation().y() = -parameters.feet_spacing;
      T_support_center.translation().y() = -parameters.feet_spacing / 2.;
    }
    else
    {
      T_support_floatingIdle.translation().y() = parameters.feet_spacing;
      T_support_center.translation().y() = parameters.feet_spacing / 2.;
    }

    // Updating the position
    Eigen::Vector3d error = T_support_target.translation() - T_support_floatingIdle.translation();

    double rescale = 1.;

    if (error.x() < -accessibility_length)
    {
      rescale = std::min(rescale, -accessibility_length / error.x());
      arrived = false;
    }
    if (error.x() > accessibility_length)
    {
      rescale = std::min(rescale, accessibility_length / error.x());
      arrived = false;
    }
    if (error.y() < -accessibility_width)
    {
      rescale = std::min(rescale, -accessibility_width / error.y());
      arrived = false;
    }
    if (error.y() > accessibility_width)
    {
      rescale = std::min(rescale, accessibility_width / error.y());
      arrived = false;
    }

    double dist = error.norm();
    error = error * rescale;

    // Updating the yaw
    double error_yaw;

    if (dist > place_threshold)
    {
      Eigen::Vector3d target_to_center =
          (T_world_support.inverse() * T_world_target).translation() - T_support_center.translation();
      error_yaw = atan2(target_to_center.y(), target_to_center.x());
    }
    else
    {
      error_yaw = placo::frame_yaw(T_support_target.rotation());
    }

    if (error_yaw < -accessibility_yaw)
    {
      arrived = false;
      error_yaw = -accessibility_yaw;
    }
    if (error_yaw > accessibility_yaw)
    {
      arrived = false;
      error_yaw = accessibility_yaw;
    }

    // Computing new frame
    Eigen::Affine3d new_step;
    new_step.translation() = T_support_floatingIdle.translation() + error;
    new_step.linear() = Eigen::AngleAxisd(error_yaw, Eigen::Vector3d::UnitZ()).toRotationMatrix();

    // Going to next step
    FootstepsPlanner::Footstep footstep(parameters.foot_width, parameters.foot_length);
    footstep.side = HumanoidRobot::other_side(current_support_side);
    footstep.frame = T_world_support * new_step;
    footsteps.push_back(footstep);

    if (current_support_side == HumanoidRobot::Side::Left)
    {
      right_arrived = arrived;
      T_world_currentRight = footstep.frame;
      current_support_side = HumanoidRobot::Side::Right;
    }
    else
    {
      left_arrived = arrived;
      T_world_currentLeft = footstep.frame;
      current_support_side = HumanoidRobot::Side::Left;
    }
  }

  return footsteps;
}

void FootstepsPlannerNaive::configure(Eigen::Affine3d T_world_left_target, Eigen::Affine3d T_world_right_target)
{
  // Targetted position for the robot
  T_world_targetLeft = T_world_left_target;
  T_world_targetRight = T_world_right_target;
}
}  // namespace placo
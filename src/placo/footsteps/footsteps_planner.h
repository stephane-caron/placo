#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <vector>
#include "placo/model/humanoid_robot.h"
#include "placo/model/humanoid_parameters.h"

namespace placo
{
class FootstepsPlanner
{
public:
  /**
   * @brief A footstep is the position of a specific foot on the ground
   */
  struct Footstep
  {
    Footstep(double foot_width, double foot_length);
    double foot_width;
    double foot_length;
    HumanoidRobot::Side side;
    Eigen::Affine3d frame;
    std::vector<Eigen::Vector2d> polygon;
    bool computed_polygon = false;

    bool operator==(const Footstep& other);

    std::vector<Eigen::Vector2d> support_polygon();
  };

  /**
   * @brief A support is a set of footsteps (can be one or two foot on the
   * ground)
   */
  struct Support
  {
    std::vector<Footstep> footsteps;
    std::vector<Eigen::Vector2d> polygon;
    bool computed_polygon = false;
    bool start = false;
    bool end = false;
    std::vector<Eigen::Vector2d> support_polygon();

    /**
     * @brief Returns the frame for the support. It will be the (interpolated)
     * average of footsteps frames
     * @return a frame
     */
    Eigen::Affine3d frame();

    /**
     * @brief Returns the frame for a given side (if present)
     * @param side the side we want the frame (left or right foot)
     * @return a frame
     */
    Eigen::Affine3d footstep_frame(HumanoidRobot::Side side);

    bool operator==(const Support& other);

    /**
     * @brief The support side (you should call is_both() to be sure it's not a double support before)
     */
    HumanoidRobot::Side side();

    /**
     * @brief Checks whether this support is a double support
     */
    bool is_both();

    /**
     * @brief Apply a transformation to a support (applied to all the footstep frames)
     */
    friend Support operator*(Eigen::Affine3d T, const Support& support);
  };

  /**
   * @brief Initializes the solver
   * @param parameters Parameters of the walk
   */
  FootstepsPlanner(HumanoidParameters& parameters);

  /**
   * @brief Generate the footsteps
   * @param flying_side first step side
   * @param T_world_left frame of the initial left foot
   * @param T_world_right frame of the initial right foot
   */
  std::vector<Footstep> plan(HumanoidRobot::Side flying_side, Eigen::Affine3d T_world_left,
                             Eigen::Affine3d T_world_right);

  /**
   * @brief Generate the supports from the footsteps
   * @param start should we add a double support at the begining of the move?
   * @param middle should we add a double support between each step ?
   * @param end should we add a double support at the end of the move?
   * @return vector of supports to use. It starts with initial double supports,
   * and add double support phases between footsteps.
   */
  static std::vector<Support> make_supports(std::vector<Footstep> footsteps, bool start = true, bool middle = false,
                                            bool end = true);

  static void add_first_support(std::vector<Support>& supports, Support support);

  /**
   * @brief Return the opposite footstep in a neutral position (i.e. at a
   * distance parameters.feet_spacing from the given footstep)
   */
  Footstep neutral_opposite_footstep(Footstep footstep, double d_x = 0., double d_y = 0., double d_theta = 0.);

  /**
   * @brief Same as neutral_opposite footstep, but the clipping is applied
   */
  Footstep clipped_neutral_opposite_footstep(Footstep footstep, double d_x = 0., double d_y = 0., double d_theta = 0.);

  /**
   * @brief Return the frame between the feet in the neutral position
   * @param footstep The footstep of one of the feet
   */
  Eigen::Affine3d neutral_frame(Footstep footstep);

  Footstep create_footstep(HumanoidRobot::Side side, Eigen::Affine3d T_world_foot);

  // Humanoid parameters for planning and control
  HumanoidParameters& parameters;

protected:
  virtual void plan_impl(std::vector<Footstep>&, HumanoidRobot::Side flying_side, Eigen::Affine3d T_world_left,
                         Eigen::Affine3d T_world_right) = 0;
};
}  // namespace placo
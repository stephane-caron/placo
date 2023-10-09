#pragma once

#include <Eigen/Dense>
#include "placo/model/robot_wrapper.h"
#include "placo/problem/problem.h"
#include "placo/control/axises_mask.h"

namespace placo
{
struct GravityTorques
{
public:
  struct Result
  {
    // Checks if the gravity computation is a success
    bool success;

    // Torques computed by the solver
    Eigen::VectorXd tau;
  };

  struct Contact
  {
    std::string frame_name = "";

    enum Type
    {
      Planar = 0,
      Point = 1
    };

    Type type = Point;

    // Configures the contact
    void configure(const std::string& frame_name, Type type, double mu = 1., double length = 0., double width = 0.);

    // For planar contacts, the length and width of the contact rectangle
    // Length is along x axis in local frame, and width along y axis
    double length = 0.;
    double width = 0.;

    // Friction coefficient
    double mu = 1.;

    // Weights for optimization
    double weight_forces = 1.0;
    double weight_moments = 1.0;

    // Adds the wrench to the problem
    Expression add_wrench(RobotWrapper& robot, Problem& problem);

    // Wrench computed by the solver
    Eigen::MatrixXd wrench;
    Variable* variable;
  };

  struct LoopClosure
  {
    std::string frame_a;
    std::string frame_b;
    AxisesMask mask;
  };

  GravityTorques(RobotWrapper& robot);
  virtual ~GravityTorques();

  // Contacts
  std::vector<Contact*> contacts;

  // Passive joints
  std::set<std::string> passive_joints;

  /**
   * @brief Adds a contact to the solver
   * @param contact the contact to add
   */
  Contact& add_contact();

  /**
   * @brief Sets a DoF as passive
   * @param joint_name the DoF name
   * @param is_passive true if the DoF should be passive
   */
  void set_passive(const std::string& joint_name, bool is_passive = true);

  /**
   * @brief Adds a loop closing constraint (xy should be zero)
   * @param frame_a
   * @param frame_b
   */
  void add_loop_closing_constraint(const std::string& frame_a, const std::string& frame_b, const std::string& axises);

  /**
   * @brief Computes the torques required to compensate gravity given a set of unilateral contacts. This
   * formulates and try to solve a QP problem with the following properties:
   *
   * - Objective function:
   *   - Trying to minimize the moments at contact (as a result, ZMP is tried to be kept as much as
   *     possible at the center of the contact)
   *   - Trying to minimize the required torques
   * - Constraints:
   *   - Equation of motion: tau + sum(J^T f) = g
   *   - The contact fz are positive (contacts are unilaterals)
   *   - The ZMP is kept in the admissible rectangles (using foot_length and foot_width)
   *   - Friction cones using the given mu ratio
   *
   * (In the future, this API might change in favour of more versatile contacts representation)
   *
   * @param robot robot wrapper
   * @param contacts list of frames which are unitaleral contacts
   * @param contact_length contact rectangles length (you might consider some margin)
   * @param contact_width contact rectangles width (you might consider some margin)
   * @param mu friction coefficient
   * @return
   */
  Result compute();

protected:
  RobotWrapper& robot;
  std::vector<LoopClosure> loop_closing_constraints;
};
}  // namespace placo
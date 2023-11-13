#pragma once

#include <Eigen/Dense>
#include <set>

#include "placo/model/robot_wrapper.h"

// Tasks
#include "placo/kinematics/task.h"
#include "placo/kinematics/position_task.h"
#include "placo/kinematics/orientation_task.h"
#include "placo/kinematics/frame_task.h"
#include "placo/kinematics/relative_position_task.h"
#include "placo/kinematics/relative_orientation_task.h"
#include "placo/kinematics/relative_frame_task.h"
#include "placo/kinematics/com_task.h"
#include "placo/kinematics/distance_task.h"
#include "placo/kinematics/joints_task.h"
#include "placo/kinematics/gear_task.h"
#include "placo/kinematics/regularization_task.h"
#include "placo/kinematics/centroidal_momentum_task.h"

// Constraints
#include "placo/kinematics/constraint.h"
#include "placo/kinematics/avoid_self_collisions_constraint.h"
#include "placo/kinematics/com_polygon_constraint.h"

// Problem formulation
#include "placo/problem/problem.h"

namespace placo::kinematics
{
/**
 * @brief Inverse Kinematics solver
 */
class KinematicsSolver
{
public:
  KinematicsSolver(model::RobotWrapper& robot_);
  virtual ~KinematicsSolver();

  /**
   * @brief Adds a position task
   * @param frame the robot frame we want to control
   * @param target_world the target position, expressed in the world (as T_world_frame)
   * @return position task
   * @pyignore
   */
  PositionTask& add_position_task(model::RobotWrapper::FrameIndex frame, Eigen::Vector3d target_world);

  /**
   * @brief Adds a position task
   * @param frame the robot frame we want to control
   * @param target_world the target position, expressed in the world (as T_world_frame)
   * @return position task
   */
  PositionTask& add_position_task(std::string frame, Eigen::Vector3d target_world);

  /**
   * @brief Adds a relative position task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param target the target vector between frame a and b (expressed in world)
   * @return relative position task
   * @pyignore
   */
  RelativePositionTask& add_relative_position_task(model::RobotWrapper::FrameIndex frame_a,
                                                   model::RobotWrapper::FrameIndex frame_b, Eigen::Vector3d target);

  /**
   * @brief Adds a relative position task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param target the target vector between frame a and b (expressed in world)
   * @return relative position task
   */
  RelativePositionTask& add_relative_position_task(std::string frame_a, std::string frame_b, Eigen::Vector3d target);

  /**
   * @brief Adds a com position task
   * @param targetCom_world the target position, expressed in the world (as T_world_frame)
   * @return com task
   */
  CoMTask& add_com_task(Eigen::Vector3d targetCom_world);

  /**
   * @brief Adds an orientation task
   * @param frame the robot frame we want to control
   * @param R_world_frame the target orientation we want to achieve, expressed in the world (as T_world_frame)
   * @return orientation task
   * @pyignore
   */
  OrientationTask& add_orientation_task(model::RobotWrapper::FrameIndex frame, Eigen::Matrix3d R_world_frame);

  /**
   * @brief Adds an orientation task
   * @param frame the robot frame we want to control
   * @param R_world_frame the target orientation we want to achieve, expressed in the world (as T_world_frame)
   * @return orientation task
   */
  OrientationTask& add_orientation_task(std::string frame, Eigen::Matrix3d R_world_frame);

  /**
   * @brief Adds a relative orientation task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param R_a_b the desired orientation
   * @return relative orientation task
   * @pyignore
   */
  RelativeOrientationTask& add_relative_orientation_task(model::RobotWrapper::FrameIndex frame_a,
                                                         model::RobotWrapper::FrameIndex frame_b,
                                                         Eigen::Matrix3d R_a_b);

  /**
   * @brief Adds a relative orientation task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param R_a_b the desired orientation
   * @return relative orientation task
   */
  RelativeOrientationTask& add_relative_orientation_task(std::string frame_a, std::string frame_b,
                                                         Eigen::Matrix3d R_a_b);

  /**
   * @brief Adds a frame task, this is equivalent to a position + orientation task, resulting in decoupled tasks for
   * a given frame
   * @param frame the robot frame we want to control
   * @param T_world_frame the target for the frame we want to control, expressed in the world (as T_world_frame)
   * @param priority task priority (hard: equality constraint, soft: objective function)
   * @return frame task
   * @pyignore
   */
  FrameTask add_frame_task(model::RobotWrapper::FrameIndex frame,
                           Eigen::Affine3d T_world_frame = Eigen::Affine3d::Identity());

  /**
   * @brief Adds a frame task, this is equivalent to a position + orientation task, resulting in decoupled tasks for
   * a given frame
   * @param frame the robot frame we want to control
   * @param T_world_frame the target for the frame we want to control, expressed in the world (as T_world_frame)
   * @param priority task priority (hard: equality constraint, soft: objective function)
   * @return frame task
   */
  FrameTask add_frame_task(std::string frame, Eigen::Affine3d T_world_frame = Eigen::Affine3d::Identity());

  /**
   * @brief Adds a relative frame task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param T_a_b desired transformation
   * @return relative frame task
   * @pyignore
   */
  RelativeFrameTask add_relative_frame_task(model::RobotWrapper::FrameIndex frame_a,
                                            model::RobotWrapper::FrameIndex frame_b, Eigen::Affine3d T_a_b);

  /**
   * @brief Adds a relative frame task
   * @param frame_a frame a
   * @param frame_b frame b
   * @param T_a_b desired transformation
   * @return relative frame task
   */
  RelativeFrameTask add_relative_frame_task(std::string frame_a, std::string frame_b, Eigen::Affine3d T_a_b);

  /**
   * @brief Adds joints task
   * @param joints value for the joints
   * @return joints task
   */
  JointsTask& add_joints_task(std::map<std::string, double>& joints);

  /**
   * @brief Adds joints task
   * @return joints task
   */
  JointsTask& add_joints_task();

  /**
   * @brief Adds a gear task, allowing replication of joints
   * @return gear task
   */
  GearTask& add_gear_task();

  /**
   * @brief Adds a distance task to be maintained between two frames
   * @param frame_a frame a
   * @param frame_b frame b
   * @param distance distance to maintain
   * @return distance task
   * @pyignore
   */
  DistanceTask& add_distance_task(model::RobotWrapper::FrameIndex frame_a, model::RobotWrapper::FrameIndex frame_b,
                                  double distance);

  /**
   * @brief Adds a distance task to be maintained between two frames
   * @param frame_a frame a
   * @param frame_b frame b
   * @param distance distance to maintain
   * @return distance task
   */
  DistanceTask& add_distance_task(std::string frame_a, std::string frame_b, double distance);

  /**
   * @brief Adding a centroidal momentum task
   * @param L_world desired centroidal angular momentum in the world
   * @return centroidal task
   */
  CentroidalMomentumTask& add_centroidal_momentum_task(Eigen::Vector3d L_world);

  /**
   * @brief Adds a regularization task for a given magnitude
   * @param magnitude regularization magnitude
   * @return regularization task
   */
  RegularizationTask& add_regularization_task(double magnitude = 1e-6);

  /**
   * @brief Adds a self collision avoidance constraint
   * @return constraint
   */
  AvoidSelfCollisionsConstraint& add_avoid_self_collisions_constraint();

  /**
   * @brief Adds a CoM polygon constraint
   * @param polygon clockwise polygon
   * @param margin margin
   * @return constraint
   */
  CoMPolygonConstraint& add_com_polygon_constraint(std::vector<Eigen::Vector2d> polygon, double margin = 0.);

  /**
   * @brief Constructs the QP problem and solves it
   * @param apply apply the solution to the robot model
   * @return the vector containing delta q, which are target variations for the robot degrees of freedom.
   */
  Eigen::VectorXd solve(bool apply = false);

  /**
   * @brief Masks (disables a DoF) from being used by the QP solver (it can't provide speed)
   * @param dof the dof name
   */
  void mask_dof(std::string dof);

  /**
   * @brief Unmsks (enables a DoF) from being used by the QP solver (it can provide speed)
   * @param dof the dof name
   */
  void unmask_dof(std::string dof);

  /**
   * @brief Decides if the floating base should be masked
   */
  void mask_fbase(bool masked);

  /**
   * @brief Clears the internal tasks
   */
  void clear();

  /**
   * @brief Retrieve a copy of the set of tasks
   */
  std::set<Task*> get_tasks();

  /**
   * @brief Removes a task from the solver
   * @param task task
   */
  void remove_task(Task& task);

  /**
   * @brief Removes a frame task from the solver
   * @param task task
   */
  void remove_task(FrameTask& task);

  /**
   * @brief Removes aconstraint from the solver
   * @param constraint constraint
   */
  void remove_constraint(Constraint& constraint);

  /**
   * @brief Dumps the status to a given stream
   */
  void dump_status_stream(std::ostream& stream);

  /**
   * @brief Shows the tasks status
   */
  void dump_status();

  /**
   * @brief Enables/disables joint limits inequalities
   */
  void enable_joint_limits(bool enable);

  /**
   * @brief Enables/disables joint velocity inequalities
   */
  void enable_velocity_limits(bool enable);

  /**
   * @brief Number of tasks
   */
  int tasks_count();

  /**
   * @brief The robot controlled by this solver
   */
  model::RobotWrapper& robot;

  /**
   * @brief Size of the problem (number of variables)
   */
  int N;

  /**
   * @brief Some configuration noise added before solving
   */
  double noise = 1e-4;

  /**
   * @brief solver dt (for speeds limiting)
   */
  double dt = 0.;

  /**
   * @brief scale obtained when using tasks scaling
   */
  double scale = 0.;

  /**
   * @brief whether the optimisation requires scaling
   */
  bool has_scaling = false;

  /**
   * @brief The underlying QP problem
   */
  problem::Problem problem;

  /**
   * @brief Adds a custom task to the solver
   * @param task
   */
  void add_task(Task& task);

  /**
   * @brief Adds a custom constraint to the solver
   * @param constraint
   */
  void add_constraint(Constraint& constraint);

  template <typename T>
  T& add_task(T* task)
  {
    task_id += 1;
    task->solver = this;
    task->solver_memory = true;
    std::ostringstream oss;
    oss << "Task_" << task_id;
    task->name = oss.str();
    tasks.insert(task);

    return *task;
  }

  template <typename T>
  T& add_constraint(T* constraint)
  {
    constraint_id += 1;
    constraint->solver = this;
    constraint->solver_memory = true;
    std::ostringstream oss;
    oss << "Constraint_" << constraint_id;
    constraint->name = oss.str();
    constraints.insert(constraint);

    return *constraint;
  }

protected:
  problem::Variable* qd = nullptr;
  problem::Variable* scale_variable = nullptr;

  std::set<int> masked_dof;
  bool masked_fbase;
  std::set<Task*> tasks;
  std::set<Constraint*> constraints;

  Eigen::VectorXi activeSet;
  size_t activeSetSize;

  // Modes to limit the DoFs
  bool joint_limits = true;
  bool velocity_limits = false;

  void compute_limits_inequalities();

  // Task id (this is only useful when task names are not specified, each task will have an unique ID)
  int task_id = 0;
  int constraint_id = 0;
};
}  // namespace placo::kinematics
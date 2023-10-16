#include "placo/dynamics/relative_position_task.h"
#include "placo/dynamics/dynamics_solver.h"

namespace placo
{
namespace dynamics
{

RelativePositionTask::RelativePositionTask(RobotWrapper::FrameIndex frame_a_index,
                                           RobotWrapper::FrameIndex frame_b_index, Eigen::Vector3d target)
{
  this->frame_a_index = frame_a_index;
  this->frame_b_index = frame_b_index;
  this->target = target;
}

void RelativePositionTask::update()
{
  // Transformation matrices
  Eigen::Affine3d w_T_a = solver->robot.get_T_world_frame(frame_a_index);
  Eigen::Affine3d w_T_b = solver->robot.get_T_world_frame(frame_b_index);
  Eigen::Matrix3d a_R_w = w_T_a.rotation().transpose();

  // AB vector expressed in world and in a
  Eigen::Vector3d w_AB = w_T_a.translation() - w_T_b.translation();
  Eigen::Vector3d a_AB = a_R_w * w_AB;

  // Computing J and dJ for frame_a and frame_b
  Eigen::MatrixXd Ja = solver->robot.frame_jacobian(frame_a_index, pinocchio::ReferenceFrame::LOCAL_WORLD_ALIGNED);
  Eigen::MatrixXd dJa =
      solver->robot.frame_jacobian_time_variation(frame_a_index, pinocchio::ReferenceFrame::LOCAL_WORLD_ALIGNED);

  Eigen::MatrixXd Jb = solver->robot.frame_jacobian(frame_b_index, pinocchio::ReferenceFrame::LOCAL_WORLD_ALIGNED);
  Eigen::MatrixXd dJb =
      solver->robot.frame_jacobian_time_variation(frame_b_index, pinocchio::ReferenceFrame::LOCAL_WORLD_ALIGNED);

  // Rotation velocity of frame a in the world frame
  Eigen::Vector3d w_omega_a = Ja.block(3, 0, 3, solver->N) * solver->robot.state.qd;
  Eigen::Vector3d a_omega_w = -a_R_w * w_omega_a;

  // Velocity of the error expressed in a
  Eigen::Vector3d w_dAB = (Ja.block(0, 0, 3, solver->N) - (Jb.block(0, 0, 3, solver->N))) * solver->robot.state.qd;
  Eigen::Vector3d a_dAB = a_omega_w.cross(a_AB) + w_dAB;

  // Computing error
  Eigen::Vector3d error = target - a_AB;
  Eigen::Vector3d derror = Eigen::Vector3d::Zero() - a_dAB;
  Eigen::Vector3d desired_acceleration = kp * error + 2 * sqrt(kp) * derror;

  // The acceleration of AB in a is expressed as: J * ddq + e
  Eigen::MatrixXd J = pinocchio::skew(a_AB) * a_R_w * Ja.block(3, 0, 3, solver->N);
  J += a_R_w * (dJb.block(0, 0, 3, solver->N) - dJa.block(0, 0, 3, solver->N));

  Eigen::Vector3d e = 2 * pinocchio::skew(a_omega_w) * a_R_w * w_dAB;
  e += pinocchio::skew(a_omega_w) * pinocchio::skew(a_omega_w) * a_AB;
  e += a_R_w * (Jb.block(3, 0, 3, solver->N) - Ja.block(3, 0, 3, solver->N)) * solver->robot.state.qd;
  e += pinocchio::skew(a_omega_w) * a_R_w * Ja.block(3, 0, 3, solver->N) * solver->robot.state.qd;

  A = J;
  b = -e + desired_acceleration;
}

std::string RelativePositionTask::type_name()
{
  return "relative_position";
}

std::string RelativePositionTask::error_unit()
{
  return "m";
}
}  // namespace dynamics
}  // namespace placo
#include "placo/control/relative_orientation_task.h"
#include "placo/control/kinematics_solver.h"

namespace placo
{
RelativeOrientationTask::RelativeOrientationTask(RobotWrapper::FrameIndex frame_a, RobotWrapper::FrameIndex frame_b,
                                                 Eigen::Matrix3d R_a_b)
  : frame_a(frame_a), frame_b(frame_b), R_a_b(R_a_b)
{
}

void RelativeOrientationTask::update()
{
  auto T_world_a = solver->robot->get_T_world_frame(frame_a);
  auto T_world_b = solver->robot->get_T_world_frame(frame_b);
  auto T_a_b = T_world_a.inverse() * T_world_b;

  // (R_a_b* R_a_b^{-1}) R_a_b = R_a_b*
  // |-----------------|
  //         | This part is the world error that "correct" the rotation
  //           matrix to the desired one
  Eigen::Vector3d error = pinocchio::log3(R_a_b * T_a_b.linear().inverse());

  Eigen::MatrixXd J_a = solver->robot->frame_jacobian(frame_a, pinocchio::WORLD);
  Eigen::MatrixXd J_b = solver->robot->frame_jacobian(frame_b, pinocchio::WORLD);
  Eigen::MatrixXd J_ab = pinocchio::SE3(T_world_a.inverse().matrix()).toActionMatrix() * (J_b - J_a);

  A = J_ab.block(3, 0, 3, solver->N);
  b = error;
}

std::string RelativeOrientationTask::type_name()
{
  return "relative_orientation";
}

std::string RelativeOrientationTask::error_unit()
{
  return "rad";
}
}  // namespace placo
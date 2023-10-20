#include "placo/dynamics/dynamics_solver.h"
#include "placo/problem/problem.h"

namespace placo::dynamics
{
void DynamicsSolver::set_passive(const std::string& joint_name, bool is_passive, double kp, double kd)
{
  if (!is_passive)
  {
    passive_joints.erase(joint_name);
  }
  else
  {
    PassiveJoint pj;
    pj.kp = kp;
    pj.kd = kd;
    passive_joints[joint_name] = pj;
  }
}

PointContact& DynamicsSolver::add_point_contact(PositionTask& position_task)
{
  return add_contact(new PointContact(position_task, false));
}

PointContact& DynamicsSolver::add_unilateral_point_contact(PositionTask& position_task)
{
  return add_contact(new PointContact(position_task, true));
}

RelativePointContact& DynamicsSolver::add_relative_point_contact(RelativePositionTask& position_task)
{
  return add_contact(new RelativePointContact(position_task));
}

PlanarContact& DynamicsSolver::add_planar_contact(FrameTask& frame_task)
{
  return add_contact(new PlanarContact(frame_task, true));
}

PlanarContact& DynamicsSolver::add_fixed_contact(FrameTask& frame_task)
{
  return add_contact(new PlanarContact(frame_task, false));
}

ExternalWrenchContact& DynamicsSolver::add_external_wrench_contact(pinocchio::FrameIndex frame_index)
{
  return add_contact(new ExternalWrenchContact(frame_index));
}

ExternalWrenchContact& DynamicsSolver::add_external_wrench_contact(std::string frame_name)
{
  return add_external_wrench_contact(robot.get_frame_index(frame_name));
}

PuppetContact& DynamicsSolver::add_puppet_contact()
{
  return add_contact(new PuppetContact());
}

TaskContact& DynamicsSolver::add_task_contact(Task& task)
{
  return add_contact(new TaskContact(task));
}

PositionTask& DynamicsSolver::add_position_task(pinocchio::FrameIndex frame_index, Eigen::Vector3d target_world)
{
  return add_task(new PositionTask(frame_index, target_world));
}

PositionTask& DynamicsSolver::add_position_task(std::string frame_name, Eigen::Vector3d target_world)
{
  return add_position_task(robot.get_frame_index(frame_name), target_world);
}

RelativePositionTask& DynamicsSolver::add_relative_position_task(pinocchio::FrameIndex frame_a_index,
                                                                 pinocchio::FrameIndex frame_b_index,
                                                                 Eigen::Vector3d target_world)
{
  return add_task(new RelativePositionTask(frame_a_index, frame_b_index, target_world));
}

RelativePositionTask& DynamicsSolver::add_relative_position_task(std::string frame_a_name, std::string frame_b_name,
                                                                 Eigen::Vector3d target_world)
{
  return add_relative_position_task(robot.get_frame_index(frame_a_name), robot.get_frame_index(frame_b_name),
                                    target_world);
}

CoMTask& DynamicsSolver::add_com_task(Eigen::Vector3d target_world)
{
  return add_task(new CoMTask(target_world));
}

void DynamicsSolver::set_static(bool is_static_)
{
  is_static = is_static_;
}

JointsTask& DynamicsSolver::add_joints_task()
{
  return add_task(new JointsTask());
}

MimicTask& DynamicsSolver::add_mimic_task()
{
  return add_task(new MimicTask());
}

OrientationTask& DynamicsSolver::add_orientation_task(pinocchio::FrameIndex frame_index, Eigen::Matrix3d R_world_frame)
{
  return add_task(new OrientationTask(frame_index, R_world_frame));
}

OrientationTask& DynamicsSolver::add_orientation_task(std::string frame_name, Eigen::Matrix3d R_world_frame)
{
  return add_orientation_task(robot.get_frame_index(frame_name), R_world_frame);
}

FrameTask DynamicsSolver::add_frame_task(pinocchio::FrameIndex frame_index, Eigen::Affine3d T_world_frame)
{
  PositionTask& position = add_position_task(frame_index, T_world_frame.translation());
  OrientationTask& orientation = add_orientation_task(frame_index, T_world_frame.rotation());

  return FrameTask(&position, &orientation);
}

FrameTask DynamicsSolver::add_frame_task(std::string frame_name, Eigen::Affine3d T_world_frame)
{
  return add_frame_task(robot.get_frame_index(frame_name), T_world_frame);
}

DynamicsSolver::DynamicsSolver(RobotWrapper& robot) : robot(robot)
{
  N = robot.model.nv;
}

DynamicsSolver::~DynamicsSolver()
{
  for (auto& contact : contacts)
  {
    delete contact;
  }
  for (auto& task : tasks)
  {
    delete task;
  }
}

void DynamicsSolver::enable_joint_limits(bool enable)
{
  joint_limits = enable;
}

void DynamicsSolver::enable_velocity_limits(bool enable)
{
  velocity_limits = enable;
}

void DynamicsSolver::enable_velocity_vs_torque_limits(bool enable)
{
  velocity_limits = enable;
  velocity_vs_torque_limits = enable;
}

void DynamicsSolver::enable_torque_limits(bool enable)
{
  torque_limits = enable;
}

void DynamicsSolver::enable_self_collision_avoidance(bool enable, double margin, double trigger)
{
  avoid_self_collisions = enable;
  self_collisions_margin = margin;
  self_collisions_trigger = trigger;
}

void DynamicsSolver::configure_self_collision_avoidance(bool soft, double weight)
{
  self_collisions_soft = soft;
  self_collisions_weight = weight;
}

void DynamicsSolver::compute_limits_inequalities(Expression& tau)
{
  if ((joint_limits || velocity_limits) && dt == 0.)
  {
    throw std::runtime_error("DynamicsSolver::compute_limits_inequalities: dt is not set");
  }

  std::set<int> passive_ids;
  for (auto& passive_joint : passive_joints)
  {
    passive_ids.insert(robot.get_joint_v_offset(passive_joint.first));
  }

  if (torque_limits)
  {
    problem.add_constraint(tau.slice(6) <= robot.model.effortLimit.bottomRows(N - 6));
    problem.add_constraint(tau.slice(6) >= -robot.model.effortLimit.bottomRows(N - 6));
  }

  if (is_static)
  {
    return;
  }

  int constraints = 0;
  if (joint_limits)
  {
    constraints += 2 * (N - 6 - passive_joints.size());
  }
  if (velocity_limits)
  {
    constraints += 2 * (N - 6 - passive_joints.size());
  }

  if (constraints > 0)
  {
    Expression e;
    e.A = Eigen::MatrixXd(constraints, problem.n_variables);
    e.A.setZero();
    e.b = Eigen::VectorXd(constraints);
    int constraint = 0;

    // Iterating for each actuated joints
    for (int k = 0; k < N - 6; k++)
    {
      if (passive_ids.count(k + 6) > 0)
      {
        continue;
      }
      double q = robot.state.q[k + 7];
      double qd = robot.state.qd[k + 6];

      if (velocity_limits)
      {
        if (torque_limits && velocity_vs_torque_limits)
        {
          double ratio = robot.model.velocityLimit[k + 6] / robot.model.effortLimit[k + 6];

          // qd + dt*qdd <= qd_max - ratio * tau
          // ratio * tau + dt*qdd + qd - qd_max <= 0
          e.A.block(constraint, 0, 1, problem.n_variables) = ratio * tau.A.block(k + 6, 0, 1, problem.n_variables);
          e.b[constraint] = ratio * tau.b[k + 6];
          e.A(constraint, k + 6) += dt;
          e.b[constraint] += qd - robot.model.velocityLimit[k + 6];
          constraint++;

          // qd + dt*qdd >= -qd_max - ratio * tau
          // -ratio*tau - dt*qdd - qd - qd_max <= 0
          e.A.block(constraint, 0, 1, problem.n_variables) = -ratio * tau.A.block(k + 6, 0, 1, problem.n_variables);
          e.b[constraint] = -ratio * tau.b[k + 6];
          e.A(constraint, k + 6) -= dt;
          e.b[constraint] -= qd + robot.model.velocityLimit[k + 6];
          constraint++;
        }
        else
        {
          e.A(constraint, k + 6) = dt;
          e.b(constraint) = -robot.model.velocityLimit[k + 6] + qd;
          constraint++;

          e.A(constraint, k + 6) = -dt;
          e.b(constraint) = -robot.model.velocityLimit[k + 6] - qd;
          constraint++;
        }
      }

      if (joint_limits)
      {
        double qdd_safe = 1.;  // XXX: This should be specified somewhere else

        if (q > robot.model.upperPositionLimit[k + 7])
        {
          // We are in the contact, ensuring at least
          // qdd <= -qdd_safe
          e.A(constraint, k + 6) = 1;
          e.b(constraint) = qdd_safe;
        }
        else
        {
          // qdd*dt + qd <= qd_max
          double qd_max = sqrt(2. * (robot.model.upperPositionLimit[k + 7] - q) * qdd_safe);
          e.A(constraint, k + 6) = dt;
          e.b(constraint) = qd - qd_max;
        }
        constraint++;

        if (q < robot.model.lowerPositionLimit[k + 7])
        {
          // We are in the contact, ensuring at least
          // qdd >= qdd_safe
          e.A(constraint, k + 6) = -1;
          e.b(constraint) = qdd_safe;
        }
        else
        {
          // qdd*dt + qd >= -qd_max
          double qd_max = sqrt(2. * fabs(robot.model.lowerPositionLimit[k + 7] - q) * qdd_safe);
          e.A(constraint, k + 6) = -dt;
          e.b(constraint) = -qd - qd_max;
        }
        constraint++;
      }
    }

    problem.add_constraint(e <= 0);
  }
}

void DynamicsSolver::compute_self_collision_inequalities()
{
  if (avoid_self_collisions)
  {
    std::vector<RobotWrapper::Distance> distances = robot.distances();

    int constraints = 0;

    for (auto& distance : distances)
    {
      if (distance.min_distance < self_collisions_trigger)
      {
        constraints += 1;
      }
    }

    if (constraints == 0)
    {
      return;
    }

    Expression e;
    e.A = Eigen::MatrixXd(constraints, N);
    e.b = Eigen::VectorXd(constraints);
    int constraint = 0;

    double xdd_safe = 1.;  // XXX: How to estimate this ?

    for (auto& distance : distances)
    {
      if (distance.min_distance < self_collisions_trigger)
      {
        Eigen::Vector3d v = distance.pointB - distance.pointA;
        Eigen::Vector3d n = v.normalized();

        if (distance.min_distance < 0)
        {
          // If the distance is negative, the points "cross" and this vector should point the other way around
          n = -n;
        }

        Eigen::MatrixXd X_A_world = pinocchio::SE3(Eigen::Matrix3d::Identity(), -distance.pointA).toActionMatrix();
        Eigen::MatrixXd JA = X_A_world * robot.joint_jacobian(distance.parentA, pinocchio::ReferenceFrame::WORLD);

        Eigen::MatrixXd X_B_world = pinocchio::SE3(Eigen::Matrix3d::Identity(), -distance.pointB).toActionMatrix();
        Eigen::MatrixXd JB = X_B_world * robot.joint_jacobian(distance.parentB, pinocchio::ReferenceFrame::WORLD);

        // We want: current_distance + J dq >= margin
        Eigen::MatrixXd J = n.transpose() * (JB - JA).block(0, 0, 3, N);

        if (distance.min_distance >= self_collisions_margin)
        {
          // We prevent excessive velocity towards the collision
          double error = distance.min_distance - self_collisions_margin;
          double xd = (J * robot.state.qd)(0, 0);
          double xd_max = sqrt(2. * error * xdd_safe);

          e.A.block(constraint, 0, 1, N) = dt * J;
          e.b[constraint] = xd + xd_max;
        }
        else
        {
          // We push outward the collision
          e.A.block(constraint, 0, 1, N) = J;
          e.b[constraint] = -xdd_safe;
        }

        constraint += 1;
      }
    }

    problem.add_constraint(e >= 0).configure(self_collisions_soft ? ProblemConstraint::Soft : ProblemConstraint::Hard,
                                             self_collisions_weight);
  }
}

void DynamicsSolver::clear_tasks()
{
  for (auto& task : tasks)
  {
    delete task;
  }
  tasks.clear();
}

void DynamicsSolver::dump_status_stream(std::ostream& stream)
{
  stream << "* Dynamics Tasks:" << std::endl;
  if (is_static)
  {
    std::cout << "  * Solver is static (qdd is 0)" << std::endl;
  }
  for (auto task : tasks)
  {
    task->update();
    stream << "  * " << task->name << " [" << task->type_name() << "]" << std::endl;
    stream << "    - Priority: ";
    if (task->priority == Task::Priority::Hard)
    {
      stream << "hard";
    }
    else
    {
      stream << "soft (weight:" << task->weight << ")";
    }
    stream << std::endl;
    char buffer[128];
    sprintf(buffer, "    - Error: %.06f [%s]\n", task->error.norm(), task->error_unit().c_str());
    stream << buffer << std::endl;
    sprintf(buffer, "    - DError: %.06f [%s]\n", task->derror.norm(), task->error_unit().c_str());
    stream << buffer << std::endl;
  }
}

void DynamicsSolver::dump_status()
{
  dump_status_stream(std::cout);
}

DynamicsSolver::Result DynamicsSolver::solve()
{
  DynamicsSolver::Result result;
  std::vector<Variable*> contact_wrenches;

  problem.clear_constraints();
  problem.clear_variables();

  Expression qdd;
  if (is_static)
  {
    qdd = Expression::from_vector(Eigen::VectorXd::Zero(robot.model.nv));
  }
  else
  {
    Variable& qdd_varaible = problem.add_variable(robot.model.nv);
    qdd = qdd_varaible.expr();
  }

  for (auto& task : tasks)
  {
    task->update();

    if (!is_static)
    {
      ProblemConstraint::Priority task_priority = ProblemConstraint::Hard;
      if (task->priority == Task::Priority::Soft)
      {
        task_priority = ProblemConstraint::Soft;
      }

      Expression e;
      e.A = task->A;
      e.b = -task->b;
      problem.add_constraint(e == 0).configure(task_priority, task->weight);
    }
  }

  // We build the expression for tau, given the equation of motion
  // tau = M qdd + b - J^T F

  // M qdd
  Expression tau = robot.mass_matrix() * qdd + robot.state.qd * friction;

  // b
  tau = tau + robot.non_linear_effects();

  // J^T F
  // Computing body jacobians
  for (auto& contact : contacts)
  {
    Contact::Wrench wrench = contact->add_wrench(problem);
    tau = tau - wrench.J.transpose() * wrench.f;
  }

  compute_limits_inequalities(tau);
  compute_self_collision_inequalities();

  // Floating base has no torque
  problem.add_constraint(tau.slice(0, 6) == 0);

  // Passive joints have no torque
  for (auto& entry : passive_joints)
  {
    std::string joint = entry.first;
    PassiveJoint pj = entry.second;
    double q = robot.get_joint(joint);
    double qd = robot.get_joint_velocity(joint);
    double target_tau = q * pj.kp + qd * pj.kd;

    problem.add_constraint(tau.slice(robot.get_joint_v_offset(joint), 1) == target_tau);
  }

  // We want to minimize torques
  problem.add_constraint(tau == 0).configure(ProblemConstraint::Soft, 1.);

  try
  {
    // Solving the QP
    problem.solve();
    result.success = true;

    // Exporting result values
    result.tau = tau.value(problem.x);
    result.qdd = qdd.value(problem.x);
  }
  catch (QPError& e)
  {
    result.success = false;
  }

  return result;
}

void DynamicsSolver::remove_task(Task& task)
{
  tasks.erase(&task);

  delete &task;
}

void DynamicsSolver::remove_contact(Contact& contact)
{
  // Removing the contact from the vector
  contacts.erase(std::remove(contacts.begin(), contacts.end(), &contact), contacts.end());

  delete &contact;
}
}  // namespace placo::dynamics
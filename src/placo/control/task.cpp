#include "placo/control/task.h"

namespace placo
{
Task::Priority priority_from_string(std::string priority)
{
  if (priority == "soft")
  {
    return Task::Priority::Soft;
  }
  else if (priority == "hard")
  {
    return Task::Priority::Hard;
  }
  else if (priority == "scaled")
  {
    return Task::Priority::Scaled;
  }
  else
  {
    throw std::runtime_error(std::string("KinematicsSolver: Invalid priority: ") + priority);
  }
}

Task::Task()
{
  solver = nullptr;
  priority = Soft;
  weight = 1.0;
}

Task::~Task()
{
}

void Task::set_priority_value(Priority priority_)
{
  priority = priority_;
}

void Task::set_priority(std::string priority)
{
  priority = priority_from_string(priority);
}

void Task::set_weight(double weight_)
{
  weight = weight_;
}

void Task::set_name(std::string name_)
{
  name = name_;
}

void Task::configure(std::string name_, std::string priority_, double weight_)
{
  name = name_;
  priority = priority_from_string(priority_);
  weight = weight_;
}

void Task::configure(std::string name_, Priority priority_, double weight_)
{
  name = name_;
  priority = priority_;
  weight = weight_;
}

std::string Task::priority_name()
{
  switch (priority)
  {
    case Hard:
      return "hard";
    case Soft:
      return "soft";
    case Scaled:
      return "scaled";
    default:
      throw std::runtime_error("Task::priority_name: Invalid priority");
  }
}

Eigen::MatrixXd Task::error()
{
  return b;
}

double Task::normalized_error()
{
  return b.norm();
}
}  // namespace placo
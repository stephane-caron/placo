#include <map>
#include <chrono>
#include "placo/problem/problem.h"
#include "placo/problem/qp_error.h"
#include "eiquadprog/eiquadprog.hpp"

namespace placo
{
Problem::Problem()
{
}

Problem::~Problem()
{
  for (auto constraint : constraints)
  {
    delete constraint;
  }

  for (auto variable : variables)
  {
    delete variable;
  }

  constraints.clear();
  variables.clear();
}

Variable& Problem::add_variable(int size)
{
  Variable* variable = new Variable;
  variable->k_start = n_variables;
  variable->k_end = n_variables + size;
  n_variables += size;

  variables.push_back(variable);

  return *variable;
}

void Problem::add_limit(Expression expression, Eigen::VectorXd target)
{
  // -target <= expression <= target
  add_constraint(-target <= expression);
  add_constraint(expression <= target);
}

ProblemConstraint& Problem::add_constraint(const ProblemConstraint& constraint_)
{
  ProblemConstraint* constraint = new ProblemConstraint;
  *constraint = constraint_;
  constraints.push_back(constraint);

  return *constraint;
}

ProblemConstraints Problem::add_constraints(const std::vector<ProblemConstraint>& constraints)
{
  ProblemConstraints problem_constraints;
  for (auto& constraint : constraints)
  {
    problem_constraints.constraints.push_back(&add_constraint(constraint));
  }
  return problem_constraints;
}

void Problem::clear_constraints()
{
  for (auto constraint : constraints)
  {
    delete constraint;
  }

  constraints.clear();
}

void Problem::clear_variables()
{
  for (auto variable : variables)
  {
    delete variable;
  }

  variables.clear();
  n_variables = 0;
}

void Problem::solve()
{
  int n_equalities = 0;
  int n_inequalities = 0;
  int slack_variables = 0;

  for (auto constraint : constraints)
  {
    if (constraint->inequality)
    {
      constraint->is_active = false;
      if (constraint->priority == ProblemConstraint::Soft)
      {
        slack_variables += constraint->expression.rows();
      }
    }
    else
    {
      if (constraint->priority == ProblemConstraint::Hard)
      {
        n_equalities += constraint->expression.rows();
      }

      constraint->is_active = true;
    }
  }

  // Equality constraints
  Eigen::MatrixXd A(n_equalities, n_variables + slack_variables);
  Eigen::VectorXd b(n_equalities);
  A.setZero();
  b.setZero();
  int k_equality = 0;

  for (auto constraint : constraints)
  {
    if (!constraint->inequality && constraint->priority == ProblemConstraint::Hard)
    {
      // Ax + b = 0
      A.block(k_equality, 0, constraint->expression.rows(), constraint->expression.cols()) = constraint->expression.A;
      b.block(k_equality, 0, constraint->expression.rows(), 1) = constraint->expression.b;
      k_equality += constraint->expression.rows();
    }
  }

  int qp_variables = n_variables;

  int rewriting_variables = 0;
  // XXX: Handle memory better
  Eigen::ColPivHouseholderQR<Eigen::Matrix<double, -1, -1, 1, -1, -1>>* QR = nullptr;
  Eigen::MatrixXd y;
  if (rewrite_equalities && A.rows() > 0)
  {
    // Computing QR decomposition of A.T
    QR = new Eigen::ColPivHouseholderQR<Eigen::Matrix<double, -1, -1, 1, -1, -1>>(A.transpose().colPivHouseholderQr());

    int rank = QR->rank();  // XXX: Remove rank variable

    // A = Q * R * P.inverse()
    Eigen::MatrixXd R = QR->matrixR().transpose().block(0, 0, rank, rank);

    Eigen::MatrixXd b2 = b.transpose();
    QR->colsPermutation().applyThisOnTheRight(b2);
    b2.transposeInPlace();

    y = R.triangularView<Eigen::Lower>().solve(-b2);

    qp_variables = n_variables - rank;
    rewriting_variables = rank;

    // Removing equality constraints
    A.resize(0, 0);
    b.resize(0);
  }

  Eigen::MatrixXd P(qp_variables + slack_variables, qp_variables + slack_variables);
  Eigen::VectorXd q(qp_variables + slack_variables);

  P.setZero();
  q.setZero();

  // Adding regularization
  // XXX: The user variables should maybe not be regularized by default?
  double epsilon = 1e-8;

  // If equalities are not rewritten, regularizing x
  P.block(0, 0, qp_variables, qp_variables).setIdentity();
  P.block(0, 0, qp_variables, qp_variables) *= epsilon;

  // Scanning the constraints (counting inequalities and equalities, building objectif function)
  for (auto constraint : constraints)
  {
    if (constraint->expression.cols() > n_variables)
    {
      throw QPError("Problem: Inconsistent problem size");
    }
    if (constraint->expression.A.rows() == 0 || constraint->expression.b.rows() == 0)
    {
      throw QPError("Problem: A or b is empty");
    }
    if (constraint->expression.A.rows() != constraint->expression.b.rows())
    {
      throw QPError("Problem: A.rows() != b.rows()");
    }

    if (constraint->inequality)
    {
      // If the constraint is hard, this will be the true inequality, else, this will be the inequality
      // enforcing the slack variable to be >= 0
      n_inequalities += constraint->expression.rows();
    }
    else if (constraint->priority == ProblemConstraint::Soft)
    {
      Eigen::MatrixXd expression_A = constraint->expression.A;
      Eigen::VectorXd expression_b = constraint->expression.b;

      if (rewriting_variables)
      {
        expression_A.conservativeResize(expression_A.rows(), n_variables);
        expression_A
            .block(0, constraint->expression.A.cols(), expression_A.rows(),
                   n_variables - constraint->expression.A.cols())
            .setZero();
        QR->matrixQ().applyThisOnTheRight(expression_A);

        expression_b = constraint->expression.b + expression_A.leftCols(rewriting_variables) * y;
        expression_A = expression_A.rightCols(qp_variables);
      }

      // Adding the soft constraint to the objective function
      if (use_sparsity)
      {
        Sparsity sparsity = Sparsity::detect_columns_sparsity(expression_A);

        int constraints = expression_A.rows();

        for (auto interval : sparsity.intervals)
        {
          int size = 1 + interval.end - interval.start;

          Eigen::MatrixXd block = expression_A.block(0, interval.start, constraints, size);

          P.block(interval.start, interval.start, size, size).noalias() +=
              constraint->weight * block.transpose() * block;
        }

        q.block(0, 0, expression_A.cols(), 1).noalias() +=
            constraint->weight * (expression_A.transpose() * expression_b);
      }
      else
      {
        int n = expression_A.cols();
        P.block(0, 0, n, n).noalias() += constraint->weight * (expression_A.transpose() * expression_A);
        q.block(0, 0, n, 1).noalias() += constraint->weight * (expression_A.transpose() * expression_b);
      }
    }
  }

  // Inequality constraints
  Eigen::MatrixXd G(n_inequalities, qp_variables + slack_variables);
  Eigen::VectorXd h(n_inequalities);
  G.setZero();
  h.setZero();

  // Used to keep track of the hard/soft inequalities constraints
  // The hard mapping maps index from inequality row to constraint, and the soft
  // mapping maps index from slack variables to the constraint.
  std::map<int, ProblemConstraint*> hard_inequalities_mapping;
  std::map<int, ProblemConstraint*> soft_inequalities_mapping;

  int k_inequality = 0;
  int k_slack = 0;

  // Slack variables should be positive
  for (int slack = 0; slack < slack_variables; slack += 1)
  {
    // s_i >= 0
    G(k_inequality, qp_variables + slack) = 1;
    k_inequality += 1;
  }

  for (auto constraint : constraints)
  {
    if (constraint->inequality)
    {
      Eigen::MatrixXd expression_A = constraint->expression.A;
      Eigen::VectorXd expression_b = constraint->expression.b;

      if (rewriting_variables)
      {
        expression_A.conservativeResize(expression_A.rows(), n_variables);
        expression_A
            .block(0, constraint->expression.A.cols(), expression_A.rows(),
                   n_variables - constraint->expression.A.cols())
            .setZero();
        QR->matrixQ().applyThisOnTheRight(expression_A);

        expression_b = constraint->expression.b + expression_A.leftCols(rewriting_variables) * y;
        expression_A = expression_A.rightCols(qp_variables);
      }

      if (constraint->priority == ProblemConstraint::Hard)
      {
        // Ax + b >= 0
        G.block(k_inequality, 0, expression_A.rows(), expression_A.cols()) = expression_A;
        h.block(k_inequality, 0, expression_b.rows(), 1) = expression_b;

        for (int k = k_inequality; k < k_inequality + expression_A.rows(); k++)
        {
          hard_inequalities_mapping[k] = constraint;
        }
        k_inequality += expression_A.rows();
      }
      else
      {
        // min(Ax + b - s)
        // A slack variable is assigend with all "soft" inequality and a minimization is added to the problem
        Eigen::MatrixXd As(expression_A.rows(), qp_variables + slack_variables);
        As.setZero();
        As.block(0, 0, expression_A.rows(), expression_A.cols()) = expression_A;

        for (int k = 0; k < expression_A.rows(); k++)
        {
          soft_inequalities_mapping[k_slack] = constraint;
          As(k, qp_variables + k_slack) = -1;
          k_slack += 1;
        }

        P.noalias() += constraint->weight * (As.transpose() * As);
        q.noalias() += constraint->weight * (As.transpose() * expression_b);
      }
    }
  }

  Eigen::VectorXi active_set;
  size_t active_set_size;

  // std::cout << "QP variables: " << qp_variables << " / " << n_variables << std::endl;
  // std::cout << "Inequalities: " << n_inequalities << std::endl;
  // std::cout << "Equalities: " << A.rows() << std::endl;

  x = Eigen::VectorXd(qp_variables + slack_variables);
  x.setZero();
  double result =
      eiquadprog::solvers::solve_quadprog(P, q, A.transpose(), b, G.transpose(), h, x, active_set, active_set_size);

  if (rewriting_variables)
  {
    Eigen::VectorXd u(n_variables, 1);
    u.setZero();
    u.topRows(rewriting_variables) = y;
    u.bottomRows(qp_variables) = x;
    QR->matrixQ().applyThisOnTheLeft(u);
    x = u;
  }

  if (QR != nullptr)
  {
    delete QR;
  }

  // Checking that the problem is indeed feasible
  if (result == std::numeric_limits<double>::infinity())
  {
    throw QPError("Problem: Infeasible QP (check your hard inequality constraints)");
  }

  // Checking that equality constraints were enforced, since this is not covered by above result
  if (A.rows() > 0)
  {
    // XXX: This is not compliant with soft inequality slacks
    Eigen::VectorXd equality_constraints = A * x + b;
    for (int k = 0; k < A.rows(); k++)
    {
      if (fabs(equality_constraints[k]) > 1e-6)
      {
        throw QPError("Problem: Infeasible QP (equality constraints were not enforced)");
      }
    }
  }

  // Checking for NaNs in solution
  if (x.hasNaN())
  {
    throw QPError("Problem: NaN in the QP solution");
  }

  // Reporting on the active constraints
  for (int k = 0; k < active_set_size; k++)
  {
    int active_constraint = active_set[k];

    if (active_constraint >= 0 && hard_inequalities_mapping.count(active_constraint))
    {
      hard_inequalities_mapping[active_constraint]->is_active = true;
    }
  }

  slacks = x.block(qp_variables, 0, slack_variables, 1);
  for (int k = 0; k < slacks.rows(); k++)
  {
    if (slacks[k] <= 1e-6 && soft_inequalities_mapping.count(k))
    {
      soft_inequalities_mapping[k]->is_active = true;
    }
  }

  for (auto variable : variables)
  {
    variable->version += 1;
    variable->value = Eigen::VectorXd(variable->size());
    variable->value = x.block(variable->k_start, 0, variable->size(), 1);
  }
}

};  // namespace placo
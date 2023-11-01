#pragma once

#include <stdexcept>

namespace placo
{
/**
 * @brief Exception raised by \ref Problem in case of failure
 */
class QPError : public std::runtime_error
{
public:
  QPError(std::string message = "");
};
}  // namespace placo
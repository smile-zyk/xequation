#pragma once
#include <string>
#include <unordered_set>

#include "value.h"

namespace xequation
{
class ExprContext
{
  public:
    // Checks if key exists.
    virtual bool Contains(const std::string &key) const = 0;

    // Gets value for the given key.
    virtual Value Get(const std::string &key) const = 0;

    // Sets value for the given key.
    virtual void Set(const std::string &key, const Value &value) = 0;

    // Removes the given key.
    virtual bool Remove(const std::string &key) = 0;

    // Clears all entries.
    virtual void Clear() = 0;

    // Returns all keys in the context.
    virtual std::unordered_set<std::string> keys() const = 0;

    // Returns the number of entries.
    virtual size_t size() const
    {
      return keys().size();
    }

    // Checks if the dictionary is empty.
    virtual bool empty() const
    {
      return keys().size() == 0;
    }
};
} // namespace xequation
#pragma once

#include <sol.hpp>

namespace effil {

struct IInterruptable;

namespace this_thread {

void setNotifier(IInterruptable* notifier);
void interruptionPoint();

// Lua API
std::string threadId();
void yield();
void sleep(const sol::stack_object& duration, const sol::stack_object& metric);

} // namespace this_thread
} // namespace effil

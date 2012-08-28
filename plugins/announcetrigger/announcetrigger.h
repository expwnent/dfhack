#pragma once

#include "Core.h"
#include "ColorText.h"

#include <string>

void registerTrigger(const char* condition, DFHack::command_result (*function) (std::string&, DFHack::color_ostream&), DFHack::command_result (*periodic)(DFHack::color_ostream&) = 0);



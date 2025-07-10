#pragma once

#include <Babylon/Graphics/Device.h>
#include <Babylon/Polyfills/Console.h>

const char* EnumToString(Babylon::Polyfills::Console::LogLevel logLevel);
int RunTests(const Babylon::Graphics::Configuration& config);

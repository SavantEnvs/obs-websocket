#pragma once

struct Config;
class WebSocketServer;

void HarnessEnvInit();
void HarnessObsStart();
void HarnessObsStop();
void HarnessRegisterTypes();
Config &HarnessConfig();
WebSocketServer &HarnessServer();

#include <string>

void HarnessFrontendStart(const std::string &scratchDir);
void HarnessFrontendStop();


extern "C" void HarnessTickFrames(void);
extern "C" void HarnessVideoStart(void);
extern "C" void HarnessVideoStop(void);

#pragma once
#include <string>

// Train on a command (call after every execute)
void predictor_train(const std::string& cmd);

// Get best prediction for current input prefix, returns "" if none
std::string predictor_suggest(const std::string& input);

// Save/load model to disk
void predictor_save(const std::string& path);
void predictor_load(const std::string& path);

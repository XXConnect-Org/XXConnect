#pragma once
#include <cstdint>
#include <vector>

#include "ISavingWorker.hpp"

class WavWorker : public ISavingWorker {
public:
    WavWorker(const std::string& filename) : _filename(filename) {}
    bool Save() override;
private:
    std::string _filename;
};



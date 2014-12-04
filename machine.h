#ifndef MACHINE_H
#define MACHINE_H

#include <simlib.h>
#include <string>
#include <sstream>

class Machine : public Facility
{
public:
    Machine() : Facility(), _fullName() {}

    void SetNameWithNum(const std::string& name, int num)
    {
        std::stringstream ss;
        ss << num;
        _fullName = name;
        _fullName += ' ';
        _fullName += ss.str();
        SetName(_fullName.c_str());
    }

    virtual ~Machine() {}

private:
    std::string _fullName;
};

#endif // MACHINE_H

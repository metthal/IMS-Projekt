#ifndef LINE_H
#define LINE_H

#include <simlib.h>
#include <string>
#include <sstream>

class Line : public Store
{
public:
    Line() : Store(), _fullName() {}

    void SetNameWithNum(const std::string& name, int num)
    {
        std::stringstream ss;
        ss << num;
        _fullName = name;
        _fullName += ' ';
        _fullName += ss.str();
        SetName(_fullName.c_str());
    }

    virtual ~Line() {}

private:
    std::string _fullName;
};

#endif // LINE_H


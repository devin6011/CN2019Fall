#pragma once

#include <cctype>
#include <string>
#include <algorithm>

bool string2int(const std::string &str, int &v) {
    if(not std::all_of(begin(str), end(str),
                       static_cast<int(*)(int)>(std::isdigit)))
        return false;

    try {
        v = std::stoi(str);
    }
    catch(...) {
        return false;
    }

    return true;
}

bool string2ushort(const std::string &str, unsigned short &v) {
    int temp;
    if(not string2int(str, temp))
        return false;

    if(temp < 0 or temp > 65535)
        return false;

    v = static_cast<unsigned short>(temp);
    return true;
}

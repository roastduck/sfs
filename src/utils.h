#ifndef UTILS_H_
#define UTILS_H_

#include <iostream>

#define UNUSED(x) ((void)(x)) // Mark a variable as unused to make the compiler happy

extern std::ostream *logPtr;
#define LOG (*logPtr)

#endif // UTILS_H_


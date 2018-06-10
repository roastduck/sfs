#ifndef MANGLE_H_
#define MANGLE_H_

#include <string>

extern const std::string path_mangle_prefix;
extern const std::size_t path_mangle_len;

std::string path_mangle(const std::string &path);
std::string path_demangle(const std::string &path);
void test_mangle();

#endif // MANGLE_H_


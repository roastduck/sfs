#include <cassert>
#include <sstream>
#include "mangle.h"

const std::string path_mangle_prefix = "$";
const std::size_t path_mangle_len = path_mangle_prefix.length();

// TODO(twd2): performance improvement

std::string path_mangle(const std::string &path)
{
    std::stringstream newpath;
    std::size_t len = path.length();
    std::stringstream part_stream;
    for (std::size_t i = 0; i <= len; i++)
    {
        if (i == len || path[i] == '/')
        {
            std::string part = part_stream.str();
            part_stream.str("");
            if (part == "" || part == "." || part == "..")
            {
                newpath << part;
            }
            else
            {
                newpath << path_mangle_prefix << part;
            }
            if (i < len)
            {
                newpath.put('/');
            }
        }
        else
        {
            part_stream.put(path[i]);
        }
    }
    return newpath.str();
}

std::string path_demangle(const std::string &path)
{
    std::stringstream newpath;
    std::size_t len = path.length();
    std::stringstream part_stream;
    for (std::size_t i = 0; i <= len; i++)
    {
        if (i == len || path[i] == '/')
        {
            std::string part = part_stream.str();
            std::size_t part_len = part.length();
            part_stream.str("");
            if (part == "" || part == "." || part == "..")
            {
                newpath << part;
            }
            else if (part_len <= path_mangle_len ||
                     part.substr(0, path_mangle_len) != path_mangle_prefix)
            {
                return "";
            }
            else
            {
                newpath << part.substr(path_mangle_len);
            }
            if (i < len)
            {
                newpath.put('/');
            }
        }
        else
        {
            part_stream.put(path[i]);
        }
    }
    return newpath.str();
}

#ifndef NDEBUG
void test_mangle()
{
    // embedded tests
    assert(path_mangle("/path/to/.git") == "/$path/$to/$.git");
    assert(path_mangle("/") == "/");
    assert(path_mangle("///123") == "///$123");
    assert(path_mangle("file") == "$file");
    assert(path_mangle("/./a") == "/./$a");
    assert(path_mangle("/../a") == "/../$a");
    assert(path_mangle("/.../a") == "/$.../$a");
    assert(path_demangle("/$path/$to/$.git") == "/path/to/.git");
    assert(path_demangle("/") == "/");
    assert(path_demangle("///$123") == "///123");
    assert(path_demangle("$file") == "file");
    assert(path_demangle("/./$a") == "/./a");
    assert(path_demangle("/../$a") == "/../a");
    assert(path_demangle("/$.../$a") == "/.../a");
    assert(path_demangle("/$path/file") == "");
    assert(path_demangle("") == "");
    assert(path_demangle("/path/file") == "");
    assert(path_demangle("$///") == "");
    assert(path_demangle("$123///") == "123///");
}
#endif


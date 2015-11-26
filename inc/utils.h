#ifndef UTILITIES_H_
#define UTILITIES_H_
#include "string"
#include "vector"
using namespace std;

std::vector<std::string> split(const std::string &s, char delim);
void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);


#endif
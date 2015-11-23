#ifndef UTILITIES_H_
#define UTILITIES_H_
#include "string"
#include "vector"
using namespace std;

vector<string> split(const string& s, const char& delimiter);
void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);


#endif
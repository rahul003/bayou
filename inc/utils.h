#ifndef UTILITIES_H_
#define UTILITIES_H_
#include "string"
#include "vector"
#include "unordered_map"
using namespace std;

std::vector<std::string> split(const std::string &s, char delim);
void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);
int GetPortFromFd(int fd);
int GetPeerPortFromFd(int fd);
unordered_map<int, int> StringToUnorderedMap(string str);

#endif
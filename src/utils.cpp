#include "../inc/utils.h"
#include "string"
#include "vector"
#include "iostream"
#include "sstream"
using namespace std;
#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

vector<string> split(const std::string& s, const char& delimiter)
{
	vector<string> parts;
    std::stringstream ss(s);
    std::string temp;
    while (getline(ss, temp, delimiter))
    	parts.push_back(temp);
}

/**
 * creates a new thread
 * @param  f function pointer to be passed to the new thread
 * @param  arg arguments for the function f passed to the thread
 * @param  thread thread identifier for new thread
 */
 void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread) {
    if (pthread_create(&thread, NULL, f, arg)) {
        D(cout << "U " << ": ERROR: Unable to create thread" << endl;)
        pthread_exit(NULL);
    }
}
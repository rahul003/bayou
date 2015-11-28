#ifndef DATA_CLS_H
#define DATA_CLS_H
#include "string"
#include "set"
#include "map"
using namespace std;

class IdTuple{
public:
	IdTuple(int csn, string sname, int ts);
	IdTuple();
	bool operator<(const IdTuple &t2) const;
	bool operator>(const IdTuple &t2) const;
	bool operator==(const IdTuple &t2) const;
	bool operator<=(const IdTuple &t2) const;
	bool operator>=(const IdTuple &t2) const;

	string as_string();

	int get_csn() const;
	int get_accept_ts() const;
	string get_sname() const;
    
    void set_csn(int csn);
private:
	int csn_;
	string s_name_;
	int accept_ts_;
};

class Command{
public:
	Command(string, string, string);
	Command(string, string);
	Command();
	Command(string);
	string as_string();
	string get_type();
	string get_song();
	string get_url();
private:
	string type_;
	string song_; 
	string url_; 
};


class Graph{
public:
	Graph();
    void AddNode(int);
    void RemoveNode(int);
    void RemoveEdge(int, int);
    void AddEdge(int, int);
	set<set<int> > GetConnectedComponents();

private:
    std::map<int, set<int> > adj_;
};


template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
  template <> struct hash<IdTuple>
  {
    inline size_t operator()(const IdTuple & t) const
    {
      size_t seed = 0;
      hash_combine(seed, t.get_csn());
      hash_combine(seed, t.get_sname());
      hash_combine(seed, t.get_accept_ts());
      return seed;
    }
  };
}

#endif
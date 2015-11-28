#include "../inc/data_classes.h"
#include "../inc/constants.h"
#include "unordered_map"
#include "stack"
IdTuple::IdTuple(int csn, string sname, int ts)
{
    csn_ = csn;
    s_name_ = sname;
    accept_ts_ = ts;
}
IdTuple::IdTuple()
{

}

bool IdTuple::operator<(const IdTuple &t2) const {
    if(this->csn_<t2.csn_)
        return true;
    else if(this->csn_>t2.csn_)
        return false;
    else if(this->csn_==t2.csn_)
    {
        if(this->s_name_<t2.s_name_)
            return true;
        else if(this->s_name_ > t2.s_name_)
            return false;
        else if(this->s_name_== t2.s_name_)
        {
            if(this->accept_ts_<t2.accept_ts_)
                return true;
            else
                return false;
        }
    }
}
bool IdTuple::operator==(const IdTuple &t2) const {
    return this->csn_==t2.csn_ && this->s_name_==t2.s_name_ && this->accept_ts_==t2.accept_ts_;
}
bool IdTuple::operator<=(const IdTuple &t2) const {
    return ((*this) == t2 || (*this) < t2);
}
bool IdTuple::operator>(const IdTuple &t2) const {
    return !((*this) <= t2 );
}
bool IdTuple::operator>=(const IdTuple &t2) const {
    return !((*this) < t2 );
}

int IdTuple::get_csn() const{
    return csn_;
}
int IdTuple::get_accept_ts() const{
    return accept_ts_;
}
string IdTuple::get_sname() const{
    return s_name_;
}
string IdTuple::as_string(){
    string m;
    m+=to_string(csn_)+kInternalWriteDelim;
    m+=s_name_+kInternalWriteDelim;
    m+=to_string(accept_ts_)+kInternalWriteDelim;
    return m;
}

void IdTuple::set_csn(int csn) {
    csn_ = csn;
}

/////////////////////////////////////////////////////////////////

Command::Command(string a, string b, string c)
{
    type_ = a;
    song_ = b;
    url_ = c;
}
Command::Command(string a, string b)
{
    type_ = a;
    song_ = b;
}
Command::Command(){
    
}
Command::Command(string type){
    type_ = type;
}
string Command::as_string(){
    string m;
    m+=type_+kInternalWriteDelim;
    m+=song_+kInternalWriteDelim;
    m+=url_+kInternalWriteDelim;
    return m;
}
string Command::get_type(){
    return type_;
}
string Command::get_song(){
    return song_;
}
string Command::get_url(){
    return url_;
}
/////////////////////////////////////////////////////
Graph::Graph(){

}
void Graph::AddNode(int id){
    //add this node to every node already in graph
    //add vector with every node in graph to map
    set<int> new_set;
    for(auto &node: adj_)
    {
        new_set.insert(node.first);            
        node.second.insert(id);
    }
    adj_[id] = new_set;
}
void Graph::RemoveNode(int id){
    adj_.erase(id);
    for(auto &n: adj_)
    {
        n.second.erase(id);
    }
}
void Graph::RemoveEdge(int id1, int id2){       
    adj_[id1].erase(id2);
    adj_[id2].erase(id1);
}
void Graph::AddEdge(int id1, int id2){
    adj_[id1].insert(id2);
    adj_[id2].insert(id1);
}
set<set<int> > Graph::GetConnectedComponents(){
    set<set<int> > rval;
    //initialize visited map
    unordered_map<int, bool> visited;
    for(auto &n : adj_)
    {
        visited[n.first] = false;
    }
    stack<int> to_explore;

    for(auto &n: visited)
    {   
        if(n.second)
            continue; //already visited

        set<int> one_comp;
        to_explore.push(n.first);
        while(!to_explore.empty())
        {
            int cur = to_explore.top();
            to_explore.pop();
            visited[cur] = true;
            one_comp.insert(cur);
            for (auto &neighb : adj_[cur])
            {
                if(!visited[neighb])
                    to_explore.push(neighb);
            }
        }
        rval.insert(one_comp);
    }        

    return rval;
}
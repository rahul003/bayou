#include <iostream>
#include <map>

int main ()
{
  std::map<char,int> mymap;
  std::map<char,int>::iterator it;
  std::map<char,int>::reverse_iterator rit;

  // insert some values:
  mymap['a']=10;
  mymap['b']=20;
  mymap['c']=30;
  mymap['d']=40;
  mymap['e']=50;
  mymap['f']=60;

  for (it=mymap.begin(); it!=mymap.end(); ++it)
  {
    std::cout << it->first << " => " << it->second << '\n';
  }

  // show content:
  int c=0;
  for (rit=mymap.rbegin(); rit!=mymap.rend(); rit)
  {
    std::cout <<"erasing" <<rit->first << " => " << rit->second << '\n';
    mymap.erase(rit->first);
    c++;
    if (c==2)
      break;
  }

  for (it=mymap.begin(); it!=mymap.end(); ++it)
  {
    std::cout << it->first << " => " << it->second << '\n';
  }
  return 0;
}
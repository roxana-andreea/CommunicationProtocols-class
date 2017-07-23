#ifndef _CLIENT_
#define _CLIENT_
#include <string>
#include <fstream>
using namespace std;

class Client {
 private:
    string name, ip, time_stamp, dir, port;
 public:
    Client() {
    }

    Client(const string& name, const string& ip, const string& dir, const string& port, const string& time_stamp) :
        name(name), ip(ip), time_stamp(time_stamp), dir(dir), port(port) { }

    string get_name();
    string get_ip();
    string get_dir();
    string get_port();
    string get_time_stamp();
    friend ostream& operator << (ostream&, const Client&);  
};

string Client::get_name() { return name; }
string Client::get_ip() { return ip; }
string Client::get_time_stamp() { return time_stamp; }
string Client::get_dir() { return dir; }
string Client::get_port() { return port; }

ostream& operator << (ostream& out, const Client& cl) {
    out << cl.name << ' ' << cl.ip << ' ' << cl.port << ' ' << cl.time_stamp;
    return out;
}

#endif

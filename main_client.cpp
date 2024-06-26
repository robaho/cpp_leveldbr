#include "client.h"
#include "exceptions.h"
#include <stdexcept>

int main(int argc,char **argv) {
    try {
        RemoteDatabase::remove("localhost:8501","main",0);
    } catch (DatabaseNotFound& ex) {}

    auto db = RemoteDatabase::open("localhost:8501","main",true,0);
    std::cout << "db opened!\n";
    db->put("my key","my value");
    db->put("my key2","my value2");
    std::cout << "put value!\n";
    auto value = db->get("my key");
    if(value!="my value") throw std::runtime_error("invalid value");
    std::cout << "read value!\n";

    auto itr = db->lookup("","");

    auto kv = itr.next();
    if(kv.key!="my key") throw std::runtime_error("invalid lookup key");
    if(kv.value!="my value") throw std::runtime_error("invalid lookup value");
    kv = itr.next();
    if(kv.key!="my key2") throw std::runtime_error("invalid lookup key");
    if(kv.value!="my value2") throw std::runtime_error("invalid lookup value");

    db->close();
}
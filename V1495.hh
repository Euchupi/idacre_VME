#ifndef _V1495_HH_
#define _V1495_HH_

#include <memory>
#include <map>

class MongoLog;
class Options;

class V1495{

  public:
    V1495(std::shared_ptr<MongoLog>&, std::shared_ptr<Options>&, int, int, unsigned);
    virtual ~V1495();
    virtual int Arm(std::map<std::string, int>&);
    // Functions for a child class to implement
    virtual int BeforeSINStart() {return 0;}
    virtual int AfterSINStart() {return 0;}
    virtual int BeforeSINStop() {return 0;}
    virtual int AfterSINStop() {return 0;}

  protected:
    int WriteReg(unsigned int, unsigned int);
    int fBoardHandle, fBID;
    unsigned int fBaseAddress;
    std::shared_ptr<Options> fOptions;
    std::shared_ptr<MongoLog> fLog;
};
#endif

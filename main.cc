#include <iostream>
#include <fstream>
/* 
iostream and fstream is to output the variables 
sudo apt install libcxxtools-dev , dev is short for developper
*/

#include <csignal>
/*
csingal library is to handle the basic signal functions 
void (*signal(int sig, void (*func)(int)))(int)
int sig includes SIGABRT, SIGFPE,SIGILL , SIGINT , SIGSEGV, SIGTERM and will execute the func when the sig changes
*/

#include <unistd.h>
/*
unistd.h is to handle the API between the redax and the POSIX (Linux) , also related to the commandline args 
*/

#include <getopt.h>
/*
getopt is to get the string from the command line 
*/

#include <chrono>
/*
Chrono Library is to log the time / date information just like the time module of Python 
*/

#include <thread>
#include <atomic>
/* 
The thread library is to allow building multiple threads softwares . 
Atomic library defines a type called atomic so that we could handle it multithreading .
(For example ,  the int64 running on a 32bits machine )
In principle , we should always define the global variables via the atomic library's atomic type . 
*/

#include "DAQController.hh"
#include "CControl_Handler.hh"
#include "MongoLog.hh"
#include "Options.hh"
/*
The most important c source files of redax are the DAQControl , CControl , MongoLog , Options 
*/

#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
/*
MongoCXX is to write the files into the mongo database 
uri - client - instance - database - collectionis the data structure of the mongodb 
The pool is about the basic types of the mongo database . 
These header files could be installed via the mongocxx-driver downloaded from the mongo official website . 
*/

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
//Bson is a useful file format mainly used by the mongodb . 









#ifndef REDAX_BUILD_COMMIT
#define REDAX_BUILD_COMMIT "unknown"
#endif
// REDAX_Build_commit means that whether or not the build has been committed 
int PrintVersion() {
  std::cout << "PrintVersion: Redax commit " << REDAX_BUILD_COMMIT << "\n";
  return 0;
}
// PrintVersion is to output the version of the redax (usage not found yet ..)






std::atomic_bool b_run = true;
// Define an atomic variable called the b_run 

std::string hostname = "";
// hostname is set to null by default 









void SignalHandler(int signum) {
    std::cout << "\n SingalHandler"  << "\nReceived signal "<<signum<<std::endl;
    b_run = false;
    return;
}
/*
  Signum has many types of signum .   
  It seems that the function is to stop the process via modifying b_run  ???? 
  If something went wrong or interrupted, and then the process would be stopped . 
*/


void UpdateStatus(std::shared_ptr<mongocxx::pool> pool, std::string dbname,
    std::unique_ptr<DAQController>& controller) {
  
  using namespace std::chrono;
  //chrono is to process the time related functions .
  
  auto client = pool->acquire();
  auto db = (*client)[dbname];
  auto collection = db["status"];
  auto next_sec = ceil<seconds>(system_clock::now());
  /*
    pool is about the variable stored within computer's memory
   "auto" is to detect the type of the variable automatically . 
    We will write the status data into the client[dbname]['status']
  */
  
  const auto dt = seconds(1);
  std::this_thread::sleep_until(next_sec);
  /*
  :: Scope operator is to define the function namespace 
  We would only update the status of the system once per second . 
  */
  
  while (b_run == true) {
    // I think it is important to undertand the b_run now...
    try{
      controller->StatusUpdate(&collection);
      // The function is within the controller.cc that to update the status stored in the db['status']
      
    }catch(const std::exception &e){
      std::cout<<"Can't connect to DB to update."<<std::endl;
      std::cout<<e.what()<<std::endl;
      /*  
      Exceptions is to record the exception (errors ) that is defined via the std::exception originate from the try  . 
      If we could not update the status via the Redax , we shall output the errors . 
      */
    }
    next_sec += dt;
    std::this_thread::sleep_until(next_sec);
  }
  std::cout<<"Status update returning\n";
  // When the b_run is False , we shall also return some feedback . 
}
// UpdatesStatus function is to write the status into the database once per second .. 



int PrintUsage() {
  std::cout<< "\n" 
    << "PrintUsage : Welcome to redax\n"
    << "Accepted command-line arguments:\n"
    << "--id <id number>: id number of this readout instance, required\n"
    << "--uri <mongo uri>: full MongoDB URI, required\n"
    << "--db <database name>: name of the database to use, default \"daq\"\n"
    << "--logdir <directory>: where to write the logs, default pwd\n"
    << "--reader: this instance is a reader\n"
    << "--cc: this instance is a crate controller\n"
    << "--log-retention <value>: how many days to keep logfiles, default 7, 0 = forever\n"
    << "--help: print this message\n"
    << "--version: print version information and return\n"
    << "Notice : real host name : host+__reader/cc__+id   "
    << "\n";
  return 1;
}
// Printusage is to output the user manual of the sofware . 










int main(int argc, char** argv){
  mongocxx::instance instance{};
  /*
   Need to create a mongocxx instance and it must exist for the entirety of the program. So here seems good.
  */
  
  
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  /*
  SIGINT: signal interrupt 
  SIGTERM: signal terminate 
  When the program is interrupted or terminated , it will make sense . 
  */

  std::string current_run_id="none", log_dir = "";
  std::string dbname = "daq", suri = "", sid = "";
  bool reader = false, cc = false;
  // current_run_id , log_dir , dbname , suri , sid 
  
  int log_retention = 7; // days, 0 = someone else's problem
  int c(0), opt_index;
  // int c(0) means that c is set to 0 by default . 
  
  
  enum { arg_id, arg_uri, arg_db, arg_logdir, arg_reader, arg_cc,
    arg_retention, arg_help, arg_version };
    
    
  struct option longopts[] = {
    {"id", required_argument, 0, arg_id},
    {"uri", required_argument, 0, arg_uri},
    {"db", required_argument, 0, arg_db},
    {"logdir", required_argument, 0, arg_logdir},
    {"reader", no_argument, 0, arg_reader},
    {"cc", no_argument, 0, arg_cc},
    {"log-retention", required_argument, 0, arg_retention},
    {"help", no_argument, 0, arg_help},
    {"version", no_argument, 0, arg_version},
    {0, 0, 0, 0}
  };
  // arguments includes the id , uri , db , logdir, reader , cc , log-retention , help , version 
  
  
  
  
  while ((c = getopt_long(argc, argv, "", longopts, &opt_index)) != -1) {
    switch(c) {
      case arg_id:
        sid = optarg; break;
      case arg_uri:
        suri = optarg; break;
      case arg_db:
        dbname = optarg; break;
      // dbname is set to daq by default 
      case arg_logdir:
        log_dir = optarg; break;
      case arg_reader:
        reader = true; break;
      // If we have --reader command line , then the reader will be set true 
      case arg_cc:
        cc = true; break;
      // If we have --cc command line  , then the cc will be set true 
      // We could not set both the cc and reader true . 
      case arg_retention:
        log_retention = std::stoi(optarg); break;
      // for the time being , I do not know the log_retention meaning 
      case arg_help:
        return PrintUsage();
      case arg_version:
        return PrintVersion();
      default:
        std::cout<<"Received unknown arg\n";
        return PrintUsage();
    }
  }
  if (suri == "" || sid == "") return PrintUsage();
  // PrintUsage is to output the user manual of the Redax and then stop this software . 
  // We need to specify the uri and id at the same time so that it could run . 
  if (reader == cc) {
    std::cout<<"Specify --reader XOR --cc\n";
    return 1;
  }
  // By default , the reader and cc are all set to false . 
 
  /*
  argc is the number of the arguments 
  argv is the content of the arguments , and then the argv[0] is the command itself .
  optstring : the options we need , new:parameter .
  -1 means the end of command line parameters . 
  */






  // We will consider commands addressed to this PC's ID 
  const int HOST_NAME_MAX = 64; // should be #defined in unistd.h but isn't???
  char chostname[HOST_NAME_MAX];
  gethostname(chostname, HOST_NAME_MAX);
  hostname=chostname;
  hostname+= (reader ? "_reader_" : "_controller_") + sid;
  PrintVersion();
  std::cout<<"Reader starting with ID: "<<hostname<<std::endl;
  // The detailed hostname includes the hostname+_reader_+sid , not just the host name like the "RelicsDAQ"


  /*
  ./redax  --uri mongodb://192.168.1.88:27017 --logdir /home/data/TPC --id 0 --reader
  Redax commit 67efe549e936b05d3dc2b4c6b4a6b7c91535d4b0
  Reader starting with ID: RelicsDAQ_reader_0
  Local file logging to /home/data/TPC
  Logging to "/home/data/TPC/20220713_RelicsDAQ_reader_0.log"
  
  A typical start and original output of the redax is like above ; 
  The first line is about the redax commit and its version . 
  The second line is about the redax hostname -- not only the hostname but also the reader and the run_id ; 
  To every run_mode , the run id should be unique and constant . 
  */


  // MongoDB Connectivity for control database. Bonus for later:
  // exception wrap the URI parsing and client connection steps
  mongocxx::uri uri(suri.c_str());
  auto pool = std::make_shared<mongocxx::pool>(uri);
  // pool is a shared variable to log the uri of the instance , and we could read it via the pool.acquire()
  auto client = pool->acquire();
  mongocxx::database db = (*client)[dbname];
  mongocxx::collection control = db["control"];
  mongocxx::collection opts_collection = db["options"];
  
  /*
  Connection variables : 
  mongocxx::uri  , just the link of the mongo 
  Now , the client is shared by the memory via the make_shared variable --pool .  
  client - database - collections : control , options 
  */
  
  /* 
  pool : a memory shared uri address 
  db : the database to be used 
  control ; the contorl collection of the database 
  opts_collection : the options collection of the database . 
  */




  // Logging
  std::shared_ptr<MongoLog> fLog;
  if (log_dir == "nT")
    fLog = std::make_shared<MongoLog_nT>(pool, dbname, hostname);
  else
  {
    std::cout << "We are creating the shared mongo log pointer at" <<  log_dir << "\n" ;
    fLog = std::make_shared<MongoLog>(log_retention, pool, dbname, log_dir, hostname); 
    //Initialize one mongolog pointer . 
  }
  // However , I think we could use the reload to avoid this if ...
  
  if (fLog->Initialize()) {
    std::cout<<"Could not initialize logs!\n";
    exit(-1);
  }
  // fLog is a shared pointer of the log files 


  //Options
  std::cout << "We are creating the shared mongo options pointer \n" ; 
  std::shared_ptr<Options> fOptions;
  // Options is a shared pointer of the options 
  
  
  
  // The DAQController object is responsible for passing commands to the
  // boards and tracking the status
  std::unique_ptr<DAQController> controller;
  if (cc)
  {
    std::cout<< "We are running the CControl_Handler\n" ; 
    controller = std::make_unique<CControl_Handler>(fLog, hostname);
  }
  else
  {
    std::cout<< "We are running the DAQController\n" ; 
    controller = std::make_unique<DAQController>(fLog, hostname);
  }
  std::thread status_update(&UpdateStatus, pool, dbname, std::ref(controller));
  // controller and recorder are different , DAQController is to read the data and then transfer to the DAQ instruments. 




  using namespace bsoncxx::builder::stream;
  auto opts = mongocxx::options::find_one_and_update{};
  opts.sort(document{} << "_id" << 1 << finalize);
  std::string ack_host = "acknowledged." + hostname;
  std::cout << "options sorted by time \n"  ; 
  // Sort oldest to newest
  
  auto query = document{} << "host" << hostname << ack_host << 0 << finalize;
  auto update = document{} << "$currentDate" << open_document <<
    ack_host << true << close_document << finalize;
  std::cout << "ack_host:" << ack_host << std::endl ; 
  // The acknowledged host and the host only differs in the "acknowledged."
  
  
  using namespace std::chrono;
  // Main program loop. Scan the database and look for commands addressed to this hostname. 
  
  while(b_run == true)
  {
    try
    {
      auto qdoc = control.find_one_and_update(query.view(), update.view(), opts);
      // The find_one_and_update includes the query and the updates from the opts (option collection)
      if (qdoc) 
      {
	      //If we found a doc from the database and then get the command out of the doc
        auto doc = qdoc->view();
	      std::string command = "";
	      std::string user = "";
	      try
        {
	        command = (doc)["command"].get_utf8().value.to_string();
	        user = (doc)["user"].get_utf8().value.to_string();
	      }
	      catch (const std::exception &e)
        {
	       fLog->Entry(MongoLog::Warning, "Received malformed command %s",
			   bsoncxx::to_json(doc).c_str());
         // If something went wrong , we shall also log it . 
	      }
	      fLog->Entry(MongoLog::Debug, "Found a doc with command %s", command.c_str());
        auto ack_time = system_clock::now();
        // It seems that we have already got the file .
        // Process commands
        
	      if(command == "start")
        {
	       if(controller->status() == 2) 
         {
	          if(controller->Start()!=0)
            {
	           continue;
	          }
            auto now = system_clock::now();
            fLog->Entry(MongoLog::Local, "Ack to start took %i us",
                duration_cast<microseconds>(now-ack_time).count());
	        }
	        else
	        fLog->Entry(MongoLog::Debug, "Cannot start DAQ since not in ARMED state (%i)", controller->status());
	      }
        
        else if(command == "stop")
        {
	      // "stop" is also a general reset command and can be called any time
	       if(controller->Stop()!=0)
	       fLog->Entry(MongoLog::Error,
			   "DAQ failed to stop. Will continue clearing program memory.");
          auto now = system_clock::now();
          fLog->Entry(MongoLog::Local, "Ack to stop took %i us",
              duration_cast<microseconds>(now-ack_time).count());
          fLog->SetRunId(-1);
          fOptions.reset();
	      } 
        
        
        else if(command == "arm")
        {
	       // Can only arm if we're idle
	       if(controller->status() == 0)
         {
           //To arm the instruments , we have to stop the controllers first . 
           fLog->Entry(MongoLog::Local, "We are going to stop the controller. ");
	         controller->Stop();
           fLog->Entry(MongoLog::Local, "We have finished stopping the controller. ");
           // Get an override doc from the 'options_override' field if it exists
	         std::string override_json = "";
	         try
           {
	         auto oopts = doc["options_override"].get_document().view();
           std::string str_oopts =doc["options_override"].get_utf8().value.to_string();
	         override_json = bsoncxx::to_json(oopts);
           fLog->Entry(MongoLog::Local, "We have got the override_opts %s  " , str_oopts);
	         }
	        catch(const std::exception &e)
          {
            fLog->Entry(MongoLog::Local, "We did not get the override_opts " );
	        }
	        
          // Mongocxx types confusing so passing json strings around
          std::string mode = doc["mode"].get_utf8().value.to_string();
          fLog->Entry(MongoLog::Local, "Getting options doc for mode %s", mode.c_str());
          
          
          fLog->Entry(MongoLog::Local, "Ready to set up the foptions pointers" );
          fOptions = std::make_shared<Options>(fLog, mode, hostname, &opts_collection,
			      pool, dbname, override_json);
            //This command does not work well ... 
            //But if we successfully debug it , everything will be ok . 
            
            
          fLog->Entry(MongoLog::Local, "Successfully set up the fOptions pointer");
          int dt = duration_cast<milliseconds>(system_clock::now()-ack_time).count();
          fLog->SetRunId(fOptions->GetInt("number", -1));
          fLog->Entry(MongoLog::Local, "Took %i ms to load config", dt);
	        if(controller->Arm(fOptions) != 0){
	         fLog->Entry(MongoLog::Error, "Failed to initialize electronics");
	         controller->Stop();
	         }
          else
          {
	        fLog->Entry(MongoLog::Debug, "Initialized electronics");
	        }
	      } // if status is ok
	      
        
        else
	       fLog->Entry(MongoLog::Warning, "Cannot arm DAQ while not 'Idle'");
	    }
     else if (command == "quit") b_run = false;
      } // if doc
    }
    catch(const std::exception &e)
    {
      std::cout<<e.what()<<std::endl;
      std::cout<<"Can't connect to DB so will continue what I'm doing"<<std::endl;
    }
    std::this_thread::sleep_for(milliseconds(100));
  }
  
  status_update.join();
  controller.reset();
  fOptions.reset();
  fLog.reset();
  exit(0);
}


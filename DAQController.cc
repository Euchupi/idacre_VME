#include "DAQController.hh"
#include "V1724.hh"
#include "V1724_MV.hh"
#include "V1730.hh"
#include "V1725.hh"
#include "f1724.hh"
#include "DAXHelpers.hh"
#include "Options.hh"
#include "StraxFormatter.hh"
#include "MongoLog.hh"
#include <algorithm>
#include <bitset>
#include <chrono>
#include <cmath>
#include <numeric>
#include <iostream>
#include <fstream>

#include <bsoncxx/builder/stream/document.hpp>

// Status:
// 0-idle
// 1-arming
// 2-armed
// 3-running
// 4-error

DAQController::DAQController(std::shared_ptr<MongoLog>& log, std::string hostname){
  fLog=log;
  fOptions = nullptr;
  fStatus = DAXHelpers::Idle;
  fReadLoop = false;
  fNProcessingThreads=8;
  fDataRate=0.;
  fHostname = hostname;
  fPLL = 0;
}




DAQController::~DAQController(){
  if(fProcessingThreads.size()!=0)
    CloseThreads();
}




int DAQController::Arm(std::shared_ptr<Options>& options){
  fOptions = options;
  std::cout << "Start arm the devices" << std::endl ;

  fNProcessingThreads = fOptions->GetInt("processing_threads");
  std::cout << "Successfully initialized the fnprocessingthreads" << std::endl ; 

  fLog->Entry(MongoLog::Local, "Beginning electronics initialization with %i threads",
	      fNProcessingThreads);

  // Initialize digitizers
  fPLL = 0;
  fStatus = DAXHelpers::Arming;
  int num_boards = 0;

  std::cout << "Prepare for get boards" << std::endl ; 
  for(auto& d : fOptions->GetBoards("V17XX")){
    fLog->Entry(MongoLog::Local, "Arming new digitizer %i", d.board);

    std::shared_ptr<V1724> digi;
    try{
      std::cout << "Board init start" << std::endl ; 
      if(d.type == "V1724_MV")
        digi = std::make_shared<V1724_MV>(fLog, fOptions, d.board, d.vme_address);
      else if(d.type == "V1730")
      { 
        std::cout << "Init the V1730" << std::endl ; 
        digi = std::make_shared<V1730>(fLog, fOptions, d.board, d.vme_address);
        std::cout << "Finish initialized the V1730" << std::endl ;
      }
      else if(d.type == "V1725")
      digi = std::make_shared<V1725>(fLog, fOptions, d.board, d.vme_address);
      else if(d.type == "f1724")
        digi = std::make_shared<f1724>(fLog, fOptions, d.board, 0);
      else
        digi = std::make_shared<V1724>(fLog, fOptions, d.board, d.vme_address);
      std::cout << "Prepare for Board init" << std::endl ; 
      
      if (digi->Init(d.link, d.crate))
        throw std::runtime_error("Board init failed");
      std::cout << "Board init in progress" << std::endl ;
      fDigitizers[d.link].emplace_back(digi);
      num_boards++;
    }
    catch(const std::exception& e) {
      fLog->Entry(MongoLog::Warning, "Failed to initialize digitizer %i: %s", d.board,
          e.what());
      fDigitizers.clear();
      return -1;
    }
  }
  std::cout << "Finish arming the digiziers ." << std::endl ; 
  std::cout << "This host has  " << num_boards << " boards" << std::endl ; 
  std::cout << "Sleeping for two seconds" << std::endl ; 
  fLog->Entry(MongoLog::Local, "This host has %i boards", num_boards);
  fLog->Entry(MongoLog::Local, "Sleeping for two seconds");
  // For the sake of sanity and sleeping through the night,
  // do not remove this statement.
  sleep(2); // <-- this one. Leave it here.
  // Seriously. This sleep statement is absolutely vital.
  fLog->Entry(MongoLog::Local, "That felt great, thanks.");


  std::map<int, std::vector<uint16_t>> dac_values;
  std::vector<std::thread> init_threads; 
  init_threads.reserve(fDigitizers.size()); // reserve the memory for the threads . 
  std::map<int,int> rets;
  std::cout << "Create init_threads" << std::endl; 

  // Parallel digitizer programming to speed baselining
  for( auto& link : fDigitizers ) {
    rets[link.first] = 1;
    std::cout <<  "link.first got  " << link.first << std::endl ;   
    init_threads.emplace_back(&DAQController::InitLink, this,
	  std::ref(link.second), std::ref(dac_values), std::ref(rets[link.first]));
    std::cout << "init_threads.emplace_back successfully" << std::endl ; 
  }
  for (auto& t : init_threads) 
  {
    std::cout << "init_threads" << std::endl ; 
    if (t.joinable()) 
    {
      std::cout << "The thread is joinable" << std::endl ; 
      t.join();
    }
  }// : is a range based for 

  std::cout << "Parallel digitizer" << std::endl ; 

  if (std::any_of(rets.begin(), rets.end(), [](auto& p) {return p.second != 0;})) {
    fLog->Entry(MongoLog::Warning, "Encountered errors during digitizer programming");
    if (std::any_of(rets.begin(), rets.end(), [](auto& p) {return p.second == -2;}))
      fStatus = DAXHelpers::Error;
    else
      fStatus = DAXHelpers::Idle;
    return -1;
  } else
    fLog->Entry(MongoLog::Debug, "Digitizer programming successful");
  std::cout << "Digitizer programming successful" << std::endl ; 

  if (fOptions->GetString("baseline_dac_mode") == "fit" ) 
  {
    fOptions->UpdateDAC(dac_values);
  }
  std::cout << "Digitizer::Arm baseline UpdateDAC " << std::endl ; 


  for(auto& link : fDigitizers ) {
    for(auto& digi : link.second){
      digi->AcquisitionStop();
    }
  }
  std::cout << "DAQController::Arm AcquisitionStop " << std::endl ; 

  
  if (OpenThreads()) {
    fLog->Entry(MongoLog::Warning, "Error opening threads");
    std::cout << "Failed to open the threads " << std::endl ; 
    fStatus = DAXHelpers::Idle;
    return -1;
  }
  
  std::cout << "DAQController::Arm OpenThreads finished" << std::endl ; 
  //std::cout << "Have another sleep!!" << std::endl ; 
  sleep(2);
  //std::cout << "A good sleep !!  " << std::endl ; 
  


  fStatus = DAXHelpers::Armed;
  fLog->Entry(MongoLog::Local, "Arm command finished, returning to main loop");

  std::cout << "DAQController::Arm exit" << std::endl ; 
  return 0;
}




int DAQController::Start(){
  if(fOptions->GetInt("run_start", 0) == 0)
  {
    for(auto& link : fDigitizers ){
      for(auto& digi : link.second){
        if(digi->EnsureReady()!= true || digi->SoftwareStart() || digi->EnsureStarted() != true){
          fLog->Entry(MongoLog::Warning, "Board %i not started?", digi->bid());
          return -1;
        } else
        {
          fLog->Entry(MongoLog::Local, "Board %i started", digi->bid());
          std::cout << "Board " << digi->bid() << "started" << std::endl ; 
        }
      }
    }
  } 
  else {
    for (auto& link : fDigitizers)
      for (auto& digi : link.second)
        if (digi->SINStart() || !digi->EnsureReady())
          fLog->Entry(MongoLog::Warning, "Board %i not ready to start?", digi->bid());
        else
          fLog->Entry(MongoLog::Local, "Board %i is ARMED and DANGEROUS", digi->bid());
  }
  fStatus = DAXHelpers::Running;
  return 0;
}




int DAQController::Stop(){

  fReadLoop = false; // at some point.
  int counter = 0;
  bool one_still_running = false;
  do{
    // wait around for up to 10x100ms for the threads to finish reading
    one_still_running = false;
    for (auto& p : fRunning) one_still_running |= p.second;
    if (one_still_running) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }while(one_still_running && counter++ < 10);
  if (counter >= 10) fLog->Entry(MongoLog::Local, "Boards taking a while to clear");
  fLog->Entry(MongoLog::Local, "Stopping boards");
  for( auto const& link : fDigitizers ){
    for(auto digi : link.second){
      digi->AcquisitionStop(true);

      // Ensure digitizer is stopped
      if(digi->EnsureStopped() != true){
	fLog->Entry(MongoLog::Warning,
		    "Timed out waiting for %i to stop after SW stop sent", digi->bid());
          //return -1;
      }
    }
  }
  fLog->Entry(MongoLog::Debug, "Stopped digitizers, closing threads");
  CloseThreads();
  fLog->Entry(MongoLog::Local, "Closing Digitizers");
  for(auto& link : fDigitizers ){
    for(auto& digi : link.second){
      digi->End();
      digi.reset();
    }
    link.second.clear();
  }
  fDigitizers.clear();

  fPLL = 0;
  fLog->SetRunId(-1);
  fOptions.reset();
  fLog->Entry(MongoLog::Local, "Finished end sequence");
  fStatus = DAXHelpers::Idle;
  return 0;
}

void DAQController::ReadData(int link){
  fReadLoop = true;
  fDataRate = 0;
  uint32_t board_status = 0;
  int readcycler = 0;
  int err_val = 0;
  std::list<std::unique_ptr<data_packet>> local_buffer;
  std::unique_ptr<data_packet> dp;
  std::vector<int> mutex_wait_times;
  mutex_wait_times.reserve(1<<20);
  int words = 0;
  unsigned transfer_batch = fOptions->GetInt("transfer_batch", 8);
  int bytes_this_loop(0);
  fRunning[link] = true;
  std::chrono::microseconds sleep_time(fOptions->GetInt("us_between_reads", 10));
  int c = 0;
  const int num_threads = fNProcessingThreads;

  while(fReadLoop){
    for(auto& digi : fDigitizers[link]) {
      // Channel 0 Configuration 
      /*
      std::cout << "" << std::endl ; 
      std::cout << "" << std::endl ; 
	    std::cout << "Minimum Record Length 0x1020 :" << digi->ReadRegister(0x1020) << std::endl;
	    std::cout << "Dummy32 0x1024 :" << digi->ReadRegister(0x1024) << std::endl ; 
	    std::cout << "Input Dynamic Range 0x1028 :" << digi->ReadRegister(0x1028) << std::endl ; 
	    std::cout << "Input Delay 0x1034 :" << digi->ReadRegister(0x1034) << std::endl ; 
	    std::cout << "Pre Trigger 0x1038 :" << digi->ReadRegister(0x1038) << std::endl ;
	    std::cout << "Trigger Threshold 0x1060 :" << digi->ReadRegister(0x1060) << std::endl ;
	    std::cout << "Fixed Baseline 0x1064 :" << digi->ReadRegister(0x1064) << std::endl ;
	    //std::cout << "Couple Trigger Logic 0x1068 :" << digi->ReadRegister(0x1068) << std::endl ;
	    std::cout << "Samples Under Threshold  0x1078 :" << digi->ReadRegister(0x1078) << std::endl ;
	    std::cout << "Maximum Tail  0x107C :" << digi->ReadRegister(0x107C) << std::endl ;
	    std::cout << "DPP Algorithm Control 0x1080 :" << digi->ReadRegister(0x1080) << std::endl ;
	    std::cout << "Couple Over-Threshold Log 0x1084 :" << digi->ReadRegister(0x1084) << std::endl ;
    	std::cout << "Channel Status 0x1088 :" << digi->ReadRegister(0x1088) << std::endl ;
    	std::cout << "AMC Firmware Revision 0x108C :" << digi->ReadRegister(0x108C) << std::endl ;
    	std::cout << "DC Offset 0x1098 :" << digi->ReadRegister(0x1098) << std::endl ;
      std::cout << "Channel ADC Temperature 0x10A8 :" << digi->ReadRegister(0x10A8) << std::endl ;

      //Board information 
    	std::cout << "Board Configuration 0x8000 :" << digi->ReadRegister(0x8000) << std::endl ;
	    std::cout << "Acquisition Control 0x8100 :" << digi->ReadRegister(0x8100) << std::endl ;
	    std::cout << "Acquisition Status 0x8104 :" << digi->ReadRegister(0x8104) << std::endl ;
	    std::cout << "Global Trigger Mask 0x810C :" << digi->ReadRegister(0x810C) << std::endl ;
      std::cout << "Front Panel TRG-OUT Enable 0x8110 :" << digi->ReadRegister(0x8110) << std::endl ;
	    std::cout << "LVDS I/O 0x8118 :" << digi->ReadRegister(0x8118) << std::endl ;
	    std::cout << "Front Panel I/O control 0x811C :" << digi->ReadRegister(0x811C) << std::endl ;
    	std::cout << "Channel Enable Mask  0x8120 :" << digi->ReadRegister(0x8120) << std::endl ;
     	std::cout << "ROC FPGA Firmware Revision 0x8124 :" << digi->ReadRegister(0x8124) << std::endl ;
    	std::cout << "Event Stored 0x812C :" << digi->ReadRegister(0x812C) << std::endl ;
    	std::cout << "Voltage Level Mode Configuration 0x8138 :" << digi->ReadRegister(0x8138) << std::endl ;
	    std::cout << "Board info 0x8140 :" << digi->ReadRegister(0x8140) << std::endl ;
    	std::cout << "Analog Monitor Mode 0x8144 :" << digi->ReadRegister(0x8144) << std::endl ;
    	std::cout << "Event Size 0x814C :" << digi->ReadRegister(0x814C) << std::endl ;
    	std::cout << "Fan Speed Control 0x8168 :" << digi->ReadRegister(0x8168) << std::endl ;
    	std::cout << "Run/Start/Stop Delay 0x8170 :" << digi->ReadRegister(0x8170) << std::endl ;
    	std::cout << "Board Failure Status 0x8178 :" << digi->ReadRegister(0x8178) << std::endl ;
    	std::cout << "Front Panel LVDS I/O New Features 0x81A0 :" << digi->ReadRegister(0x81A0) << std::endl ;
    	std::cout << "Buffer Occupancy Gain 0x81B4 :" << digi->ReadRegister(0x81B4) << std::endl ;
    	std::cout << "Extented Veto Delay 0x81C4 :" << digi->ReadRegister(0x81C4) << std::endl ;
    	std::cout << "Readout Control  0xEF00 :" << digi->ReadRegister(0xEF00) << std::endl ;
    	std::cout << "Readout Status 0xEF04 :" << digi->ReadRegister(0xEF04) << std::endl ;
    	std::cout << "Board ID 0xEF08 :" << digi->ReadRegister(0xEF08) << std::endl ;
    	std::cout << "MCST Base Address and Control 0xEF0C :" << digi->ReadRegister(0xEF0C) << std::endl ;
    	std::cout << "Relocation Address 0xEF10 :" << digi->ReadRegister(0xEF10) << std::endl ;
    	std::cout << "Interrupt Status/ID 0xEF14 :" << digi->ReadRegister(0xEF14) << std::endl ;
    	std::cout << "Interrupt Event Number 0xEF18 :" << digi->ReadRegister(0xEF18) << std::endl ;
    	std::cout << "Max Number of Events per BLT 0xEF1C :" << digi->ReadRegister(0xEF1C) << std::endl ;
    	std::cout << "Scratch 0xEF20 :" << digi->ReadRegister(0xEF20) << std::endl ;
      */ 

      // periodically report board status
      if(readcycler == 0){
        board_status = digi->GetAcquisitionStatus();
        fLog->Entry(MongoLog::Local, "Board %i has status 0x%04x",
            digi->bid(), board_status);
      }
      if (digi->CheckFail()) {
        err_val = digi->CheckErrors();
        fLog->Entry(MongoLog::Local, "Error %i from board %i", err_val, digi->bid());
        std::cout << "Error " << err_val << " from board" << digi->bid() << std::endl ; 
        if (err_val == -1 || err_val == 0) {
        } else {
          fStatus = DAXHelpers::Error; // stop command will be issued soon
          if (err_val & 0x1) {
            fLog->Entry(MongoLog::Local, "Board %i has PLL unlock", digi->bid());
            fPLL++;
          }
          if (err_val & 0x2) fLog->Entry(MongoLog::Local, "Board %i has VME bus error", digi->bid());
        }
      }
      if((words = digi->Read(dp))<0){ 
        dp.reset();
        fStatus = DAXHelpers::Error;
        break;
      } else if(words>0){
        //std::cout << words << std::endl ; 
        dp->digi = digi;
        local_buffer.emplace_back(std::move(dp));
        bytes_this_loop += words*sizeof(char32_t);
      }
    } // for digi in digitizers
    if (local_buffer.size() && (readcycler % transfer_batch == 0)) {
      fDataRate += bytes_this_loop;
      auto t_start = std::chrono::high_resolution_clock::now();
      while (fFormatters[(++c)%num_threads]->ReceiveDatapackets(local_buffer, bytes_this_loop)) {}
      auto t_end = std::chrono::high_resolution_clock::now();
      mutex_wait_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
            t_end-t_start).count());
      bytes_this_loop = 0;
    }
    if (++readcycler > 10000) readcycler = 0;
    std::this_thread::sleep_for(sleep_time);
  } // while run
  if (mutex_wait_times.size() > 0) {
    std::sort(mutex_wait_times.begin(), mutex_wait_times.end());
    fLog->Entry(MongoLog::Local, "RO thread %i mutex report: min %i max %i mean %i median %i num %i",
        link, mutex_wait_times.front(), mutex_wait_times.back(),
        std::accumulate(mutex_wait_times.begin(), mutex_wait_times.end(), 0l)/mutex_wait_times.size(),
        mutex_wait_times[mutex_wait_times.size()/2], mutex_wait_times.size());
  }
  fRunning[link] = false;
  fLog->Entry(MongoLog::Local, "RO thread %i returning", link);
  std::cout << "DAQController::ReadData exit" << std::endl ; 
  return ; 
}

int DAQController::OpenThreads(){
  const std::lock_guard<std::mutex> lg(fMutex);
  std::cout << "Function DAQController::OpenThreads" << std::endl ; 
  fProcessingThreads.reserve(fNProcessingThreads);
  for(int i=0; i<fNProcessingThreads; i++){
    try 
    {
       fFormatters.emplace_back(std::make_unique<StraxFormatter>(fOptions, fLog));
      // We initialize the straxformatter and then run it . 
       fProcessingThreads.emplace_back(&StraxFormatter::Process, fFormatters.back().get());
    } 
    catch(const std::exception& e) 
    {
      fLog->Entry(MongoLog::Warning, "Error opening processing threads: %s",
          e.what());
      return -1;
    }
    std::cout << "Initialize fNProcessingThreads " << i << std::endl ;
  }
  
  std::cout << "All Threads has been initialized " << std::endl ; 
  fReadoutThreads.reserve(fDigitizers.size()); // .reserve could modify the capacity of the vector . 
  
  int fDigitizers_count =0 ; 
  for (auto& p : fDigitizers)
  {
    fDigitizers_count += 1; 
    fReadoutThreads.emplace_back(&DAQController::ReadData, this, p.first);
  }
  std::cout << "fDigitizers number: " << fDigitizers_count << std::endl ; 
  std::cout << "DAQController::OpenThreads Exit" << std::endl; 

  return 0;
}

void DAQController::CloseThreads(){
  const std::lock_guard<std::mutex> lg(fMutex);
  fLog->Entry(MongoLog::Local, "Ending RO threads");
  for (auto& t : fReadoutThreads) if (t.joinable()) t.join();
  fLog->Entry(MongoLog::Local, "Joining processing threads");
  std::map<int,int> board_fails;
  for (auto& sf : fFormatters) {
    while (sf->GetBufferSize().first > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sf->Close(board_fails);
  }
  for (auto& t : fProcessingThreads) if (t.joinable()) t.join();
  fProcessingThreads.clear();
  fLog->Entry(MongoLog::Local, "Destroying formatters");
  for (auto& sf : fFormatters) sf.reset();
  fFormatters.clear();

  if (std::accumulate(board_fails.begin(), board_fails.end(), 0,
	[=](int tot, auto& iter) {return std::move(tot) + iter.second;})) {
    std::stringstream msg;
    msg << "Found board failures: ";
    for (auto& iter : board_fails) msg << iter.first << ":" << iter.second << " | ";
    fLog->Entry(MongoLog::Warning, msg.str());
  }
}

void DAQController::StatusUpdate(mongocxx::collection* collection) {
  using namespace bsoncxx::builder::stream;
  auto insert_doc = document{};
  std::map<int, int> retmap;
  std::pair<long, long> buf{0,0};
  int rate = fDataRate;
  int i_count=0 ; 
  fDataRate -= fDataRate; // atomic nonsense, increment and decrement ops are better than assignment
  {
    const std::lock_guard<std::mutex> lg(fMutex);
    for (auto& p : fFormatters) {
      p->GetDataPerChan(retmap);
      i_count= i_count +1 ; 
      auto x = p->GetBufferSize();
      buf.first += x.first;
      buf.second += x.second;
    }
  }
  int rate_alt = std::accumulate(retmap.begin(), retmap.end(), 0,
      [&](int tot, const std::pair<int, int>& p) {return std::move(tot) + p.second;});
  auto doc = document{} <<
    "host" << fHostname <<
    "time" << bsoncxx::types::b_date(std::chrono::system_clock::now())<<
    "rate_old" << rate*1e-6 <<
    "rate" << rate_alt*1e-6 <<
    "status" << fStatus <<
    "buffer_size" << (buf.first + buf.second)/1e6 <<
    "mode" << (fOptions ? fOptions->GetString("name", "none") : "none") <<
    "number" << (fOptions ? fOptions->GetInt("number", -1) : -1) <<
    "pll" << fPLL.load() <<
    "channels" << open_document <<
      [&](key_context<> doc){
      for( auto const& pair : retmap)
        doc << std::to_string(pair.first) << short(pair.second>>10); // KB not MB
      } << close_document << 
    finalize;
  collection->insert_one(std::move(doc));
  // std::cout << "We now have " << i_count << " available StraxFormatter threads" << std::endl ;
  return;
}

void DAQController::InitLink(std::vector<std::shared_ptr<V1724>>& digis,
    std::map<int, std::vector<uint16_t>>& dac_values, int& ret) {
  std::cout << "DAQController InitLink Function " << std::endl ;
  std::string baseline_mode = fOptions->GetString("baseline_dac_mode", "n/a");
  if (baseline_mode == "n/a")
    baseline_mode = fOptions->GetNestedString("baseline_dac_mode."+fOptions->Detector(), "fixed");
  int nominal_dac = fOptions->GetInt("baseline_fixed_value", 7000);
  std::cout << "Got the baseline_fixed_value  " <<  nominal_dac  <<std::endl; 
  if (baseline_mode == "fit") {
    if ((ret = FitBaselines(digis, dac_values)) < 0) {
      fLog->Entry(MongoLog::Warning, "Errors during baseline fitting");
      return;
    } else if (ret > 0) {
      fLog->Entry(MongoLog::Debug, "Baselines didn't converge so we'll use Plan B");
    }
  }
  std::cout << "DAQController::InitLink Finished fit the baseline DAQController::FitBaselines" << std::endl ; 

  for(auto& digi : digis){
    fLog->Entry(MongoLog::Local, "Board %i beginning specific init", digi->bid());
    digi->ResetFlags();

    std::cout << "Board " << digi->bid() << " Baseline_mode now: " << baseline_mode <<  std::endl ; 
    // Multiple options here
    int bid = digi->bid(), success(0);
    if (baseline_mode == "fit")
    {
    } 
    else if(baseline_mode == "cached") 
    {
      dac_values[bid] = fOptions->GetDAC(bid, digi->GetNumChannels(), nominal_dac);
      fLog->Entry(MongoLog::Local, "Board %i using cached baselines", bid);
    } 
    else if(baseline_mode == "fixed")
    {
      fLog->Entry(MongoLog::Local, "Loading fixed baselines with value 0x%04x", nominal_dac);
      dac_values[bid].assign(digi->GetNumChannels(), nominal_dac);
    } 
    else {
      fLog->Entry(MongoLog::Warning, "Received unknown baseline mode '%s', valid options are 'fit', 'cached', and 'fixed'", baseline_mode.c_str());
      ret = -1;
      return;
    }
    std::cout << "DAQController:InitLink Baseline coule be setup via fixed, cached ,fit " << std::endl ;

    for(auto& regi : fOptions->GetRegisters(bid)){
      unsigned int reg = DAXHelpers::StringToHex(regi.reg);
      unsigned int val = DAXHelpers::StringToHex(regi.val);
      success+=digi->WriteRegister(reg, val);
    }
    // We write the registers during the initlink . 
    std::cout << "fOptions successfully got the registers" << std::endl ;  

    success += digi->LoadDAC(dac_values[bid]);
    std::cout << "fOptions successfully got the dac_values" << std::endl;
    // Load all the other fancy stuff
    // bid is short for the board id . 
    success += digi->SetThresholds(fOptions->GetThresholds(bid));
    std::cout << "fOptions sucessfully get thresholds" << std::endl;

    fLog->Entry(MongoLog::Local, "Board %i programmed", digi->bid());
    if(success!=0){
      fLog->Entry(MongoLog::Warning, "Failed to configure digitizers.");
      ret = -1;
      return;
    }
  } // loop over digis per link

  ret = 0;
  std::cout << "DAQController::InitLink Exit" << std::endl ; 
  return;
}

int DAQController::FitBaselines(std::vector<std::shared_ptr<V1724>> &digis,
    std::map<int, std::vector<u_int16_t>> &dac_values) {
  /* This function has caused a lot of problems in the past for a wide variety of reasons.
   * What it's trying to do isn't complex: figure out iteratively what value to write to the DAC so the
   * baselines show up where you want them to. Usually the boards cooperate, sometimes they don't.
   * A large fraction of the code is dealing with when they don't.
   */


  std::cout << "Get into the DAQController::FitBaselines" << std::endl ; 
  int max_steps = fOptions->GetInt("baseline_max_steps", 30);
  int convergence = fOptions->GetInt("baseline_convergence_threshold", 3);
  uint16_t start_dac = fOptions->GetInt("baseline_start_dac", 10000);
  std::map<int, std::vector<int>> channel_finished;
  std::map<int, bool> board_done;
  std::map<int, std::vector<double>> bl_per_channel;
  int bid;


  std::cout << "Digis is std::vector<std::shared_ptr<V1724>>" << std::endl ; 
  for (auto digi : digis) { // alloc ALL the things!
    bid = digi->bid();
    dac_values[bid] = std::vector<uint16_t>(digi->GetNumChannels(), start_dac); // start higher than we need to
    channel_finished[bid] = std::vector<int>(digi->GetNumChannels(), 0);
    board_done[bid] = false;
    bl_per_channel[bid] = std::vector<double>(digi->GetNumChannels(), 0);
    digi->SetFlags(2);
  }

  std::cout << "Begin baseline step" << std::endl; 
  for (int step = 0; step < max_steps; step++) {
    std::cout << "Beginning baseline step " << step << "/"<< max_steps-1 << std::endl ; 
    fLog->Entry(MongoLog::Local, "Beginning baseline step %i/%i", step, max_steps);
    // prep
    for (auto& d : digis) {
      bid = d->bid();
      std::cout << "Bid : " << d->bid() << std::endl ; 
      if (board_done[bid])
        continue;
      std::cout << "board_done false : " << bid << std::endl ; 
      if (d->LoadDAC(dac_values[d->bid()])) {
        std::cout << "Board " << d->bid() << "failed to load DAC" << std::endl ; 
        fLog->Entry(MongoLog::Warning, "Board %i failed to load DAC", d->bid());
        return -2;
      }
    }
    std::cout << "Half-Finish baseline setup step " << step << "/" << max_steps-1 << std::endl ; 
    // "After writing, the user is recommended to wait for a few seconds before
    // a new RUN to let the DAC output get stabilized" - CAEN documentation
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // sleep(2) seems unnecessary after preliminary testing

    for (auto& d : digis) {
      int bid = d->bid();
      if (board_done[bid])
        continue;
      if (d->BaselineStep(dac_values[bid], channel_finished[bid], bl_per_channel[bid], step) < 0) {
        fLog->Entry(MongoLog::Error, "Error fitting baselines");
        return -2;
      }
      std::cout << "Finish the baselineStep setup step" <<  step << "/" << max_steps-1 << std::endl ; 
      board_done[bid] = std::all_of(channel_finished[bid].begin(), channel_finished[bid].end(), [=](int v){return v >= convergence;});
    }

    if (std::all_of(board_done.begin(), board_done.end(), [](auto& p){return p.second;})) return 0;
    } // end steps

  std::cout << "Finish baseline setup all " << std::endl ;  
  std::string backup_bl = fOptions->GetString("baseline_fallback_mode", "fail");
  std::cout << "Get the backup_bl " << backup_bl << std::endl ; 
  if (backup_bl == "fail") {
    fLog->Entry(MongoLog::Warning, "Baseline fallback mode is 'fail'");
    return -3;
  }
  int fixed = fOptions->GetInt("baseline_fixed_value", 7000);
  std::cout << "Get the baseline_fixed_value " << fixed << std::endl ;

  
  for (auto& p : channel_finished) // (bid, vector)
    for (unsigned i = 0; i < p.second.size(); i++)
      if (p.second[i] < convergence) {
        fLog->Entry(MongoLog::Local, "%i.%i didn't converge, last value %.1f, last offset 0x%x",
            p.first, i, bl_per_channel[p.first][i], dac_values[p.first][i]);
        if (backup_bl == "cached")
          dac_values[p.first][i] = fOptions->GetSingleDAC(p.first, i, fixed);
        else if (backup_bl == "fixed")
          dac_values[p.first][i] = fixed;
      }
  std::cout << "Exit DAQController::FitBaseline" << std::endl ; 
  return 1;
}


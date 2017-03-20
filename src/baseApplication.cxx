#include "baseApplication.hh"
#include "mdbReader.hh"
#include <iostream>
#include <sstream>
#include <stdlib.h>
using namespace levbdim;
std::string wget(std::string url);
baseApplication::baseApplication(std::string name)
{
  
  _fsm=new fsmweb(name);
  
  
  // Register state
  _fsm->addState("VOID");
  _fsm->addState("CREATED");
  
  _fsm->addTransition("CREATE","VOID","CREATED",boost::bind(&baseApplication::create, this,_1));
  
  // Commands
  _fsm->addCommand("GETCONFIG",boost::bind(&baseApplication::c_getconfiguration,this,_1,_2));
  _fsm->addCommand("GETPARAM",boost::bind(&baseApplication::c_getparameter,this,_1,_2));
  _fsm->addCommand("SETPARAM",boost::bind(&baseApplication::c_setparameter,this,_1,_2));
  
  _fsm->setState("VOID");
  
  
  
  // Host and process name
  char hname[80];
  gethostname(hname, 80);
  _hostname = hname;
  char* pname=getenv("PROCESSNAME");
  if (pname!=NULL)
    _processName=pname;
  else
    _processName="UNKNOWN";
  _jConfig=Json::Value::null;
  _jParam=Json::Value::null;
}

void  baseApplication::create(levbdim::fsmmessage* m)
{
  Json::Value jc=m->content();
  if (jc.isMember("file"))
  {
    std::string fileName=jc["file"].asString();
    Json::Reader reader;
    std::ifstream ifs (fileName.c_str(), std::ifstream::in);
    
    bool parsingSuccessful = reader.parse(ifs,_jConfig,false);
    
    
  }
  else
    if (jc.isMember("url"))
    {
      std::string url=jc["url"].asString();
      std::cout<<url<<std::endl;
      std::cout<<"Hostname "<<_hostname<<std::endl;
      std::string jsconf=wget(url);
      std::cout<<jsconf<<std::endl;
      Json::Reader reader;
      bool parsingSuccessful = reader.parse(jsconf, _jConfig);
      
      
    }
    else
      if (jc.isMember("mongo"))
      {
        Json::Value dbp=jc["mongo"];
        mdbReader mdb(dbp["dbname"].asString(),dbp["host"].asString(),dbp["port"].asInt());
        std::string jsconf=mdb.queryDaqConfig(dbp["collection"].asString(),dbp["name"].asString(),dbp["version"].asInt());
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(jsconf, _jConfig); 
      }
    // Overwrite msg
    //Prepare complex answer
    // Now parse the config find the host and the PROCESSNAME
    if (_jConfig==Json::Value::null) 
    {
      Json::Value rep;
      rep["error"]="Missing configuration";
      m->setAnswer(rep);
      return;
    }
    if (!_jConfig.isMember("HOSTS")) 
    {
      Json::Value rep;
      rep["error"]="Missing HOSTS tag";
      m->setAnswer(rep);
      return;
    }
    if (!_jConfig["HOSTS"].isMember(_hostname)) 
    {
      Json::Value rep;
      rep["error"]="Missing hostname in list";
      rep["config"]=_jConfig;
      m->setAnswer(rep);
      return;
    }
    Json::Value _jconf=_jConfig["HOSTS"][_hostname];
    const Json::Value& blist = _jconf;
    
    for (Json::ValueConstIterator it = blist.begin(); it != blist.end(); ++it)
    {
      const Json::Value& b = *it;
      
      if (b.isMember("NAME"))
      {
        if (b["NAME"].asString().compare(_processName)==0)
        {
          if (b.isMember("PARAMETER")) 
          {
            _jParam=b["PARAMETER"];
            break;
          }
        }   
      } 
      
    }      
  end:
  
    Json::Value rep;
    rep["param"]=_jParam;
    rep["config"]=_jConfig;
    m->setAnswer(rep);
    this->userCreate(m);
    return;

}
void baseApplication::c_getconfiguration(Mongoose::Request &request, Mongoose::JsonResponse &response)
{
  if (_jConfig==Json::Value::null)    {response["STATUS"]="NO Configuration yet"; return;}
  //uint32_t adr=atol(request.get("address","2").c_str());
  //uint32_t val=ccc->getCCCReadout()->DoReadRegister(adr);
  
  response["STATUS"]="DONE";
  response["configuration"]=_jConfig;
}
void baseApplication::c_getparameter(Mongoose::Request &request, Mongoose::JsonResponse &response)
{
  if (_jParam==Json::Value::null)    {response["STATUS"]="No Parameters yet"; return;}
  //std::string request.get("address","2").c_str());
  //uint32_t val=ccc->getCCCReadout()->DoReadRegister(adr);
  
  response["STATUS"]="DONE";
  response["PARAMETER"]=_jParam;
}
void baseApplication::c_setparameter(Mongoose::Request &request, Mongoose::JsonResponse &response)
{
  std::string str=request.get("PARAMETER","NONE");
  if (str.compare("NONE")==0) {response["STATUS"]="No PARAMETER tag found"; return;}
  Json::Reader reader;
  Json::Value newconf;
  bool parsingSuccessfull = reader.parse(str,newconf);
  if (parsingSuccessfull) 
    {
      _jParam=newconf;
      response["STATUS"]="DONE";
      response["PARAMETER"]=_jParam;
    }
  else    
    response["STATUS"]="Invalid parsing of PARAMETER";
}
void  baseApplication::userCreate(levbdim::fsmmessage* m) {;}
Json::Value baseApplication::configuration() { return _jConfig;}
Json::Value baseApplication::parameters() {return _jParam;}
fsmweb* baseApplication::fsm(){return _fsm;}

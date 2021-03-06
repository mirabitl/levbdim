#include "baseApplication.hh"
#include "mdbReader.hh"
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include "fsmwebCaller.hh"
using namespace levbdim;
std::string wget(std::string url);
baseApplication::baseApplication(std::string name) : _login("")
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

  // Find the instance
  _instance=0;
 
  char* wi=getenv("INSTANCE");
  if (wi!=NULL) _instance=atoi(wi);
  // Find the port
  _port=0;
 
  char* wp=getenv("WEBPORT");
  if (wp!=NULL) _port=atoi(wp);

  char* wl=getenv("WEBLOGIN");
  if (wl!=NULL) _login=std::string(wl);
  
  _jConfig=Json::Value::null;
  _jParam=Json::Value::null;
}

void  baseApplication::create(levbdim::fsmmessage* m)
{

  Json::Value jc=m->content();
  if (jc.isMember("login"))
    {
      _login=jc["login"].asString();
    }
  std::cout<<"WEB LOGIN is "<<_login<<std::endl<<std::flush;
  // else
  //   _login=std::string("");
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
      //std::string jsconf=wget(url);

      std::string jsconf=fsmwebCaller::curlQuery(url,_login);
      std::cout<<jsconf<<std::endl;
      Json::Reader reader;
      Json::Value jcc;
      bool parsingSuccessful = reader.parse(jsconf,jcc);
      if (jcc.isMember("content"))
	_jConfig=jcc["content"];
      else
	_jConfig=jcc;
      
      
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
	  uint32_t port=0,instance=0;
	  if (b.isMember("ENV"))
	    {
	      const Json::Value& jenv=b["ENV"];

	      for (Json::ValueConstIterator ie = jenv.begin(); ie != jenv.end(); ++ie)
		{
		  std::string envp=(*ie).asString();
              //      std::cout<<"Env found "<<envp.substr(0,7)<<std::endl;
              //std::cout<<"Env found "<<envp.substr(8,envp.length()-7)<<std::endl;
		  if (envp.substr(0,7).compare("WEBPORT")==0)
		    {
		      port=atol(envp.substr(8,envp.length()-7).c_str());
		    }
		  if (envp.substr(0,8).compare("INSTANCE")==0)
		    {
		      instance=atol(envp.substr(9,envp.length()-8).c_str());
		    }

		}
	    }
	    if (port!=_port || instance!=_instance) continue;
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
Json::Value& baseApplication::parameters() {return _jParam;}
fsmweb* baseApplication::fsm(){return _fsm;}

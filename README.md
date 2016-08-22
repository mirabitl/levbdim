LEVBDIM: A Light EVent builder based on DIM

2016-08-22

  ------------------
  Laurent Mirabito
  ------------------

**Abstract.** We developed a light data acquisition system, based on DIM
and mongoose-cpp frameworks. Providing binary data collection, events
building, web accessible finite state machine and process control, it is
well suit to manage distribute data source of laboratory or beam test.
It supports only simple event building (one unique process)

### 1. Introduction {#sec-introduction .h1 data-heading-depth="1" style="display:block"}

DIM [[1](#dim "C Gaspar, DIM,")] is an HEP acquisition framework
developed in the DELPHI experiment. It embeds binary packet exchange in
messages published by server and subscribed by client. It is light and
TCP/IP based and can be installed on nearly all currently used OS
nowdays. Mongoose-cpp is a tiny web server that is used to bind CGI
commands to the DAQ application, either in the finite state machine of
the process or in standalone mode.

LEVBDIM is a simple acquisition framework where DIM is used to exchange
structured binary buffer. It provides several functionalities often used
in modern data acquisition:

-   Event Building
    -   Predefine structure of buffer
    -   Publication of data
    -   Collection of data from numerous publisher
    -   data writing
-   Run control
    -   Generic **F**inite **S**tate **M**achine
    -   Web access
-   Process control
    -   Dedicated FSM used to create process on remote computers with a
        JSON[[5](#json "JSON reference")] description of the
        environment.

All those functionalities are mainly independent and can be used on
their own. The last section of this documentation details a full example
using all the capabilities of LEVBDIM.

### 2. Installation {#sec-installation .h1 data-heading-depth="1" style="display:block"}

#### 2.1. Additional packages {#sec-additional-packages .h2 data-heading-depth="2" style="display:block"}

Few standard packages are needed and can be found on any Linux
distributions: boost, jsoncpp, curl, git and scons

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
sudo apt-get -y install libboost-dev libboost-system-dev 
libboost-filesystem-dev libboost-thread-dev libjsoncpp-dev 
libcurl4-gnutls-dev git scons
 
```

Two network libraries should be installed. The first one is DIM that can
be download and compile from https://dim.web.cern.ch/dim/. The second
one is used to give web access to the application, it's
Mongoose-cpp[[2](#mongoose-cpp "https://github.com/Gregwar/mongoose-cpp")]
based on
Mongoose [[3](#mongoose "https://github.com/cesanta/mongoose")]. One
version is distributed with *levbdim* and can be compiled before the
installation.

#### 2.2. LEVBDIM installation {#sec-levbdim-installation .h2 data-heading-depth="2" style="display:block"}

##### 2.2.1. Getting the levbdim software {#sec-getting-the-levbdim-software .h3 data-heading-depth="3" style="display:block"}

The software is distributed on GitHub. The master version is download
with

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
git clone http://github.com/mirabitl/levbdim.git 
```

##### 2.2.2. Installing the software {#sec-installing-the-software .h3 data-heading-depth="3" style="display:block"}

###### Mongoose-cpp {#sec-mongoose-cpp .h4 data-heading-depth="4" style="display:block"}

A snapshot of mongoose is copied in the web directory

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
cd levbdim 
source web/mongoose.install 
```

The default installation is in /opt/dhcal/levbdim directory.

###### Compiling LEVBDIM {#sec-compiling-levbdim .h4 data-heading-depth="4" style="display:block"}

The compilation is using **Scons**

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
cd levbdim
scons  
```

The library *liblevbdim.so* is installed in the *lib* subdirectory.

### 3. Library functionalities {#sec-library-functionalities .h1 data-heading-depth="1" style="display:block"}

#### 3.1. The Event builder {#sec-the-event-builder .h2 data-heading-depth="2" style="display:block"}

Event building consists of merging various data source that collect
event fragment at the same time. Each data source should consequently
have a localization tag and a time tag for each data fragment it
provides. This fragment are published by a DimService and is centrally
collected to build an event, i.e a collection of data fragment with an
identical time tag.

The event builder proposed is not itself distributed, it is unique and
so no geographical or time dependent partitioning is available. All data
sources are collected by a unique process. It subscribes to all
available data source and writes them on receipt in a shared memory. A
separate thread scans the memory and build the event, i.e a collection
of data buffer from each registered data source with the same time tag.
A last thread process completed event and call registered *event
processors* that can write event to disk in user defined format. The
next sections detail the software tools provided to achieve those tasks.

##### 3.1.1. The buffer structure {#sec-the-buffer-structure .h3 data-heading-depth="3" style="display:block"}

The *levbdim::buffer* class is a simple data structure that provides
space to store the read data (the *payload*) and four tags:

-   The *Detector ID*: A four bytes id used to tag different
    geographical partition (ex. tracker barrel, ECAL end cap…)

-   The *source ID*: A four bytes id characterizing the data source (ex.
    ADC or TDC number)

-   The *event number*: A four bytes number of readouts

-   The *bunch crossing number*: an eight bytes number characterizing
    the time of the event in clocks count. The clock is the typical
    clock of the experiment. It may allows a finer events building if
    needed. It si currently optional

-   The *payload*: a bytes array containing detector data for the given
    *event number* readout.

-   The *payload size*: the size of the *payload* array.

The payload can be compressed and inflate on the fly using the
gzip[[4](#gzip "gzip reference")] library calls (*compress,uncompress*
methods).

Buffer do not need to be allocated by data producers since they are
embedded in the *levbdim::datasource* class described bellow.

##### 3.1.2. Server side: The datasource class {#sec-server-side--the-datasource-class .h3 data-heading-depth="3" style="display:block"}

A *levbdim::datasource* object instantiates a *levbdim::buffer* with a
maximal size specified. It creates also the associated DimService with
the name **/FSM/LEVBDIM/DS-X-Y/DATA** where X is the detector id and Y
the data source one. The buffer has method to access the payload in
order to allow the user process to fill it.

The server process can instantiate any number of *levbdim::datasource*
fill the associated buffers and publish them every time a new event is
collected. The only requirement is that a DimServer is started at the
initialization phase. The following pseudo code gives an example of
usage.

###### Initialization {#sec-initialization .clearnum .h4 data-heading-depth="4" style="display:block"}

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
  // Header
 std::vector<levbdim::datasource*> _sources;
  // Initialisation
  // Starting the DIM server
  std::stringstream s0;
  s0.str(std::string());
  s0<<"dummyServer-"<<name;
  DimServer::start(s0.str().c_str()); 

  // Configuration
  // Loop on data sources
   std::cout <<"Creating datasource Detector="<<det<<"  SourceId="<<sid<<std::endl;
   // Maximum buffer size is set to 128 kBytes
   levbdim::datasource* ds= new levbdim::datasource(det,sid,0x20000);
   _sources.push_back(ds);
```

###### Readout loop {#sec-readout-loop .clearnum .h4 data-heading-depth="4" style="display:block"}

This is an example with one thread per data source but all data source
can be filled and publish sequentially if needed.

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
  // Create a readout thread per data source
  // calling the readdata method 
  void dummyServer::start()
  {
    _running=true;
    for (std::vector<levbdim::datasource*>::iterator ids=_sources.begin();ids!=_sources.end();ids++)
      {
      _gthr.create_thread(boost::bind(&dummyServer::readdata, this,(*ids)));
      ::usleep(500000);
      }
  }
  // Loop  on Events and publish data
  void dummyServer::readdata(levbdim::datasource *ds)
  {
  // Filling the buffer with random data
  // evt and bx are global data
  std::srand(std::time(0));
  while (_running)
    {
      ::usleep(10000);
      if (!_running) break;
      if (evt%100==0)
        std::cout<<"Thread of "<<ds->buffer()->dataSourceId()<<" is running "<<evt<<" "<<_running<<std::endl;
      // Just fun , ds is publishing a buffer containing sourceid X int of value sourceid
      uint32_t psi=ds->buffer()->dataSourceId();
      
      uint32_t* pld=(uint32_t*) ds->payload();
      for (int i=0;i<psi;i++) pld[i]= std::rand();
      pld[0]=evt;
      pld[psi-1]=evt;
      // publishing data of current event  number evt and bunch crossing number bx
      ds->publish(evt,bx,psi*sizeof(uint32_t));
    }
  // Running is set false by the stop method , thread exits
  std::cout<<"Thread of "<<ds->buffer()->dataSourceId()<<" is exiting"<<std::endl;
}  
```

As one can see the *levbdim::datasource* provides an easy way to store
event data and publish them on the fly.

##### 3.1.3. Client side: The datasocket class {#sec-client-side--the-datasocket-class .h3 data-heading-depth="3" style="display:block"}

On client side the process should subscribe to the datasource published
it wants to collect. It is achieved by using *levbdim::datasocket*
class. It instantiates a *DimInfo* object that subscribe to the
*DimService* named **/FSM/LEVBDIM/DS-X-Y/DATA** published by the
*levbdim::datasource*. It's a *DimClient* and it has an *infoHandler*
method that will collect the publish data. The data are then accessible
in the *levbdim::buffer*. This method can also be overwritten to
implement user treatment but the preferred usage is to save received
data to */dev/shm* so it can be used by the event builder class. This is
achieved at initialization in an analog way to data sources

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
  std::cout <<"Creating datasocket "<<det<<" "<<sid<<std::endl;
  // data socket storing data in buffer of same size 
  levbdim::datasocket* ds= new levbdim::datasocket(det,sid,0x20000);
  // Save data in share memory directory
  ds->save2disk("/dev/shm/levbdim/");
  _sources.push_back(ds);
```

The needed directory */dev/shm/levbdim* and */dev/shm/levbim/closed*
needed by the method are not created by the code and MUST be created by
the user. Real disks directory can obviously be used but performances
may be deeply affected since */dev/shm* is memory disk and can provide
much faster access.

In this example, each *datasocket* object will write on reception a
binary file named

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
  Event_#det_#source_#event_#bx
```

in */dev/shm/levbdim* containing the buffer. Once this
one-event,one-source data are written, it creates an empty file with the
same name in the */dev/shm/levbdim/closed* directory. By scanning this
later directory, the event builder will be able to read back the
completed data in the memory files.

This mechanism decoupled the data reception from the event building.

##### 3.1.4. Event building: The shmdriver class {#sec-event-building--the-shmdriver-class .h3 data-heading-depth="3" style="display:block"}

Finally the event collection is done asynchronously by the
*levbdim::shmdriver* class. The class registers data source identified
by their *detectorId* and their *dataSourceId* at initialization. It
also registers *levbdim::shmprocessors* that will treat completed
events. Two threads are then created:

1.  The first one is doing the following task:
    -   listing the files in */dev/shm/levbdim/closed* directory
    -   reading the corresponding files in */dev/shm/levbdim/*
        directory. The data are stored in a map with the key being the
        event number, an *levbdim::buffer* is pushed in an associated
        vector for each data source read.
    -   erasing the read files in */dev/shm/levbdim* and *closed*
        directory

2.  The second one is looping on the map entries, If one entry collected
    the exact number of data sources registered, the entry is processed
    and then removed from the map.

The *shmprocessor* class is a pure virtual class implementing only three
methods:

-   *void start(uint32\_t run)* called at each new run start.
-   *void stop()* called at end of run
-   *void processEvent(uint32\_t key,std::vector dss)* called for each
    completed pair of event number and buffer list.

An example of processor is given in *levbdim::basicwriter* class which
is writing data in a binary format. Many processors can be registered
and will be called sequentially (data writing, monitoring,
visualization…).

The main limitation of LEVBDIM is that this event building is unique and
the computing not distributed. It is nevertheless possible to duplicate
the event building process on a different computer (for cpu consuming
task) but data from data sources are then sent twice and the bandwidth
might be affected if the event sizes are large.

The following pseudo code implements an example of event builder:

-   **configure**

    ``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
     // Delete existing datasockets
    for (std::vector<levbdim::datasocket*>::iterator it=_sources.begin();it!=_sources.end();it++)
        delete (*it);
    _sources.clear();

    // Now create the builder
    _evb= new levbdim::shmdriver("/dev/shm/levbdim");
    _evb->createDirectories();
    _evb->cleanShm();

    // register a processor
    _writer= new levbdim::basicwriter("/tmp");
    _evb->registerProcessor(_writer);

    // register datasockets
    // Loop on data source ids
    // ...
      std::cout <<"Creating datasocket "<<det<<" "<<sid<<std::endl;
      levbdim::datasocket* ds= new levbdim::datasocket(det,sid,0x20000);
      ds->save2disk("/dev/shm/levbdim/");
      _sources.push_back(ds);
      _evb->registerDataSource(det,sid);
    ```

-   **start**

    ``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
    // Starts the two shmdriver threads and pass the run number to the processors
    _evb->start(run);
    ```

-   **stop**

    ``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
    // stop the threads
    _evb->stop();
    ```

    #### 3.2. The Finite State machine {#sec-the-finite-state-machine .h2 data-heading-depth="2" style="display:block"}

##### 3.2.1. Structure of the FSM mechanism {#sec-structure-of-the-fsm-mechanism .h3 data-heading-depth="3" style="display:block"}

All data acquisition processes ( hardware readout, event builder, DB
interfaces, Slow control…) may have different states corresponding to
different phases of the read out process (OFF, READY, CONFIGURED,
RUNNING,STOPPED…). It is therefore compulsory to have a software
mechanism able to trigger the transition between the different states. A
DIM-based implementation is done with the *levbdim::fsm* class that
contains:

-   A vector of states names
-   A map of command names associated to a vector of
    *levbdim::fsmTransition*
-   A *DimRpc* object (*levbdim::rpcFsmMessage* class) handling the
    command reception and its processing

An *fsmTransition* is a class associating an initial state, a final
state and a command handler. The command handler is a boost functor with
a *levbdim::fsmmessage* parameter. As an example one can add a
transition to the fsm in a *wummyServer* class with the following code

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
  fsm->addTransition("START","CONFIGURED","RUNNING",boost::bind(&wummyServer::start,
     this,_1));
```

where

-   START is the command
-   CONFIGURED and RUNNING are the initial and final state
-   *start(levbdim::fsmmessage\* m)* is a method of the *wummyServer*
    class

The *levbdim::fsmmessage* is a class handling a *Json::Value* with two
attributes:

-   *command* : the command name
-   *content* : A possibly empty set of parameters

On return of the fsm handler one may add to the *content* set a
Json::Value named *answer* with the *setAnswer()* method.

On the client side an *levbdim::fsmClient* class is provided allowing
the controlling program to call the *execute(levbdim::fsmmessage\* m)*
method with the message containing the required command and parameters.

This structure is performing but required that the run control is
written using DIM Rpc implementation. In order to permit much lighter
clients we implements a web based version of the state machine.

##### 3.2.2. The FSM web {#sec-the-fsm-web .h3 data-heading-depth="3" style="display:block"}

The *levbdim::fsmweb* class is an overloading of the *fsm* class with a
mongoose-cpp [[2](#mongoose-cpp "https://github.com/Gregwar/mongoose-cpp")]
webserver on a port XXXX with three services:

1.  **htpp://mypc:XXXX/** returns the list of possible Finite State
    Machine commands, the list of standalone commands and the *PREFIX*
    used, in a JSON file format.
2.  **htpp://mypc:XXXX/PREFIX/FSM?command=NAME&content={…}** calls the
    FSM transition NAME with the *content* parameters set. The answered
    *levbdim::fsmmessage* is published in JSON format
3.  **htpp://mypc:XXXX/PREFIX/CMD?name=NAME&toto={…}** calls the boost
    callback registered in the *fsmweb* for the command named NAME.

    In the last case additional commands to the FSM ones can be
    registered with *addCommand* method:

    ``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
    _fsm->addCommand("DOWNLOAD",boost::bind(&wummyServer::download, this,_1,_2));
    ```

    will register the command named DOWNLOAD and link it to the boost
    functor. The method *download(Mongoose::Request &request,
    Mongoose::JsonResponse &response)* is called. The *request*
    parameter is an object embedding the CGI parameters, the *response*
    is a pure Json::value object that will be published as a JSON file
    on return.

    This approach allows to add commands not linked to state transition
    (monitoring,debug,..)

    ### 4. Full example of an FSM based process {#sec-full-example-of-an-fsm-based-process .h1 data-heading-depth="1" style="display:block"}

    #### 4.1. The wummyServer class {#sec-the-wummyserver-class .h2 data-heading-depth="2" style="display:block"}

    The class *wummyServer* is a dummy data server that will publish
    periodically random data. Its prototype is the following

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
class wummyServer {
public:
  wummyServer(std::string name,uint32_t port);
  void configure(levbdim::fsmmessage* m);
  void start(levbdim::fsmmessage* m);
  void stop(levbdim::fsmmessage* m);
  void halt(levbdim::fsmmessage* m);
  void readdata(levbdim::datasource *ds);
  void download(Mongoose::Request &request, Mongoose::JsonResponse &response);
  void list(Mongoose::Request &request, Mongoose::JsonResponse &response);
  void setEvent(uint32_t e) {_event=e;}
  void setBx(uint64_t b) {_bx=b;}
  uint32_t event(){return _event;}
  uint64_t bx(){return _bx;}
private:
  fsmweb* _fsm;
  std::vector<levbdim::datasource*> _sources;
  bool _running,_readout;
  boost::thread_group _gthr;
  uint32_t _event;
  uint64_t _bx;
}
```

###### Initialization {#sec-initialization .h4 data-heading-depth="4" style="display:block"}

The constructor instantiates the state machine, registers the states and
start the Web and DIM services.

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
wummyServer::wummyServer(std::string name,uint32_t port) : _running(false)
{
  _fsm=new fsmweb(name);

  // Register state
  _fsm->addState("CREATED");
  _fsm->addState("CONFIGURED");
  _fsm->addState("RUNNING");
  
  // Register allowed transition
  _fsm->addTransition("CONFIGURE","CREATED","CONFIGURED",
      boost::bind(&wummyServer::configure, this,_1));
  _fsm->addTransition("CONFIGURE","CONFIGURED","CONFIGURED",
      boost::bind(&wummyServer::configure, this,_1));
  _fsm->addTransition("START","CONFIGURED","RUNNING",
      boost::bind(&wummyServer::start, this,_1));
  _fsm->addTransition("STOP","RUNNING","CONFIGURED",
    boost::bind(&wummyServer::stop, this,_1));
  _fsm->addTransition("HALT","RUNNING","CREATED",
    boost::bind(&wummyServer::halt, this,_1));
  _fsm->addTransition("HALT","CONFIGURED","CREATED",
    boost::bind(&wummyServer::halt, this,_1));

  // Register additional commands
  _fsm->addCommand("DOWNLOAD",boost::bind(&wummyServer::download, this,_1,_2));
  _fsm->addCommand("LIST",boost::bind(&wummyServer::list, this,_1,_2));

  //Start Dim server
  std::stringstream s0;
  s0.str(std::string());
  s0<<"wummyServer-"<<name;
  DimServer::start(s0.str().c_str());
  
  // Start web services 
  _fsm->start(port);
}
```

##### 4.1.1. Handlers {#sec-handlers .h3 data-heading-depth="3" style="display:block"}

There is then handlers declared for each FSM or standalone commands

###### Configure transition {#sec-configure-transition .h4 data-heading-depth="4" style="display:block"}

In this configure transition there is only the registration of the data
sources. In real world it will also include the hardware configuration.

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::configure(levbdim::fsmmessage* m)
{
  std::cout<<"Received command "<<m->command()<<
    " with parameters "<<m->value()<<std::endl;
  // Delete existing datasources if any
  for (std::vector<levbdim::datasource*>::iterator it=_sources.begin();it!=_sources.end();it++)
      delete (*it);
  _sources.clear();
  // Add the data source
  // Parse the json message, the format used in the example is
  // {"command": "CONFIGURE", "content": {"detid": 100, "sourceid": [23, 24, 26]}}
  Json::Value jc=m->content();
  int32_t det=jc["detid"].asInt();
  const Json::Value& dss = jc["sourceid"];
  // Book a Json array to return the list of source booked
  Json::Value array_keys;
  // Loop on sources
  for (Json::ValueConstIterator it = dss.begin(); it != dss.end(); ++it)
    {
      const Json::Value& book = *it;
      int32_t sid=(*it).asInt();
      std::cout <<"Creating datasource "<<det<<" "<<sid<<std::endl;
      array_keys.append((det<<16)|sid);
      levbdim::datasource* ds= new levbdim::datasource(det,sid,0x20000);
      _sources.push_back(ds);
    }
  // Overwrite msg
  //Prepare complex answer
  m->setAnswer(array_keys);
}
```

###### Start transition {#sec-start-transition .h4 data-heading-depth="4" style="display:block"}

The handler is just starting a boost thread of data “reading” for each
data source

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::start(levbdim::fsmmessage* m)
{
    std::cout<<"Received "<<m->command()<<std::endl;
    _running=true;
    for (std::vector<levbdim::datasource*>::iterator ids=_sources.begin();ids!=_sources.end();ids++)
      {
        _gthr.create_thread(boost::bind(&wummyServer::readdata, this,(*ids)));
        ::usleep(500000);
      }
}
```

The *readdata* method is defined with the datasource argument. It's
typically the method that is interfacing the hardware readout of the
event data.

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::readdata(levbdim::datasource *ds)
{
  uint32_t last_evt=0;
  std::srand(std::time(0));
  while (_running)
    {
      ::usleep(10000);
      //      ::sleep(1);
      if (!_running) break;
      // Update data only when _event number is modified
      if (_event == last_evt) continue;
      if (evt%100==0)
        std::cout<<"Thread of "<<ds->buffer()->dataSourceId()<<" is running "<<
        last_evt<<" events, running status : "<<_running<<std::endl;
      // Just fun , ds is publishing a buffer containing sourceid X int of value sourceid
      uint32_t psi=ds->buffer()->dataSourceId();
      // update the payload
      uint32_t* pld=(uint32_t*) ds->payload();
      for (int i=0;i<psi;i++) pld[i]= std::rand();
      pld[0]=evt;
      pld[psi-1]=evt;
      // publish data
      ds->publish(_event,_bx,psi*sizeof(uint32_t));
    
     last_evt= _event;
    }
  std::cout<<"Thread of "<<ds->buffer()->dataSourceId()<<" is exiting"<<std::endl;
}
```

###### Stop transition {#sec-stop-transition .h4 data-heading-depth="4" style="display:block"}

The stop transition is just stopping the reading threads without
reconfiguring the process.

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::stop(levbdim::fsmmessage* m)
{    
  std::cout<<"Received "<<m->command()<<std::endl;
  // Stop running
  _running=false;
  ::sleep(1);
  std::cout<<"joining the threads"<<std::endl;
  _gthr.join_all();
}
```

###### Halt transition {#sec-halt-transition .h4 data-heading-depth="4" style="display:block"}

In the halt transition, the datasources are removed and a new
configuration is needed

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::halt(levbdim::fsmmessage* m)
{  
  std::cout<<"Received "<<m->command()<<std::endl;
  if (_running)
    this->stop(m);
  std::cout<<"Destroying data sources"<<std::endl;
  for (std::vector<levbdim::datasource*>::iterator it=_sources.begin();it!=_sources.end();it++)
   delete (*it);
  _sources.clear();
}
```

###### Standalone commands {#sec-standalone-commands .h4 data-heading-depth="4" style="display:block"}

They are dummy commands:

-   The DOWNLOAD can implement the download of database data for future
    configuration

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::download(Mongoose::Request &request, Mongoose::JsonResponse &response)
{
  std::cout<<"download "<<request.getUrl()<<" "<<request.getMethod()
    <<" "<<request.getData()<<std::endl;
   
  // Getting the data base name
  std::string state= request.get("DBSTATE","NONE")
  // if (state.compare("NONE")!=0) do the download
  std::stringstream os;
  os<<state<<" has been download"
  response["answer"]=os.str();
}
```

-   The LIST command can return the list of data sources

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
void wummyServer::list(Mongoose::Request &request, Mongoose::JsonResponse &response)
{
  std::cout<<"list "<<request.getUrl()<<" "<<request.getMethod()
    <<" "<<request.getData()<<std::endl;
  Json::Value array_keys;
  for (std::vector<levbdim::datasource*>::iterator ids=_sources.begin();ids!=_sources.end();ids++)
     array_keys.append(((*ids)->detectorId()<<16)|(*ids)->dataSourceId());
  response["answer"]=array_keys;
}
```

#### 4.2. Main program {#sec-main-program .h2 data-heading-depth="2" style="display:block"}

The main program just instantiate the wummyServer object. In order to
emulate new event arrival the event number is updated every second

``` {.para-block .pre-fenced .pre-fenced3 .language-cpp .lang-cpp .cpp .colorized style="display:block"}
int main()
{
  wummyServer s("myfirsttry",45000);
  uint32_t evt=0;
  // Just update the event number to trigger data publication
  while (1)
    {
      ::sleep(1);
      s.setEvent(evt++);
    }
} 
```

### 5. The Run and process Control {#sec-the-run-and-process-control .h1 data-heading-depth="1" style="display:block"}

#### 5.1. Run Control {#sec-run-control .h2 data-heading-depth="2" style="display:block"}

With the web implementation of the finite state machine, the run control
is really eased and can be written with very light tools like the
*curl*[[7](#curl "curl ref")] library or python. One can ofcourse use
more user friendly interfaces like *Qt* [[8](#qt "QT ref")] or web based
graphical interfaces like *Wt*[[9](#wt "Wt ref")] or javascript pages,
since all these frameworks have http request capabilities. We will give
two examples in curl and python of the control of the *wummyServer*
process.

##### 5.1.1. curl example {#sec-curl-example .h3 data-heading-depth="3" style="display:block"}

For example using *curl*, one can **configure** the *wummyServer*
process with:

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
curl http://mypc:45000/unessai/FSM?command=CONFIGURE\&content=%7B\"detid\":100,\"sourceid\":%5
B12,13,14%5D%7D
```

and **start** a run with

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
curl http://mypc:45000/unessai/FSM?command=START\&content=%7B%7D
```

Access to the **download** standalone command is also easy

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
curl http://mypc:45000/unessai/CMD?name=DOWNLOAD\&state=MyDbStateForMyDetetector
```

The configuration and the control of all daq processes is then a simple
bash script that can be run on any computer where *curl* is installed.

##### 5.1.2. python example {#sec-python-example .h3 data-heading-depth="3" style="display:block"}

We used *urllib* and *urllib2* and *socket* package (SOCKS support) and
define two methods:

-   FSM access

``` {.para-block .pre-fenced .pre-fenced3 .language-python .lang-python .python .colorized style="display:block"}
def executeFSM(host,port,prefix,cmd,params):
   if (params!=None):
       # build the URL
       myurl = "http://"+host+ ":%d" % (port)
       # parameter list
       lq={} 
       # content take values from the params python list
       lq["content"]=json.dumps(params,sort_keys=True)
       lq["command"]=cmd 
       # encode it          
       lqs=urllib.urlencode(lq)
       # Build the final url
       saction = '/%s/FSM?%s' % (prefix,lqs)
       myurl=myurl+saction
       # send the command
       req=urllib2.Request(myurl)
       r1=urllib2.urlopen(req)
       return r1.read()
```

-   Command access

``` {.para-block .pre-fenced .pre-fenced3 .language-python .lang-python .python .colorized style="display:block"}
def executeCMD(host,port,prefix,cmd,params):
   if (params!=None and cmd!=None):
       myurl = "http://"+host+ ":%d" % (port)
       # CGI parameter list
       lq={}
       lq["name"]=cmd
       for x,y in params.iteritems():
           lq[x]=y
       # build the list
       lqs=urllib.urlencode(lq)
       saction = '/%s/CMD?%s' % (prefix,lqs)
       myurl=myurl+saction
       req=urllib2.Request(myurl)
       # Check the server is alived
       try:
           r1=urllib2.urlopen(req)
       except URLError, e:
           p_rep={}
           p_rep["STATE"]="DEAD"
           return json.dumps(p_rep,sort_keys=True)
       else:
           return r1.read()
   else:
       # no additional parameters build the query 
       myurl = "http://"+host+ ":%d/%s/" % (port,prefix)
       req=urllib2.Request(myurl)
       # Check the server is alived
       try:
           r1=urllib2.urlopen(req)
       except URLError, e:
           p_rep={}
           p_rep["STATE"]="DEAD"
           return json.dumps(p_rep,sort_keys=True)
       else:
           return r1.read()
```

Using those two methods one can build any request to a *fsmweb* based
process. The usage of python allows the developers to build a much more
complex code architecture and benefit from a rich library resources
(XML,JSON configuration for example).

#### 5.2. Large architecture approach {#sec-large-architecture-approach .h2 data-heading-depth="2" style="display:block"}

The LEVBDIM package was developed to read the Semi Digital HCAL of the
Calice collaboration[[6](#calice "CALICE ref")]. It handles a large
number of processes for readout (14), database access (1), trigger
control (2), low voltage control (1) and event building(1). The
configuration sequence is complex and can be coded in python but the
control is then bind to a single python process and a single user. We
preferred to have a hierarchical approach and created an upper
*levbdim::fsmweb* process (*WDaqServer*) with a relatively simple finite
state machine (INITIALISE,CONFIGURE,START,STOP,DESTROY). This process
implements hierarchical (or parallel for readout) lower level FSM
transitions via web or DIM message calls. It's a intrinsically a web
process and can be accessed remotely by various client.

The most user-friendly client is a *Wt* based web service that is
accessing the *WDaq* server. Several clients can be connected and the
processing is driven by the *WDaq* FSM.

#### 5.3. The process management {#sec-the-process-management .h2 data-heading-depth="2" style="display:block"}

One last requirement of a distributed acquisition system is the
management of the different processes (start,kill,restart). Heavily
inspired by the *XDAQ* [[10](#xdaq "XDAQ ref")] framework jobcontrol, we
developed one application *levbdim::fsmjob* that is started on every
computer used in the Daq. On request to this application a list of
processes are forked and the specified programs are created (or killed/
restarted). The child processes are writing in PID identified log that
can be requested to the application. The management of the *fsmjob*
itself can be done using Linux daemon mechanism that will ensure that it
is started during the computer boot sequence.

##### 5.3.1. Process description {#sec-process-description .h3 data-heading-depth="3" style="display:block"}

The process are described with the following structure:

``` {.para-block .pre-fenced .pre-fenced3 .language-javascript .lang-javascript .javascript .colorized style="display:block"}
  {
   "NAME":"WRITER",
   "ARGS" : ["-d /data/NAS/oyonax"],
   "ENV" : [
           "DIM_DNS_NODE=lyosdhcal9","LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:
           /opt/dhcal/levbdim/lib:/opt/dhcal/lib:/opt/dhcal/root/lib:
           /opt/dhcal/dim/linux:/opt/dhcal/lcio/v02-00/lib/:$LD_LIBRARY_PATH"
            ],
            "PROGRAM" : "/opt/dhcal/bin/levbdim_build"
   }
```

where

-   NAME is a user given name of the process. Since sevral jobs can be
    started one the same computer the name has to be unique on agiven
    computer
-   PROGRAM is the executable
-   ARGS is an array of argument to the executable
-   ENV is an array of environment variables to be set when the program
    runs.

The state machine of the *fsmjob* is described in
figure [1](#fig-fsmjob "Finite State machine of the levbdim::fsmjob").
All jobs on a givern PC must be registered. The first possibility is to
send an INITIALISE command with a local *file* name or an *url* name in
the *content* argument. The file is read or load and parsed. The jobs
corresponding to the computer name are registered. An example of the
file structure is given here:

``` {.para-block .pre-fenced .pre-fenced3 style="display:block"}
{
    "HOSTS" :
    {
        "lyosdhcal9":[
            {
            "NAME":"WRITER",
            "ARGS" : ["-d /data/NAS/oyonax"],
            "ENV" : [
                "DIM_DNS_NODE=lyosdhcal9",
                "LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:
                /opt/dhcal/levbdim/lib:/opt/dhcal/lib:
                /opt/dhcal/root/lib:$LD_LIBRARY_PATH"
            ],
            "PROGRAM" : "/opt/dhcal/bin/levbdim_build"
            },
            {
            "NAME":"WIENER",...
            },
            {
            "NAME":"DBSERVER",
            "ARGS" : [],...
            }
            ],
        "lyoilcrpi17":[

            {   "NAME":"MDCSERVER",
                "ARGS" : [],...
                }
             ]
  }
```

It's a map of HOSTS (computer's names) containing a list of processes
with the previously described process's structure. In order to use this
initialisation, the file should be copied locally on each computer or
put in an accessible web page.

Another possibility is to use the REGISTERJOB transition and to register
each job independantly:

1.  Send a REGISTRATION command. It clears previous job list and prepare
    a map of jobs
2.  Send any number of REGISTERJOB commands with parameters of *content*
    being JSON strings:
    -   *processname* for NAME tag
    -   *processargs* for ARGS tag
    -   *processenv* for ENV tag
    -   *processbin* for PROGRAM tag

3.  Send a ENDREGISTRATION to terminate the initialisation process

Once the process is INITIALISED one can send a START command to start
all process on the PC and a KILL command to kill them. A DESTROY command
put the daemon back in CREATED state.

Additionnal standalone commands are available:

Command

Parameters

Action

STATUS

No

Returns the list and status of all processes

with per process the HOST,NAME,PID and STATUS

(Running/DEAD)

KILLJOB

*pid*, *processname*,*signal*(9)

kill the specified process

RESTART

*pid*, *processname*,*signal*(9)

restart the specified process

JOBLOG

*pid*, *processname*,*lines*(100)

return the last *lines* of the log of the specified job

![fsmjob\_fsm](data:image/png;base64,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AEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAABA0AAAAAIGgAAAAAIGgAAAAAIGgAAAAAAEEDAAAAAEEDAAAAAEEDAAAAAAgaAAAAAAgaAAAAAAgaAAAAAEDQAAAAAEDQAAAAAEDQAAAAAEDQAAAAAACCBgAAAACCBgAAAACCBgAAAAAQNAAAAAAQNAAAAAAQNAAAAACAoAEAAACAoAEAAACAoAEAAAAABA0AAAAABA0AAAAAL3jQcKAYAAAAAMR00AAAAAAAggYAAAAAggYAAAAAggYAAAAAEDQAAAAAEDQAAAAAEDQAAAAAgKABAAAAgKABAAAAgKABAAAAAAQNAAAAAAQNAAAAAAQNAAAAACBoAAAAACBoAAAAACBoAAAAAABBAwAAAABBAwAAAABBAwAAAECC9P+7O/LsnR9HdAAAAABJRU5ErkJgggA= "fsmjob_fsm")

* * * * *

**Figure 1.** Finite State machine of the *levbdim::fsmjob*

##### 5.3.2. Process control client {#sec-process-control-client .h3 data-heading-depth="3" style="display:block"}

Again it is free to the developper to write its own client using any web
library. We personally used python since the JSON format is easily
interfaced to the language structure and the network library simple. C++
(with jsoncpp and curl) is also used. A python example is given in the
example directory.

### 6. Conclusion {#sec-conclusion .h1 data-heading-depth="1" style="display:block"}

[1] C Gaspar, *DIM*, [🔎](http://www.bing.com/search?q=+Gaspar+_DIM_++)

[2]https://github.com/Gregwar/mongoose-cpp\_ [🔎](http://www.bing.com/search?q=_https+github+Gregwar+mongoose+cpp_++)

[3]https://github.com/cesanta/mongoose [🔎](http://www.bing.com/search?q=_https+github+cesanta+mongoose_++)

[4]GZIP ref [🔎](http://www.bing.com/search?q=)

[5]JSON ref [🔎](http://www.bing.com/search?q=)

[6]Calice [🔎](http://www.bing.com/search?q=+calice)

[7]curl [🔎](http://www.bing.com/search?q=+calice)

[8]QT [🔎](http://www.bing.com/search?q=+qt)

[9]Witty [🔎](http://www.bing.com/search?q=+wt)

[10]XDAQ [🔎](http://www.bing.com/search?q=+xdaq)


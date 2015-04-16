//
//  FLServerHttp.h
//  
//
//  Created by Sarah Denoux on 13/05/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

// FLServer wraps another httpPage (typically a httpdInterface) in a droppable page that can send a new source to FaustLive

#ifndef _FLSERVERHTTP_h
#define _FLSERVERHTTP_h

#define kBusyPage   "<html><body>FaustLive Server is busy, please try again later.</body></html>"
#define kErrorPage "<html>\n<body>ERROR</body>\n</html>"
#define kErrorCompile1 "<html>\n<body>\nImpossible to compile this file : \n"
#define kErrorCompile2 "\n</body>\n</html>"

#include <sstream>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <microhttpd.h>

#include <map>
#include <vector>
#include <string>
#undef min
#undef max
#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif

using namespace std;

#define POSTBUFFERSIZE 512

#define GET 0
#define POST 1

struct connection_info_struct {
    int connectiontype; // GET or POST
    struct MHD_PostProcessor *postprocessor; // the POST processor used internally by microhttpd
    int answercode; // used internally by microhttpd to see where things went wrong or right
    
    string data;
    string compilationOptions;
    string winUrl;  //To be able to replace faust content in the right FLWindow
    std::string answerstring; // the answer sent to the user after upload
};

class FLServerHttp : public QObject
{
    
    private:

        Q_OBJECT
        
        int             fMax_clients;
        string          fError;         // Not important right now
        string          fUrl;           // Url of wrapped http page 
        
        bool            fPosted;        // Post request completed
        bool            fCompiled;      // Compilation sucess
        
        string          fServerAddress;
        
        string          fJson;
        string          fHtml;
        
        string          fHome;
        
        int             handleGet(MHD_Connection *connection, const char* url);
        int             handlePost(MHD_Connection *connection, const char* url, void *info);
    
        map<int, string>     fDeclaredNames;
        
        static FLServerHttp*    _serverInstance;
    
    public:
    
        static int      fNr_of_uploading_clients;
        
        struct          MHD_Daemon* fDaemon;
        
                        FLServerHttp();
                        virtual ~FLServerHttp();
        
        
        static FLServerHttp*    _Instance();
        static void             createInstance(const string& homeFolder);
        static void             deleteInstance();
        
        int             getMaxClients();
        
        bool            start();
        void            stop();

        int             sendPage(struct MHD_Connection *connection, const char *page, int length, int status_code, const char * type = 0);
        
        static int      answerToConnection(void *cls, struct MHD_Connection *connection,
                                         const char *url, const char *method,
                                         const char *version, const char *upload_data,
                                         size_t *upload_data_size, void **con_cls);
        
        static void requestCompleted(void *cls, MHD_Connection *connection, void **con_cls, MHD_RequestTerminationCode toe);
        
        static int iteratePost(void *coninfo_cls, MHD_ValueKind kind, const char *key, const char *filename, const char *content_type, const char *transfer_encoding, const char *data, uint64_t off, size_t size);
        
        
        void            declareHttpInterface(int port, const string& name);
        void            removeHttpInterface(int port);
        
        int             redirectJsonRequest(struct MHD_Connection *connection, string portNumber);
        
        void            compileSuccessfull(const string& url);
        void            compileFailed(const string& error);
        
        void            updateAvailableInterfaces();
        
    signals:
        
        void        compile(const char*, int);
    
};

#endif




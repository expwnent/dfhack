
#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"
#include "VTableInterpose.h"

#include <pthread.h>
#include "ActiveSocket.h"
#include "PassiveSocket.h"
#include "SimpleSocket.h"

#include "modules/Screen.h"

#include "df/renderer.h"
#include "df/viewscreen.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_loadgamest.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

using namespace DFHack;
using namespace std;

static pthread_t serverListenThread;
static bool serverRunning = false;
struct Screenshot;
static Screenshot* recent;
static pthread_mutex_t screenshotMutex = PTHREAD_MUTEX_INITIALIZER;

command_result remoteDF (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("remoteDF");

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "remoteDF", "Do nothing, look pretty.",
        remoteDF, false, /* true means that the command can't be used from non-interactive user interface */
        // Extended help string. Used by CR_WRONG_USAGE and the help command:
        "  This command does nothing at all.\n"
        "Example:\n"
        "  remoteDF\n"
        "    Does nothing.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    pthread_cancel(serverListenThread);
    return CR_OK;
}



DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    return CR_OK;
}

struct ServerData {
    uint8_t* address;
    int16_t port;
};

struct Screenshot {
    int32_t xSize, ySize;
    Screen::Pen* data;

    virtual ~Screenshot() {
        if ( data != NULL ) {
            delete[] data;
            data = NULL;
        }
    }

    vector<df::coord2d>* getDifferences(Screenshot* shot) {
        if ( shot == NULL )
            return NULL;
        if ( xSize != shot->xSize || ySize != shot->ySize )
            return NULL;
        vector<df::coord2d>* resultPtr = new vector<df::coord2d>;
        vector<df::coord2d>& result = *resultPtr;
        for ( int16_t a = 0; a < xSize; a++ ) {
            for ( int16_t b = 0; b < ySize; b++ ) {
                Screen::Pen& pen1 = data[a*ySize+b];
                Screen::Pen& pen2 = shot->data[a*ySize+b];
                if ( pen1.ch == pen2.ch && pen1.fg == pen2.fg && pen1.bg == pen2.bg )
                    continue;
                //if ( pen1.ch == pen2.ch )
                    //continue;
                
                result.push_back(df::coord2d(a,b));
            }
        }
        return resultPtr;
    }

    stringstream* printDifferences(Screenshot* old) {
        vector<df::coord2d> *differences_ptr = getDifferences(old);
        if ( !differences_ptr ) {
            return print();
        }
        vector<df::coord2d>& differences = *differences_ptr;
        stringstream* stream_ptr = new stringstream(stringstream::binary | stringstream::out);
        stringstream& stream = *stream_ptr;

        int32_t count = (int32_t)differences.size();
        Core::print("Difference count = %d\n", count);
        stream.write((char*)&count, sizeof(count));
        for ( size_t a = 0; a < count; a++ ) {
            df::coord2d& pos = differences[a];
            stream.write((char*)&pos.x, sizeof(pos.x));
            stream.write((char*)&pos.y, sizeof(pos.y));
            size_t index = pos.x*ySize + pos.y;
            Screen::Pen& pen = data[index];
            stream.write((char*)&pen.ch, sizeof(pen.ch));
            stream.write((char*)&pen.fg, sizeof(pen.fg));
            stream.write((char*)&pen.bg, sizeof(pen.bg));
        }

        delete differences_ptr;
        return stream_ptr;
    }

    stringstream* print() {
        stringstream* stream_ptr = new stringstream(stringstream::binary | stringstream::out);
        stringstream& stream = *stream_ptr;
        
        size_t bob = -1;
        stream.write((char*)&bob, sizeof(bob));
        stream.write((char*)&xSize, sizeof(xSize));
        stream.write((char*)&ySize, sizeof(ySize));
        for ( int16_t a = 0; a < xSize; a++ ) {
            for ( int16_t b = 0; b < ySize; b++ ) {
                Screen::Pen& pen = data[a*ySize+b];
                stream.write((char*)&pen.ch, sizeof(pen.ch));
                stream.write((char*)&pen.fg, sizeof(pen.fg));
                stream.write((char*)&pen.bg, sizeof(pen.bg));
                //stream << pen.tile << pen.tile_fg << pen.tile_bg;
                //stream << pen.ch << pen.fg << pen.bg;// << pen.bold;
                //cout << "ch = " << pen.ch << " fg = " << (int32_t)pen.fg << " bg = " << (int32_t)pen.bg << " bold = " << (int32_t)pen.bold << " " << endl;
                //stream << pen.ch;
            }
        }
        return stream_ptr;
    }

    static Screenshot* getScreenshot() {
        Screenshot* screenshot = new Screenshot();
        df::coord2d size = Screen::getWindowSize();
        screenshot->xSize = size.x;
        screenshot->ySize = size.y;
        screenshot->data = new Screen::Pen[size.x*size.y];

        size_t index = 0;
        for ( int16_t a = 0; a < size.x; a++ ) {
            for ( int16_t b = 0; b < size.y; b++ ) {
                screenshot->data[index] = Screen::readTile(a,b);
                if ( screenshot->data[index].ch == 0 ) {
                    //screenshot->data[index].fg = screenshot->data[index].bg = 0;
                }
                index++;
            }
        }
        return screenshot;
    }
};

void* listenForConnections(void* arg) {
    ServerData* data = (ServerData*)arg;
    CPassiveSocket socket;
    CActiveSocket* client = NULL;
    const size_t bufferSize = 1024;
    uint8_t* buffer = new uint8[bufferSize];
    //uint8_t* buffer1 = buffer;
    //uint8_t* buffer2 = &buffer[bufferSize];
    memset(buffer, 0, bufferSize*sizeof(uint8_t));
    
    socket.Initialize();
    socket.Listen(data->address, data->port);

    //TODO: separate thread per client
    client = socket.Accept();
    if ( client == NULL ) {
        Core::print("%s, line %d: Null client.\n", __FILE__, __LINE__);
        exit(1);
    }

    /*
    int32_t bytesRead = client->Receive(bufferSize);
    if ( bytesRead < 0 ) {
        Core::print("%s, line %d: Neg bytes read.\n", __FILE__, __LINE__);
        exit(1);
    }
    
    memcpy(buffer, client->GetData(), bytesRead);
    Core::print("Server got message \"%s\"\n", buffer);
    */

    Screenshot* old = NULL;
    while(true) {
        Screenshot* screenshot;// = Screenshot::getScreenshot();
        pthread_mutex_lock(&screenshotMutex);
        if ( recent != NULL ) {
            screenshot = recent;
            recent = NULL;
            pthread_mutex_unlock(&screenshotMutex);
        } else {
            pthread_mutex_unlock(&screenshotMutex);
            continue;
            screenshot = Screenshot::getScreenshot();
        }
        stringstream* screen_ptr = screenshot->printDifferences(old);
        stringstream& screen = *screen_ptr;
        if ( old != NULL )
            delete old;
        old = screenshot;

        //const size_t responseSize = size.x*size.y*tileSize + 4;
        size_t responseSize = screen.tellp();
        if ( responseSize == 4 ) {
            delete screen_ptr;
            continue;
        }
        uint8* response = new uint8[responseSize];
        memset(response, 0, responseSize);
        string bob = screen_ptr->str();
        memcpy(response, bob.c_str(), responseSize);

        Core::print("thing = %d\n", *(int32_t*)response);

        //const uint8* response = (const uint8*)"I got your message.\n";
        int32_t bytesSent = 0;
        while ( bytesSent < responseSize ) {
            client->Select();
            int32_t err = (int32_t)client->Send(response+bytesSent, responseSize-bytesSent);
            if ( err < 0 ) {
                Core::print("%d out of %d bytes sent.\n", bytesSent, responseSize);
                goto end;
            }
            bytesSent += err;
        }
        delete screen_ptr;
        //delete screenshot;
        delete[] response;
    }
end:

    client->Close();
    socket.Close();

    delete client;
    delete data;
    serverRunning = false;
    delete[] buffer;
}

struct ViewscreenWatcher : df::renderer {
    typedef df::renderer interpose_base;
    DEFINE_VMETHOD_INTERPOSE(void, render, ()) {
        pthread_mutex_lock(&screenshotMutex);
        if ( recent != NULL ) {
            //delete recent;
        } else {
            recent = Screenshot::getScreenshot();
        }
        pthread_mutex_unlock(&screenshotMutex);
        Core::print("asdf!\n");
        INTERPOSE_NEXT(render)();
        //wooo! do stuff
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(ViewscreenWatcher, render);

command_result remoteDF (color_ostream &out, std::vector <std::string> & parameters)
{
    static bool bob = false;
    if ( !bob ) {
       bob = true;
       if (!INTERPOSE_HOOK(ViewscreenWatcher, render).apply()) {
           out.print("%s, line %d.\n", __FILE__, __LINE__);
           exit(1);
       }
       return CR_OK;
    }
    if (!parameters.empty())
        return CR_WRONG_USAGE;
    
    if ( serverRunning ) {
        out.print("Server already running.\n");
        return CR_OK;
    }
    
    ServerData* data = new ServerData;
    data->address = (uint8_t*)"127.0.0.1";
    data->port = 10006;
    serverRunning = true;
    pthread_create(&serverListenThread, 0, listenForConnections, data);
    
    return CR_OK;
}

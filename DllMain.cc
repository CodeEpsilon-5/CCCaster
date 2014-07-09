#include "Log.h"
#include "Util.h"
#include "Event.h"
#include "Timer.h"

#include <windows.h>

#include <vector>
#include <memory>

#define LOG_FILE FOLDER "dll.log"

#define HOOK_CALL1_ADDR     ( ( char * ) 0x40D032 )
#define LOOP_START_ADDR     ( ( char * ) 0x40D330 )
#define HOOK_CALL2_ADDR     ( ( char * ) 0x40D411 )

using namespace std;

struct EventHandler : public Timer::Owner
{
    Timer timer;

    void timerExpired ( Timer *timer ) override
    {
        LOG ( "tick" );

        timer->start ( 1000 );
    }

    EventHandler() : timer ( this )
    {
        timer.start ( 1000 );
    }
};

static bool initialized = false;

static shared_ptr<EventHandler> eventHandler;

extern "C" void callback()
{
    if ( !initialized )
    {
        EventManager::get().initializePolling();
        eventHandler.reset ( new EventHandler() );
        initialized = true;
    }

    if ( !EventManager::get().poll() )
    {
        EventManager::get().deinitialize();
    }
}

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
        {
            Log::get().initialize ( LOG_FILE );
            LOG ( "DLL_PROCESS_ATTACH" );

            Asm hookCallback1
            {
                HOOK_CALL1_ADDR,
                {
                    0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - HOOK_CALL1_ADDR - 5 ),  // call callback
                    0xE9, INLINE_DWORD ( HOOK_CALL2_ADDR - HOOK_CALL1_ADDR - 10 )           // jmp HOOK_CALL2_ADDR
                }
            };

            Asm loopStartJump
            {
                LOOP_START_ADDR,
                {
                    0xE9, INLINE_DWORD ( HOOK_CALL1_ADDR - LOOP_START_ADDR - 5 ),           // jmp HOOK_CALL1_ADDR
                    0x90                                                                    // nop
                }
            };

            Asm hookCallback2
            {
                HOOK_CALL2_ADDR,
                {
                    0x6A, 0x01,                                                             // push 01
                    0x6A, 0x00,                                                             // push 00
                    0x6A, 0x00,                                                             // push 00
                    0xE9, INLINE_DWORD ( LOOP_START_ADDR - HOOK_CALL2_ADDR - 5 )            // jmp LOOP_START_ADDR+6
                }
            };

            hookCallback1.write();
            hookCallback2.write();
            loopStartJump.write();
            break;
        }

        case DLL_PROCESS_DETACH:
            LOG ( "DLL_PROCESS_DETACH" );
            Log::get().deinitialize();
            break;
    }

    return TRUE;
}
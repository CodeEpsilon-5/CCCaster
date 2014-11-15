#include "Controller.h"
#include "Logger.h"

#include <SDL.h>
#include <windows.h>

#include <cstdlib>

using namespace std;


#define DEFAULT_DEADZONE    25000

#define EVENT_JOY_AXIS      0
#define EVENT_JOY_HAT       1
#define EVENT_JOY_BUTTON    2

#define AXIS_CENTERED       0
#define AXIS_POSITIVE       1
#define AXIS_NEGATIVE       2


static string getVKeyName ( uint32_t vkCode, uint32_t scanCode, bool isExtended )
{
    switch ( vkCode )
    {
#include "KeyboardMappings.h"

        default:
            break;
    }

    if ( isExtended )
        scanCode |= 0x100;

    char name[4096];

    if ( GetKeyNameText ( scanCode << 16, name, sizeof ( name ) ) > 0 )
        return name;
    else
        return toString ( "Key Code 0x%02X", vkCode );
}

void Controller::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
    // Only handle keyboard events for mapping
    if ( !isDown && keyToMap != 0 )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( keyToMap & ( 1u << i ) )
            {
                keybd.codes[i] = vkCode;
                keybd.names[i] = getVKeyName ( vkCode, scanCode, isExtended );
            }
            else if ( keybd.codes[i] == vkCode )
            {
                keybd.codes[i] = 0;
                keybd.names[i].clear();
            }
        }

        key = keyToMap;
    }

    cancelMapping();

    if ( owner )
        owner->doneMapping ( this, key );
}

void Controller::joystickEvent ( const SDL_JoyAxisEvent& event )
{
    uint32_t *values = stick.mappings[EVENT_JOY_AXIS][event.axis];

    uint8_t value = 0;
    if ( abs ( event.value ) > deadzones[event.axis] )
        value = ( event.value > 0 ? AXIS_POSITIVE : AXIS_NEGATIVE );

    if ( keyToMap != 0 )
    {
        uint32_t *activeValues = active.mappings[EVENT_JOY_AXIS][event.axis];

        uint8_t activeValue = 0;

        if ( activeValues[AXIS_POSITIVE] )
            activeValue = AXIS_POSITIVE;
        else if ( activeValues[AXIS_NEGATIVE] )
            activeValue = AXIS_NEGATIVE;

        if ( value == 0 && activeValue )
        {
            // Done mapping if the axis returned to 0
            values[activeValue] = keyToMap;

            // Set bit mask for neutral value
            values[AXIS_CENTERED] = ( values[AXIS_POSITIVE] | values[AXIS_NEGATIVE] );

            LOG_CONTROLLER ( this, "Mapped axis%d %s to %08x",
                             event.axis, ( activeValue == AXIS_POSITIVE ? "+" : "-" ), keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue )
            return;

        activeValues[value] = keyToMap;
        return;
    }

    state &= ~values[AXIS_CENTERED];

    if ( value != AXIS_CENTERED )
        state |= values[value];

    LOG_CONTROLLER ( this, "axis=%d; value=%s; EVENT_JOY_AXIS",
                     event.axis, ( value == 0 ? "0" : ( value == AXIS_POSITIVE ? "+" : "-" ) ) );
}

static int ConvertHatToNumPad ( int hat )
{
    int dir = 5;

    if ( hat != SDL_HAT_CENTERED )
    {
        if ( hat & SDL_HAT_UP )
            dir = 8;
        else if ( hat & SDL_HAT_DOWN )
            dir = 2;

        if ( hat & SDL_HAT_LEFT )
            --dir;
        else if ( hat & SDL_HAT_RIGHT )
            ++dir;
    }

    return dir;
}

void Controller::joystickEvent ( const SDL_JoyHatEvent& event )
{
    uint32_t *values = stick.mappings[EVENT_JOY_HAT][event.hat];

    if ( keyToMap != 0 )
    {
        uint32_t *activeValues = active.mappings[EVENT_JOY_HAT][event.hat];

        uint8_t activeValue = 0;

        if ( activeValues[SDL_HAT_UP] )
            activeValue = SDL_HAT_UP;
        else if ( activeValues[SDL_HAT_RIGHT] )
            activeValue = SDL_HAT_RIGHT;
        else if ( activeValues[SDL_HAT_DOWN] )
            activeValue = SDL_HAT_DOWN;
        else if ( activeValues[SDL_HAT_LEFT] )
            activeValue = SDL_HAT_LEFT;

        if ( event.value == SDL_HAT_CENTERED && activeValue )
        {
            // Done mapping if the hat is centered
            values[activeValue] = activeValues[activeValue];

            // Set bit mask for centered value
            values[SDL_HAT_CENTERED] = 0;
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_UP];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_RIGHT];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_DOWN];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_LEFT];

            LOG_CONTROLLER ( this, "Mapped hat%d %d to %08x",
                             event.hat, ConvertHatToNumPad ( activeValue ), keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue )
            return;

        activeValues[event.value] = keyToMap;
        return;
    }

    state &= ~values[SDL_HAT_CENTERED];

    if ( event.value & SDL_HAT_UP )
        state |= values[SDL_HAT_UP];
    else if ( event.value & SDL_HAT_DOWN )
        state |= values[SDL_HAT_DOWN];

    if ( event.value & SDL_HAT_LEFT )
        state |= values[SDL_HAT_LEFT];
    else if ( event.value & SDL_HAT_RIGHT )
        state |= values[SDL_HAT_RIGHT];

    LOG_CONTROLLER ( this, "hat=%d; value=%d; EVENT_JOY_HAT", event.hat, ConvertHatToNumPad ( event.value ) );
}

void Controller::joystickEvent ( const SDL_JoyButtonEvent& event )
{
    if ( keyToMap != 0 )
    {
        uint32_t *activeStates = active.mappings[EVENT_JOY_BUTTON][event.button];
        bool isActive = ( activeStates[SDL_PRESSED] );

        if ( event.state == SDL_RELEASED && isActive )
        {
            // Done mapping if the button was tapped
            stick.mappings[EVENT_JOY_BUTTON][event.button][SDL_PRESSED]
                = stick.mappings[EVENT_JOY_BUTTON][event.button][SDL_RELEASED] = keyToMap;

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", event.button, keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( isActive )
            return;

        activeStates[event.state] = keyToMap;
        return;
    }

    uint32_t key = stick.mappings[EVENT_JOY_BUTTON][event.button][event.state];

    if ( key == 0 )
        return;

    if ( event.state == SDL_RELEASED )
        state &= ~key;
    else if ( event.state == SDL_PRESSED )
        state |= key;

    LOG_CONTROLLER ( this, "button=%d; value=%d; EVENT_JOY_BUTTON", event.button, ( event.state == SDL_PRESSED ) );
}

Controller::Controller ( KeyboardEnum ) : name ( "Keyboard" )
{
    memset ( &guid, 0, sizeof ( guid ) );

    clearMapping();

    // TODO get default keyboard mappings from game
}

Controller::Controller ( SDL_Joystick *joystick ) : joystick ( joystick ), name ( SDL_JoystickName ( joystick ) )
{
    SDL_JoystickGUID guid = SDL_JoystickGetGUID ( joystick );
    memcpy ( &this->guid.guid, guid.data, sizeof ( guid.data ) );

    auto it = guidBitset.find ( this->guid.guid );

    if ( it == guidBitset.end() )
    {
        this->guid.index = 0;
        guidBitset[this->guid.guid] = 1u;
    }
    else
    {
        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( ( it->second & ( 1u << i ) ) == 0u )
            {
                guidBitset[this->guid.guid] |= ( 1u << i );
                this->guid.index = i;
                return;
            }
        }

        LOG_AND_THROW_STRING ( "Too many duplicate guids for: '%s'", this->guid.guid );
    }

    clearMapping();

    for ( auto& v : deadzones )
        v = DEFAULT_DEADZONE;

    // Default axis stick.mappings
    stick.mappings[EVENT_JOY_AXIS][0][0] = MASK_X_AXIS;
    stick.mappings[EVENT_JOY_AXIS][0][AXIS_POSITIVE] = BIT_RIGHT;
    stick.mappings[EVENT_JOY_AXIS][0][AXIS_NEGATIVE] = BIT_LEFT;
    stick.mappings[EVENT_JOY_AXIS][1][0] = MASK_Y_AXIS;
    stick.mappings[EVENT_JOY_AXIS][1][AXIS_POSITIVE] = BIT_DOWN; // SDL joystick Y-axis is inverted
    stick.mappings[EVENT_JOY_AXIS][1][AXIS_NEGATIVE] = BIT_UP;
    stick.mappings[EVENT_JOY_AXIS][2][0] = MASK_X_AXIS;
    stick.mappings[EVENT_JOY_AXIS][2][AXIS_POSITIVE] = BIT_RIGHT;
    stick.mappings[EVENT_JOY_AXIS][2][AXIS_NEGATIVE] = BIT_LEFT;
    stick.mappings[EVENT_JOY_AXIS][3][0] = MASK_Y_AXIS;
    stick.mappings[EVENT_JOY_AXIS][3][AXIS_POSITIVE] = BIT_DOWN; // SDL joystick Y-axis is inverted
    stick.mappings[EVENT_JOY_AXIS][3][AXIS_NEGATIVE] = BIT_UP;

    // Default hat stick.mappings
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_CENTERED] = ( MASK_X_AXIS | MASK_Y_AXIS );
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_UP]       = BIT_UP;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_RIGHT]    = BIT_RIGHT;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_DOWN]     = BIT_DOWN;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_LEFT]     = BIT_LEFT;
}

Controller::~Controller()
{
    auto it = guidBitset.find ( guid.guid );

    if ( it == guidBitset.end() )
        return;

    if ( guid.index >= 32 )
        return;

    guidBitset[guid.guid] &= ~ ( 1u << guid.index );
}

string Controller::getMapping ( uint32_t key ) const
{
    if ( isKeyboard() )
    {
        for ( uint8_t i = 0; i < 32; ++i )
            if ( key & ( 1u << i ) && keybd.codes[i] )
                return keybd.names[i];

        return "";
    }

    // TODO joystick
    return "";
}

void Controller::startMapping ( Owner *owner, uint32_t key, const void *window )
{
    cancelMapping();

    LOG ( "Starting mapping %08x", key );

    this->owner = owner;
    keyToMap = key;

    if ( isKeyboard() )
        KeyboardManager::get().hook ( this, window );
}

void Controller::cancelMapping()
{
    KeyboardManager::get().unhook();

    owner = 0;
    keyToMap = 0;

    for ( auto& a : active.mappings )
    {
        for ( auto& b : a )
        {
            for ( auto& c : b )
                c = 0;
        }
    }
}

void Controller::clearMapping ( uint32_t keys )
{
    for ( uint8_t i = 0; i < 32; ++i )
    {
        if ( keys & ( 1u << i ) )
        {
            keybd.codes[i] = 0;
            keybd.names[i].clear();
        }
    }

    for ( auto& a : stick.mappings )
    {
        for ( auto& b : a )
        {
            for ( auto& c : b )
            {
                if ( c & keys )
                    c = 0;
            }
        }
    }
}

inline static bool isPowerOfTwo ( uint32_t x )
{
    return ( x != 0 ) && ( ( x & ( x - 1 ) ) == 0 );
}

bool Controller::isOnlyGuid() const
{
    if ( isKeyboard() )
        return true;
    else
        return isPowerOfTwo ( guidBitset[this->guid.guid] );
}

unordered_map<Guid, uint32_t> Controller::guidBitset;

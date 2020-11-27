// https://godbolt.org/z/sTfPdE
#include <avr/io.h>
#include <stdbool.h>
#define F_CPU 8000000ul
#include <util/delay.h>

using u8 = uint8_t;
using u16 = uint16_t;
#define SA static auto
#define SCA static constexpr auto

/*---------------------------------------------------------------------
    mcu specific pins
---------------------------------------------------------------------*/
//mcu specific, normally in a mcu specific header
namespace PINS {
    enum PIN {  //mega328p
        B0,B1,B2,B3,B4,B5,B6,B7,
        C0,C1,C2,C3,C4,C5,C6,
        D0=16,D1,D2,D3,D4,D5,D6,D7
    };
}

/*---------------------------------------------------------------------
    generic Pin enums
---------------------------------------------------------------------*/
namespace PINS {
    enum INVERT { HIGHISON, LOWISON };
    //normaly more enums here- like
    //enum IOMODE { IMNPUT, OUTPUT };
    //etc.
}

/*---------------------------------------------------------------------
    Pin (pin specific)
---------------------------------------------------------------------*/
template<PINS::PIN Pin_, PINS::INVERT Inv_ = PINS::HIGHISON>
struct Pin {

    SCA pin_            { Pin_%8 };         //0-7
    SCA port_           { Pin_/8 };         //0-n         

    struct Reg { 
        u8:pin_; u8 IN :1; u8:7-pin_; 
        u8:pin_; u8 DIR:1; u8:7-pin_;
        u8:pin_; u8 OUT:1; u8:7-pin_;
    };

    //gcc 9.2.0, c++17, use inline reference
    static inline volatile Reg& reg{ *reinterpret_cast<Reg*>(port_*3+(int)&PINB) };

    SA  high        ()  { reg.OUT = 1; }  
    SA  low         ()  { reg.OUT = 0; } 
    SA  on          ()  { if(Inv_) low(); else high(); }  
    SA  off         ()  { if(Inv_) high(); else low(); }    
    SA  toggle      ()  { reg.IN = 1; }
    SA  output      ()  { reg.DIR = 1; }

    //more ...

};

/*---------------------------------------------------------------------
    main
---------------------------------------------------------------------*/
using namespace PINS; //so do not have to use PINS::

int main(void) {

    Pin<B7, LOWISON> led;

    led.off();
    led.output();

    while(true){
        led.toggle();
        _delay_ms( 500 );
    }
}

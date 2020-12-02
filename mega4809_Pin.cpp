// https://godbolt.org/z/KGo5WW
/*---------------------------------------------------------------------
    simple c++ example
    (online compiler cannot use >gcc5.4.0 with a mega4809 as headers
     only available in 5.4.0, so cannot use c++17 features such as
     inline vars)
---------------------------------------------------------------------*/
#include <avr/io.h>
#include <stdbool.h>

using u8 = uint8_t;
using u16 = uint16_t;
#define SA static auto
#define SCA static constexpr auto

/*---------------------------------------------------------------------
    PINS namespace - mcu specific pin enum
    in a mcu specific header
---------------------------------------------------------------------*/
//#pragma once
//#include atmega4809_48pin.hpp
namespace PINS {
    enum PIN { 
        A0=0,A1,A2,A3,A4,A5,A6,A7, 
        B0=8,B1,B2,B3,B4,B5,
        C0=16,C1,C2,C3,C4,C5,C6,C7,
        D0=24,D1,D2,D3,D4,D5,D6,D7,
        E0=32,E1,E2,E3,
        F0=40,F1,F2,F3,F4,F5,F6
    };
}

/*---------------------------------------------------------------------
    PINS namespace - general pin enums for Port
    in namespace so can use 'using' to bring them all in wherever
        you are, or access as PINS:: if you do not want to
---------------------------------------------------------------------*/
namespace PINS {
    enum IOMODE  { INPUT, OUTPUT, ANALOG };
    enum INVERT  { HIGHISON, LOWISON };
    enum PULLUP  { PULLUPOFF, PULLUPON };
    enum ISCMODE { INTDISABLE, BOTHEDGES, RISING, FALLING, INPUT_DISABLE, LEVEL };
    //set init pin val, normally set to off 
    //(USART TXD may want to use 'INITON', for example if set before
    // usart is enabled so is in idle state when pin set to output)
    enum INITVAL { INITOFF, INITON } ;
}

/*---------------------------------------------------------------------
    Pin
---------------------------------------------------------------------*/
template<PINS::PIN Pin_>
struct Pin {
    
    //==========
        private:
    //==========

    //constants

    SCA baseAddrV_ { Pin_/8 * 4 };                  //Vportn base address
    SCA baseAddr_  { Pin_/8 * 0x20 + 0x400 };       //Portn base address
    SCA pin_       { Pin_%8 };                      //0-7

    //register structs

    struct Vport { //*b is bit version
        union { u8 DIR;       struct { u8 :pin_; u8 DIRb :1;     u8 :7-pin_; }; };
        union { u8 OUT;       struct { u8 :pin_; u8 OUTb :1;     u8 :7-pin_; }; };
        union { u8 IN;        struct { u8 :pin_; u8 INb  :1;     u8 :7-pin_; }; };
        union { u8 INTFLAGS;  struct { u8 :pin_; u8 INTFLAGb :1; u8 :7-pin_; }; };
    };
    struct Pinctrl {
        union { u8 PINCTRL; struct { u8 ISC :3; u8 PULLUP :1; u8 unused :3; u8 INVEN  :1; }; };
    };    

    // init struct for init_ functions

    struct initT {
        Pinctrl PINCTRL; //store pinctrl config
        bool DIR;        //store dir
        bool VAL;        //store init value
    };  

    //==========
        public:
    //==========

    //register struct access
    
    //public to allow register access
    //  Pin<PA0>::vport->OUT = 0b10101010; //write to vportA multiple pins
    //  or for any other reason direct register access is wanted

    //this online compiler does not support using gcc >5.4.0 when a mega4809
    //is specified as the mcu, so this is what you can do if C++17 available-
    //static inline volatile Vport&   vport  { *reinterpret_cast<Vport*>(baseAddrV_) }; 
    //static inline volatile Pinctrl& pinctrl{ *reinterpret_cast<Pinctrl*>(baseAddr_+0x10) };

    //without C++17, we have to init these outside the struct, which as you will see
    //looks quite ugly because templates involved :(
    static volatile Vport&   vport;
    static volatile Pinctrl& pinctrl; 

    //==========
        private:
    //==========

    //init template functions, so can provide pin init options in 
    //any order, and the final register writes are consolidated

            template<typename ...Ts>
SCA init_   (initT& it, PINS::INITVAL e, Ts... ts) { 
                it.VAL = e; 
                init_(it, ts...); 
            }
            template<typename ...Ts>
SCA init_   (initT& it, PINS::PULLUP e, Ts... ts) { 
                it.PINCTRL.PULLUP = e;
                init_(it, ts...); 
            }
            template<typename ...Ts>
SCA init_   (initT& it, PINS::IOMODE e, Ts... ts) {
                it.DIR = (e == PINS::OUTPUT); //set dir only if output
                if(e == PINS::ANALOG) it.PINCTRL.ISC = PINS::INPUT_DISABLE;
                //if in/out, override isc if previously set to input disable for some reason
                else if(it.PINCTRL.ISC == PINS::INPUT_DISABLE) it.PINCTRL.ISC = PINS::INTDISABLE;
                init_(it, ts...); 
            }
            template<typename ...Ts>
SCA init_   (initT& it, PINS::INVERT e, Ts... ts) { 
                it.PINCTRL.INVEN = e;
                init_(it, ts...); 
            }
            template<typename ...Ts>
SCA init_   (initT& it, PINS::ISCMODE e, Ts... ts) { 
                it.ISC = e; 
                init_(it, ts...); 
            }
            //no more arguments, set from accumulated values
SCA init_   (initT& it) { 
                if( it.PINCTRL.ISC bitand 3 ) clearFlag(); //if any irq mode, clear flag
                pinctrl.PINCTRL = it.PINCTRL.PINCTRL;
                if(it.DIR) on(it.VAL);
                ioMode(PINS::IOMODE(it.DIR));
            }

    //==========
        public:
    //==========

    //constructors

                //no constructor (no init, manually call init)
    Pin         () {}
                //constructor, specify option(s)
                template<typename ...Ts>
    Pin         (Ts... ts) { init( ts... ); }

    //init
                //manual init, specify option(s)
                template<typename ...Ts>
SCA init        (Ts... ts) { 
                    initT it{};
                    init_( it, ts... );
                }

SA  deinit      (){ analog(); }                
 
    //io mode

SA  output      ()  { vport.DIRb = 1; }
SA  input       ()  { vport.DIRb = 0; } 
SA  analog      ()  { pinctrl.PINCTRL = PINS::INPUT_DISABLE; input(); }
SA  ioMode      (PINS::IOMODE e) { 
                    if     ( e == PINS::INPUT )  input(); 
                    else if( e == PINS::OUTPUT ) output(); 
                    else                         analog();
                }

    //pinctrl properties

SA  invertOn    ()  { pinctrl.INVEN = 1; }
SA  invertOff   ()  { pinctrl.INVEN = 0; }
SA  pullupOn    ()  { pinctrl.PULLUP = 1; }
SA  pullupOff   ()  { pinctrl.PULLUP = 0; }
SA  inMode      (PINS::ISCMODE e) {
                    pinctrl.PINCTRL.ISC = e;
                }

    //irq flags

SA  clearFlag   ()  { vport.INTFLAGb = 1; }
SA  isFlag      ()  { return vport.INTFLAGb; }  

    //pin state

SA  on          ()          { vport.OUTb = 1; }       
SA  off         ()          { vport.OUTb = 0; } 
SA  on          (bool tf)   { if(tf) on(); else off(); } 
SA  toggle      ()          { vport.INb = 1; }  
SA  isOn        ()          { return vport.INb; }
SA  isOff       ()          { return not isOn(); }

    // ... more functions

};
//without C++17 inline variables, we need to do this to init the
//register access references
template<PINS::PIN Pin_>
volatile typename Pin<Pin_>::Vport& Pin<Pin_>::vport{ *reinterpret_cast<Vport*>(baseAddrV_) }; 
template<PINS::PIN Pin_>
volatile typename Pin<Pin_>::Pinctrl& Pin<Pin_>::pinctrl{ *reinterpret_cast<Pinctrl*>(baseAddr_+0x10) };
 


/*---------------------------------------------------------------------
    inline delay using _delay_ms
---------------------------------------------------------------------*/
#define F_CPU 3333333ul
#include <util/delay.h>
template<typename T> 
SCA waitms(const T v) { _delay_ms(v); } 


using namespace PINS;
/*---------------------------------------------------------------------
    main
---------------------------------------------------------------------*/
int main(void) {

    Pin<A0> sw { LOWISON, INPUT, PULLUPON }; //options in any order
    Pin<B2> led{ OUTPUT, LOWISON };
   
    while( sw.isOff() ); //press sw to start

    while(true){
        led.toggle();
        waitms( 500 );
    }
}

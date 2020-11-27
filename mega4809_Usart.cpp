// https://godbolt.org/z/bGv8ad
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

//to allow anonymous unions (when -Wpedantic is on)
#define PEDANTIC_DISABLE \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#define PEDANTIC_RESTORE \
    _Pragma("GCC diagnostic pop")

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
                    pinctrl->PINCTRL.ISC = e;
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
volatile typename Pin<Pin_>::Vport& Pin<Pin_>::vport{
    *reinterpret_cast<Vport*>(baseAddrV_) }; 
template<PINS::PIN Pin_>
volatile typename Pin<Pin_>::Pinctrl& Pin<Pin_>::pinctrl{ 
    *reinterpret_cast<Pinctrl*>(baseAddr_+0x10) };
 





/*------------------------------------------------------------------------------
    USART0 - USART3 - ATmega4809 (48 Pin)
------------------------------------------------------------------------------*/
template<u8 N_, PINS::PIN Tx_, PINS::PIN Rx_, bool Alt_>
struct USART_INST {
    
    static_assert( N_ < 4, "valid USART numbers are 0-3" );
    SCA BASE_ADDR{ 0x800 + N_*0x20 };
    SCA RxD{ Rx_ }; 
    SCA TxD{ Tx_ };

    //PORTMUX.USARTROUTEA = 0x05E2, only allowing default/alt (0,1)
    //(2 unused, 3 is none and assuming is never set)
    SCA pmuxSet(){
        if( Alt_ ) ((u8*)(0x05E0))[2] or_eq (1<<(N_*2));
        else ((u8*)(0x05E0))[2] and_eq ~(1<<(N_*2));
    }
    SCA txdInit(){ Pin<TxD>::init( PINS::OUTPUT, PINS::INITON ); }
    SCA rxdInit(){ Pin<RxD>::init( PINS::INPUT, PINS::PULLUPON ); }

};
template<typename> struct Usart;
using Usart0    = Usart< USART_INST<0,PINS::A0,PINS::A1,0> >;
using Usart0alt = Usart< USART_INST<0,PINS::A4,PINS::A5,1> >;
using Usart1    = Usart< USART_INST<1,PINS::C0,PINS::C1,0> >;
using Usart1alt = Usart< USART_INST<1,PINS::C4,PINS::C5,1> >;
using Usart2    = Usart< USART_INST<2,PINS::F0,PINS::F1,0> >;
using Usart2alt = Usart< USART_INST<2,PINS::F4,PINS::F5,1> >;
using Usart3    = Usart< USART_INST<3,PINS::B0,PINS::B1,0> >;
using Usart3alt = Usart< USART_INST<3,PINS::B4,PINS::B5,1> >;



/*------------------------------------------------------------------------------
    Usart
------------------------------------------------------------------------------*/
template<typename Inst_>
struct Usart : Inst_ {

    // enums
    enum RXMODE { NORMAL, CLK2X, GENAUTO, LINAUTO };
    enum PMODE  { DISABLED, EVEN = 2, ODD };
    enum SBMODE { STOP1, STOP2 };

    //============
        private:
    //============

    struct UsartReg; //forward declare, registers are at end of struct

    //============
        public:
    //============

    //C++17
    // static inline volatile UsartReg&
    // reg { *(reinterpret_cast<UsartReg*>(Inst_::BASE_ADDR)) };

    // < C++17, init outside struct
    static volatile UsartReg& reg;

SA  isTxEmpty       ()          { return reg.DREIF; }            
SA  isTxFull        ()          { return not isTxEmpty(); }
SA  isTxComplete    ()          { return reg.TXCIF; }
SA  clearTxComplete ()          { reg.STATUS = 0x40; }
SA  isRxData        ()          { return reg.RXCIFd; }
SA  write           (u8 v)      { while( isTxFull() ); reg.TXDATAL = v; }
SA  read            (u8& v)     { 
                                    while( not isRxData() );
                                    u8 err = reg.RXDATAH bitand 0x46;
                                    v =reg.RXDATAL;
                                    return err;
                                }
                                //we want this all inline, as the compiler loses
                                //its mind for some reason here (not very optimized)
                                //always_inline seems to keep it optimal
                                [[gnu::always_inline]] 
SA  on              ()          {
                                    Inst_::pmuxSet();
                                    Inst_::txdInit();
                                    Inst_::rxdInit();
                                    reg.CTRLB = 0xC0; 
                                }                                
SA  rxMode          (RXMODE e)  { reg.RXMODE = e;}
SA  stopBits        (SBMODE e)  { reg.SBMODE = e; }
SA  parity          (PMODE e)   { reg.PMODE = e; }
SA  baudReg         (u16 v)     { reg.BAUD = v; }


    //============
        private:
    //============

    // registers
    PEDANTIC_DISABLE //turn off anonymous struct warning if -Wpedantic is on
    struct UsartReg {

                 u8 RXDATAL;
        union  { u8 RXDATAH;
        struct {    u8 DATA8    : 1;
                    u8 PERR     : 1;
                    u8 FERR     : 1;
                    u8          : 3;
                    u8 BUFOVF   : 1;
                    u8 RXCIFd   : 1;
        };};
                 u8 TXDATAL;
                 u8 TXDATAH; //1bit
        union  { u8 STATUS;
        struct {    u8 WFB      : 1;
                    u8 BDF      : 1;
                    u8          : 1;
                    u8 ISFIF    : 1;
                    u8 RXSIF    : 1;
                    u8 DREIF    : 1;
                    u8 TXCIF    : 1;
                    u8 RXCIF    : 1;
        };};
        union  { u8 CTRLA;
        struct {    u8 RS485    : 2;
                    u8 ABEIE    : 1;
                    u8 LBME     : 1;
                    u8 RXSIE    : 1;
                    u8 DREIE    : 1;
                    u8 TXCIE    : 1;
                    u8 RXCIE    : 1;
        };};
        union  { u8 CTRLB;
        struct {    u8 MPCM     : 1;
                    u8 RXMODE   : 2;
                    u8 ODME     : 1;
                    u8 SFDEN    : 1;
                    u8          : 1;
                    u8 TXEN     : 1;
                    u8 RXEN     : 1;
        };};
        union  { u8 CTRLC;
        struct {    u8          : 1;
                    u8 UCPHA    : 1;
                    u8 UDORD    : 1;
            };
        struct {    u8 CHSIZE   : 3;
                    u8 SBMODE   : 1;
                    u8 PMODE    : 2;
                    u8 CMODE    : 2;
        };};
        union  { u16 BAUD;
        struct {    u8 BAUDL;
                    u8 BAUDH;
        };};
        union  { u8 DBGCTRL;
        struct {    u8 DBGRUN   : 1;
        };};
        union  { u8 EVCTRL;
        struct {    u8 IREI     : 1;
        };};
                 u8 TXPLCTRL;
                 u8 RXPLCTRL; //7bits

    };
    PEDANTIC_RESTORE

};
//without C++17 inline variables, we need to do this to init the
//register access references
template<typename Inst_>
volatile typename Usart<Inst_>::UsartReg& Usart<Inst_>::reg { 
    *(reinterpret_cast<UsartReg*>(Inst_::BASE_ADDR)) };




using namespace PINS;
/*---------------------------------------------------------------------
    main
---------------------------------------------------------------------*/
int main(void) {

    Usart0 u0;
    u0.baudReg( 64 );
    u0.on();

    while(true){
        u8 c; //used for storing read/write char
        //if read without error, write back the same
        if( u0.read(c) ) u0.write(c);
    }
}

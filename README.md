# A Standard C++ Peripheral Style

#### my style, anyway
----------
#### The purpose here is a show a simple C++ style which can be used for any mcu and any peripheral. It will be assumed C++17 features are available.

#### A general outline of the peripheral class-

```
template<...something...>
struct Name { //struct defaults to public access, class can also be used if wanted

    public:
    //named enums needed to use this peripheral

    private:
    //constants (static constexpr)
    //register struct

    public:
    //register struct access var, publicly available

    //constructor(s)
    //init function

    //all other functions
    //all functions static
};
```
----------
**I will start with a simple example creating a Pin class for a ATmega328p, and you will see that the outline above is only a _general_ outline, which changes as required. The example is incomplete, but still works.**


**first, some headers**
```
#include <avr/io.h> //we will use existing info for now
#include <stdint.h>
#include <stdbool.h>
```
**some types and defines to reduce typing**
```
using u8 = uint8_t;
using u16 = uint16_t;
#define SA static auto
#define SCA static constexpr auto
```
**and in this case, we want a PINS namespace to hold the mcu specific pin information. The namespace is used so we do not pollute the global namespace, and it also can be brought into use with a _using_ statement in addition to the normal method of access- PINS:: (we need the creation outside the Pin class as these will be used as template arguments)**
```
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
```
**and also in the same PINS namespace, other enums that are either used as a template argument, or would be nice to be able to bring in simply with the _using_ statement**
```
/*---------------------------------------------------------------------
    generic Pin enums
---------------------------------------------------------------------*/
namespace PINS {
    enum INVERT { HIGHISON, LOWISON };
    //normaly more enums here- like
    //enum IOMODE { IMNPUT, OUTPUT };
    //etc.
}
```
**With the above in place, a Pin class can be started. In this case the template will be setup to accept the PINS::PIN argument, and whether an inverted state is wanted (on=high=HIGHISON, or on=low=LOWISON).**

**This avr has no invert mode (like an avr0/1), so we need to keep track of the invert via the Pin type (its templates). The template information travels everywhere the Pin instance goes (it is now a specific type), which is how the compiler can know all about the Pin instance, and none of that information needs to be stored on the mcu.**
```
/*---------------------------------------------------------------------
    Pin (pin specific)
---------------------------------------------------------------------*/
template<PINS::PIN Pin_, PINS::INVERT Inv_ = PINS::HIGHISON>
struct Pin {
    private:
```
**Since the PINS namespace will hold the needed enums, there are no public enums in this case.**

**We can now create constants to make usage easier. Using constexpr is no cost so use when wanted. Here we calculate the pin number and port number from the Pin_ template argument.**
```
    SCA pin_            { Pin_%8 };         //0-7
    SCA port_           { Pin_/8 };         //0-n         
````
**The register layout in this case is quite simple. There are 3 registers in use for a pin, and since we only need to access a specific bit we can isolate the 3 bits by using bitfields. Since pin_ is a constexpr, it can be used as a value to pad both ends where the named bit will end up in the right location. Since we now have direct bit access, there is no need for a pin bitmask (in this case).**
```
    struct Reg { 
        u8:pin_; u8 IN :1; u8:7-pin_; 
        u8:pin_; u8 DIR:1; u8:7-pin_;
        u8:pin_; u8 OUT:1; u8:7-pin_;
    };
```
**Now we need a way to access the Reg struct above. In C++17 we can use an inline var to do the job. This will also have public access to allow direct use of the Reg struct if needed (in this case since we only have bits, it would be of little use but the Reg struct can be changed to also allow the byte wide register access also).**

**A static inline volatile reference to Reg, with a name of _reg_, and the Reg base address is a port set of registers (offset starting at PINB in this case).**
```
    public:

    //gcc 9.2.0, c++17, using static inline reference
    static inline volatile Reg& reg{ *reinterpret_cast<Reg*>(port_*3+(int)&PINB) };
```
**Everything is in place, so can now write some functions. Since we already did the hard work (but was easy), these functions end up being quite simple. In this case we separate the on/off from high/low since we introduced the INVERT template parameter. At Pin creation is the only time we need to be concerned about what state is considered on and off. The if(Inv_) optimizes away ro a single function call which in this case results in a simple sbi or cbi instruction,**
```
    SA  high        ()  { reg.OUT = 1; }  
    SA  low         ()  { reg.OUT = 0; } 
    SA  on          ()  { if(Inv_) low(); else high(); }  
    SA  off         ()  { if(Inv_) high(); else low(); }    
    SA  toggle      ()  { reg.IN = 1; }
    SA  output      ()  { reg.DIR = 1; }

    //more ...

};
```
**Finally, put the Pin class to use. Create a Pin using the info we want (which pin, and its _on_ state). Now access to any of the class functions are available.**
```
/*---------------------------------------------------------------------
    main
---------------------------------------------------------------------*/
int main(void) {

    Pin<B7, LOWISON> led;

    led.off();
    led.output();

    while(true){
        led.toggle();
        _delay_ms( 500 );
    }
}

```
[above code on godbolt.org](https://godbolt.org/z/Y9jM3c)

----------

The same idea can be extended to any peripheral and any mcu. Here is an example for a mega4809 which takes the above and expands on it. It adds the ability to set a pins properties as arguments in any order, either at creation or by calling an init function later. The options are accumulated and applied when there are no more arguments to process.

[more dvanced example for a mega4809](https://godbolt.org/z/67cT1e)

# A Standard C++ Peripheral Style

#### my style, anyway
#### This file describes a mega328, the mega4809.md will decribe the same showing chnages needed from this description.
----------
#### The purpose here is a show a simple C++ style which can be used for any mcu and any peripheral. It will be assumed C++17 features are available.

[code on godbolt.org](https://godbolt.org/z/jbexW9)

#### A general outline of the peripheral class-

```
template<...something...>
struct Name { //struct defaults to public access, class can also be used if wanted

    public: //if using class instead of struct
    named enums needed to use this peripheral (frequently viewed)

    private:
    constants (static constexpr)
    register struct (or just its declaration)

    public:
    register struct access var
    
    private:
    any private functions, vars
    
    public:
    constructor(s)
    init function(s)

    all other functions
    all functions static
    
    register struct (if only defined earlier, so we can keep it out of the way)
};
```
#### The bottom line- we want a struct to contain what is needed to use a peripheral. We need register access at the level needed to make our functions easy and readable, which normally means using bitfields when a register is not a single purpose register. When this register access is setup correctly, we can then create enums as required to match some of these registers and bitfields. Although when producing app code the peripheral function contents are not important, it does allow for an easy read when you want to check out what a function actually does, and also allows easy changes when needed. The use of templates allows easily duplicating the peripheral for as many peripheral instances the mcu has (templates are not the only way, but is an efficient way). Since the peripheral driver is contained in a single struct which contains all its definitions, it will be inline and allows the peripheral driver to be just a header (the use of templates also makes it inline, and templates are typically header only).

----------
**I will start with a simple example creating a Pin class for a ATmega328p, and you will see that the outline above is only a _general_ outline, which changes as required. The example is incomplete, but still works. The terms _struct_ and _class_ are interchangable so I may use one or the other term that is describing the same thing, sometimes _class_ sounds/writes better so may end up writing _class_ when referring to a _struct_ or vice-versa.**


**first, we will be using just a single item from the mcu header (PINB) so will include io.h, although a manufacturer header is not necessarily required as you may end up with your own peripheral register layout as a struct inside the peripheral class**
```
#include <avr/io.h>
...
```
**some types and defines to reduce typing**
```
using u8 = uint8_t;
using u16 = uint16_t;
#define SA static auto
#define SCA static constexpr auto
```
**and in this case, we want a PINS namespace to hold the mcu specific pin information. The namespace is used so we do not pollute the global namespace, and it also can be brought into use with a _using_ statement in addition to the normal method of access- PINS:: (we need the creation outside the Pin class as these will be used as template arguments). The PIN values are 0 based, and each port letter starts at the offset of previousPort+8. This means the lowest 3 bits contain the pin number (0-7) and the other bits store the port letter (0=B, 1=C, 2=D). So, in effect 0bPPPPPnnn, where PPPPP is the port (0 to max ports-1, which is 2 in this case), and NNN is the pin number. If the mcu happens to have a max of 16 or 32 pins per port, then the nnn and PPPPP gets adjusted to match.**
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
**Also in the same PINS namespace, other enums are created that are either used as a template argument to the Pin struct, or would be nice to be able to bring in simply with the _using_ statement when using the Pin struct (such as setting pin properties), and of course are related to all the other things in the PINS namespace. This example is minimal, and the only needed enum is for the invert template argument.**
```
/*---------------------------------------------------------------------
    generic Pin enums
---------------------------------------------------------------------*/
namespace PINS {
    enum INVERT { HIGHISON, LOWISON };
    //normaly more enums here- like
    //enum IOMODE { INPUT, OUTPUT };
    //etc.
}
```
**With the above in place, a Pin class can be started. In this case the template will be setup to accept the PINS::PIN argument, and whether an inverted state is wanted (on=high=HIGHISON, or on=low=LOWISON).**

**This avr, like most mcu's, has no invert mode (an avr0/1 is an exception), so we need to keep track of the invert via the Pin type (its templates). The template information travels everywhere the Pin instance goes (it is now a specific type), which is how the compiler can always know all about the Pin instance, and none of that information needs to be stored on the mcu.**
```
/*---------------------------------------------------------------------
    Pin (pin specific)
---------------------------------------------------------------------*/
template<PINS::PIN Pin_, PINS::INVERT Inv_ = PINS::HIGHISON>
struct Pin {
```
**Since the PINS namespace will hold the needed enums, there are no public enums created in this case, but would normally go here (public). The top of the struct is a good place for these, since looking up the available enum options is done frequently.**

**We can now create constants to make usage easier. Here we calculate the pin number and port number from the Pin_ template argument, and can now use those values instead of having the division/remainder calculations show up in our functions. The compiler can figure it all out at compile time, so there is no cost to using them.**
```
    private:
    SCA pin_            { Pin_%8 };         //0-7
    SCA port_           { Pin_/8 };         //0-n         
````
**The register layout in this case is quite simple. There are 3 registers in use for a pin (port), and since we only need to access a specific bit we can isolate the 3 bits by using bitfields. Since pin_ is a constexpr, it can be used as a value to pad both ends of the bitfield where the named bit will end up in the right location. A 0 sized bitfield is valid, and signifies an alignement which in this case is 8bits, so if we end up with 0 on either end it is ok. Since we now have direct bit access, there is no need for a pin bitmask (in this case because of the avr instruction set, but a pinmask is most likely needed on other mcu's to use the set/clr/tog/flags type registers in an atomic way).**

**Normally, the register struct of most peripherals will have a register layout containing union/struct so the byte (or word) sized register can be accessed, along with bits or bit ranges by name within them. Usually, it is better to just declare the register struct here, then create it at the end of the struct so it stays out of the way.**
```
    struct Reg { 
        u8:pin_; u8 IN :1; u8:7-pin_; 
        u8:pin_; u8 DIR:1; u8:7-pin_;
        u8:pin_; u8 OUT:1; u8:7-pin_;
    };
```
**Now we need a way to access the Reg struct above. In C++17 we can use an inline var to do the job. This will also be public to allow direct use of the Reg struct if needed (in this case since we only have bits, it would be of little use but the Reg struct could be changed to also allow the byte wide register access also). If C++17 is not available, then check out the mega4809 md files in this github folder which end up with only a declaration in the struct, and needing to define the var outside the struct.**

**A static inline reference to a volatile Reg, with a name of _reg_, and the Reg address is an offset from what would be considered the base of all the ports, PINB in this case (each port uses 3 registers, all port registers are consecutive). The reference as a static inline variable effectively becomes something similar to a constexpr, where its use requires no storage on the mcu. The use of PINB to get the base address means we have to include _io.h_, but there is no reason the base address cannot be obtained in any other way- such as just using the known base address or creating a base address in a mcu specific header.**
```
    public:

    //gcc 9.2.0, c++17, using static inline reference
    static inline volatile Reg& reg{ *reinterpret_cast<Reg*>(port_*3+(int)&PINB) };
```
**Everything is in place, so can now write some functions. Since we already did the hard work (not very hard), these functions end up being quite simple. In this case we separate the on/off from high/low since we introduced the INVERT template parameter. At Pin creation is the only time we need to be concerned about what state is considered on and off. Which means for example you can turn an led on and have no need to remember if the on state is high or low. The _if(Inv_)_ optimizes away to a single function call which in this case optimizes to a simple sbi or cbi instruction.**
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
**Finally, put the Pin class to use. Create a Pin instance using the info we want (which pin from the mcu available PINS::PIN enum, and its _on_ state). Now access to any of the class functions are available.**
```
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

```


This same basic idea can be extended to any peripheral and any mcu. See the mega4809 examples for more advanced use.

# A Standard C++ Peripheral Style

#### my style, anyway
----------

**STM32G031 Gpio example**

**The original README.md should be read along with mega4809_Pin.md to get a background on where we came from, although we will not be using templates in this version.**

----------

**The PINS::PIN enum is the same as described before, and the pins listed here are for an stm32g031k8 and would be created in an mcu specific header like _stm32g031k8.hpp_.**
```
/*-------------------------------------------------------------
    MCU specific pin enums, in a namespace so both can keep out
    of global namespace and can use with 'using namespace' if
    want to use enums without having to specify PINS::
--------------------------------------------------------------*/
namespace PINS {

                enum
PIN             {
                PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7,
                PA8, PA9, PA10, PA11, PA12, PA13, PA14, PA15,
                PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7,
                PB8, PB9,
                PC6 = 38, PC14 = 46, PC15,

                SWDIO = PA13, SWCLK = PA14, //+ any alias names
                PA = PA0, PB = PB0 //if want to use GpioPort class directly
                };

}
```
**The Gpio.hpp header starts off with the normal pragma once and an include that has common things needed by all files- some typdef's for things like u8,u32,etc, and an mcu specific include such as stm32g031k8.hpp which has our PIN enum (and also has PeripheralAddresses for this mcu).**

**We also have an addition to the PINS namespace to add Gpio properties in the form of enums. Since in the same namespace as PIN, we can bring in the PINS namespace and also get these enums so we have all PINS enums at our disposal without having to use PINS:: all the time. The downside is potential name collision, but these are probably going to be used more often so 'un-collide' the others into their own space.**

```
//Gpio.hpp
#pragma once

#include "MyStm32.hpp"

/*--------------------------------------------------------------
    enum additions to the PINS namespace for GpioPin use
--------------------------------------------------------------*/
namespace PINS {

                enum
MODE            { INPUT, OUTPUT, ALTERNATE, ANALOG };
                enum
OTYPE           { PUSHPULL, ODRAIN };
                enum
PULL            { NOPULL, PULLUP, PULLDOWN };
                enum
SPEED           { SPEED0, SPEED1, SPEED2, SPEED3 }; //VLOW to VHIGH
                enum
ALTFUNC         { AF0, AF1, AF2, AF3, AF4, AF5, AF6, AF7 };
                enum
INVERT          { HIGHISON, LOWISON };

};
```

**We now depart a little from the other examples and create a GpioPort class. The GpioPin class will inherit this class, and as noted in the intro we are no longer using templates.**

**This Port layer is not needed, but it makes sense since this is what the peripheral actually is. It also allows locking a group of pin directly, or manipulating a group of pins by having direct access to the port registers. These things can also be done without having a Port class, but then you go through a Pin class to manipulate a port.**

**The register struct RegPort is created according to the datasheet and the BSRR register is split into 2 names although we will only use the set part of the register (BSRsR). There are some references to mcu peripheral addresses that originate from the stm32g031k8.hpp header. The GpioPin class will use the port_ var in a few places so it is protected and not private.**
```
/*=============================================================
    GpioPort class
=============================================================*/
struct GpioPort : PeripheralAddresses {

//-------------|
    private:
//-------------|

                //gpio port register layout
                struct RegPort {
                u32 MODER; u32 OTYPER; u32 OSPEEDR; u32 PUPDR;
                u32 IDR;   u32 ODR;    u16 BSRsR;   u16 BSRrR;
                u32 LCKR;  u32 AFR[2]; u32 BRR;
                };

//-------------|
    protected:
//-------------|

                u8 port_; //port number(letter)

//-------------|
    public:
//-------------|

                //public, allows direct access to RegPort registers
                volatile RegPort& reg_; 

                II
GpioPort        (PINS::PIN pin)
                : port_( pin/16 ), //pin to port (16pins per port)
                  reg_( *(reinterpret_cast<RegPort*>( GPIO_BASE + GPIO_SPACING*(pin/16)) ) )
                {
                }

                II auto
enable          () { *(volatile u32*)RCC_IOPENB or_eq (1<<port_); }

                //lock pin(s) on this port (bitmask)
                II auto
lock            (u16 bm)
                {
                u32 vL = (1<<16) bitor bm;
                reg_.LCKR = vL;
                reg_.LCKR = bm;
                reg_.LCKR = vL;
                return (reg_.LCKR bitand vL) == vL;
                }

};
```
**Now we get to the GpioPin class. We inherit the GpioPort class so it is added in the struct declaration.**
```
/*=============================================================
    GpioPin class
=============================================================*/
struct GpioPin : GpioPort {
```
**Some private vars are needed to keep track of what pin we are on (0-15), our pinmask value for manipulating the atomic set/reset registers, and an invert bool so we can use on/off after initially declaring what those mean via the INVERT enum (default is HIGHISON).**

**These vars need storage for each instance, but if an instance is created in a local scope there will be no storage needed as the compiler will have no need to globally store the info.**
```
//-------------|
    private:
//-------------|

                u8 pin_;        //0-15
                u16 pinmask_;   //for bsr/bsrr
                bool invert_;   //so can do on/off
```
**The constructor takes in a PIN enum, the INVERT state which defaults to HIGHISON, and a bool to indicate whether its clock enable (in Rcc, for the port) needs to be set and is true by default. The latter allows using the class in a local scope without having to enable the port clock when you already know it is enabled. The constructor sets the private vars described previously according to the values passed in, and also provides GpioPort with the info it needs.**
```
//-------------|
    public:
//-------------|

                //no init, rcc clock enabled by default unless not wanted
                II
GpioPin         (PINS::PIN pin, PINS::INVERT inv = PINS::HIGHISON, bool clken = true)
                : GpioPort(pin), pin_(pin%16), pinmask_(1<<(pin%16)), invert_(inv)
                {
                if( clken ) enable();
                }
```
**Pin properties are set with the following functions. It can be seen that pin_ is used to find the bit or set of bits needed to set the property in a register. It would be nicer if these properties were in pin specific registers, but we deal with what they provide us.**

**Also note that we return a reference to this class so we can method chain which is handy to have in this case. Templates and parameter packs can also be used as shown in mega4809_Pin.cpp init_ functions, but in this case its not worth the trouble as we are now dealing with seperate registers for each property and method chaining can do the same thing, easier. If there was a single property register for each pin, then templates would be good as you could 'accumulate' all the wanted properties in a single argument list and then set the single register to the required value in a single write, where the value is calculate at compile time.**
```
                //properities
                II GpioPin&
mode            (PINS::MODE e)
                {
                reg_.MODER = (reg_.MODER bitand compl (3<<(2*pin_))) bitor (e<<(2*pin_));
                return *this;
                }

                II GpioPin&
outType         (PINS::OTYPE e)
                {
                if( e == PINS::ODRAIN ) reg_.OTYPER or_eq pinmask_;
                else reg_.OTYPER and_eq compl pinmask_;
                return *this;
                }

                II GpioPin&
pull            (PINS::PULL e)
                {
                reg_.PUPDR = (reg_.PUPDR bitand compl (3<<(2*pin_))) bitor (e<<(2*pin_));
                return *this;
                }

                II GpioPin&
speed           (PINS::SPEED e)
                {
                reg_.OSPEEDR = (reg_.OSPEEDR bitand compl (3<<(2*pin_))) bitor (e<<(2*pin_));
                return *this;
                }

                II GpioPin&
altFunc         (PINS::ALTFUNC e)
                {
                auto& ptr = reg_.AFR[pin_>7 ? 1 : 0];
                auto bm = compl (15<<(4*(pin_ bitand 7)));
                auto bv = e<<(4*(pin_ bitand 7));
                ptr = (ptr bitand bm) bitor bv;
                mode( PINS::ALTERNATE );
                return *this;
                }
```
**The lock function calls the GpioPort lock with our pinmask.**
```
                II GpioPin&
lock            () { GpioPort::lock(pinmask_); return *this; }
```
**If a pin needs to get back to its default state, deinit will do it. Notice we will also detect if we are an SWD pin and set accordingly. Also method chaining can be seen in use here, although the same can be done without.**
```
                //back to reset state- if reconfuring pin from an unknown state
                II GpioPin&
deinit          ()
                {
                mode(PINS::ANALOG).outType(PINS::PUSHPULL).altFunc(PINS::AF0)
                    .speed(PINS::SPEED0).pull(PINS::NOPULL);
                low();
                //if we are on the sw port, and is a sw pin
                //then we have a different reset state
                if ( port_ == (PINS::SWCLK/16) and (pin_ == (PINS::SWCLK bitand 15)) ) {
                    mode( PINS::ALTERNATE ).pull( PINS::PULLDOWN );
                    }
                if ( port_ == (PINS::SWCLK/16) and (pin_ == (PINS::SWDIO bitand 15)) ) {
                    mode( PINS::ALTERNATE ).pull( PINS::PULLUP ).speed( PINS::SPEED3 );
                    }
                return *this;
                }
```
**Probably not many uses for this, but if you have a pin instance and need to get its port/pin/pinmask for some reason, these will return what you want. It may be there is no use for this, but I have used it before although that was with an nRF52 which can use any pin for digital peripherals. If not used, there is no cost.**
```
                //get info for a GpioPin instance
                //(like a reverse lookup if you only have the name)
                II auto
port            () { return port_; }
                II auto
pin             () { return pin_; }
                II auto
pinmask         () { return pinmask_; }
```
**We finally get to the main purpose of this class which is to read and manipulate the pins. The reads are broken into two functions, one to get the pin value and the other to get the output value. The output value will be used later for toggle.**
```
                //read
                II auto
pinVal          () { return reg_.IDR bitand pinmask_; }
                II auto
latVal          () { return reg_.ODR bitand pinmask_; }
```
**The pin state functions all use the pinVal() function above, and the on/off versions take into account the invert value. Notice we only refer to the above two functions.**
```
                II auto
isHigh          () { return pinVal(); }
                II auto
isLow           () { return not isHigh(); }
                II auto
isOn            () { return ( invert_ == PINS::LOWISON ) ? isLow() : isHigh(); }
                II auto
isOff           () { return not isOn(); }
```
**The two base write functions use the pinmask_ var to set/reset a pin output atomically. We label these high/low because that is what we are doing here.**
```
                //write
                II GpioPin&
high            () { reg_.BSRsR = pinmask_; return *this; }
                II GpioPin&
low             () { reg_.BRR = pinmask_; return *this; }
```
**Now we can make some more write functions based off the two above functions. The on/off functions take into account the invert_ value, where the toggle function uses the latVal function to determine which state to set. The on function is overloaded so you can pass a bool to it and get either on/off, which is nicer to use as it avoids the need to create an if/else in your code.**
```
                II GpioPin&
on              () { invert_ == PINS::LOWISON ? low() : high(); return *this; }
                II GpioPin&
off             () { invert_ == PINS::LOWISON ? high() : low(); return *this; }
                II GpioPin&
on              (bool tf) { tf ? on() : off(); return *this; }
                II GpioPin&
toggle          () { latVal() ? low() : high(); return *this; }
                II GpioPin&
pulse           () { toggle(); toggle(); return *this; }

};
```

**Done. We now have a way to deal with pins, and if you look at the NUCLEO32_G031K8 project it can be seen in use in multiple ways including the setting up of pins in the Uart class. That project also shows how this would get put into a Gpio.hpp header and can be used for multiple stm32 mcu's.**

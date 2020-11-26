# A Standard C++ Peripheral Style

#### my style, anyway
----------

**ATMega4809 Pin example**
[more advanced example for a mega4809](https://godbolt.org/z/36fM3c)

**The above link is an example of something more advanced that builds on the simple example in the README.md file.**

**Since the online compiler does not have the ATmega4809 headers available for gcc 9.2.0, gcc 5.4.0 is used. Since 5.4.0 does not have C++17 support, it means the register access references cannot be initialized inside the struct and can look a little ugly to do the init outside of the struct when templates are involved. So the inline var feature is quite nice to have, but functions the same in either case.**

**Some differences and added features (from README.md) will be explained here.**

----------

**The PINS::PIN enum is the same as before, but all the available pins for the mega4809 were specified in this example.**

**Since this example adds setting pin properties, some other enums in the PINS namespace is added.**
```
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
```
**Inside the Pin struct, we have some constexpr values. We can get the base address of the PORT and VPORT peripherals from the Pin_ template argument. Since the PORTA peripheral address on every avr0/1 is 0x400 we can simply use that address as the base. VPORTA also is a known address (0). The offsets for each port is also consistent. The use of the PINS::PIN enum also means there is no need to assert anything, as you are restricted to using the PINS::PIN values at Pin creation (enums as a type eliminates a lot of asserting in C++).**
```
    SCA baseAddrV_ { Pin_/8 * 4 };                  //Vportn base address
    SCA baseAddr_  { Pin_/8 * 0x20 + 0x400 };       //Portn base address
    SCA pin_       { Pin_%8 };                      //0-7
```
**The register structs have changed to allow byte wide register access if wanted, although they are not needed here. Since VPORT can do anything PORT can do at the individual pin level except for the pin control register, only the PINnCTRL register is used from the PORT peripheral. The 6 CLR/SET/TGL registers in PORT are only useful when doing those operations on multiple pins at the same time, and is not really a common thing to do (or really even needed in many cases) so is simply left out for this example. It would be an easy thing to add access to these additional registers if wanted.**

```
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
```
**Since we will be doing some advanced pin init things, we will create a struct to _store_ init values until they are eventually translated into actual code that does some register writing. The Pinctrl struct can be reused, and the only other values that need temporary storage is the direction and initial pin state (if other than off).**

```
    // init struct for init_ functions

    struct initT {
        Pinctrl PINCTRL; //store pinctrl config
        bool DIR;        //store dir
        bool VAL;        //store init value
    };  
```
**Since we do not have C++17 inline variables in gcc 5.4.0 as stated earlier, these register access references can only be declared here, and the init will take place outside the Pin struct. You can find the initialization after the Pin struct, and it is not complicated to figure out even though it may look that way.**
```
    static volatile Vport&   vport;
    static volatile Pinctrl& pinctrl; 
```

**Now the fun.**

**The init_ functions take in a specific set of arguments and allow _any_ additional arguments to be passed on via the use of templates (with one exception). I'm sure my explanation here will fall short, but will attempt to explain this anyway.**

**Take the first init_ function, it will be 'called' when there is a match from a 'caller' that provides the arguments it wants. In this case, it takes an iniT&, a PINS::INITVAL enum value, and 0 or more additional arguments of any kind. The compiler will see a 'call' to this type of function (with the correct function name init_, of course), and sees there is a match, so this particluar init_ will be 'called'.**

**Now that this function is 'called', it will take the initT& and access the VAL member, setting it to the enum value provided. The initT& is passed around, and in this case it modified something in the struct then passed it on to the 'next' init_ with any additional arguments that came in.**

**This cycle repeats until there are no _additional_ arguments. If you look at the last init_ function, you will see it takes only a single argument- the initT& reference (and a template no longer needed as no longer dealing with additional argumets). When we end up there (because there was only the single initT& as an argument from a previous init_ call), it is time to take the accumulated information in initT and do the register writing. In this case, if any input mode (ISC) is set which is an irq mode, the flag is cleared. The pin control properties are set in a single write (inven, pullup, and isc), if the direction is output (1) then the pin initial value is also set (normally off- which can mean high or low depending on the pin control invert bit previously set), and finally the pin direction is set.**

**So, what essential happens after starting the init_ 'engine', is we bounce around these init_ functions until no more arguments are left, where it eventually gets to the final init_ where the actual work gets done. The init_ functions second argument has to be unique so the compiler can 'match' function calls to functions. In C++, an enum can be just as unique as any other type and is why this method works.**

```
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
```
**But how to start the init_ engine? An init function will be used that takes in 0 or more arguments, which is either called by the user, or is called when creating the Pin instance via the constructor.**

**The init function creates an initT struct, which is 0 initalized. Then it starts the init_ 'engine' by passing the initT struct as a reference along with any additional arguments.**

```
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
```

**So to put the Pin class to use, simply create a Pin and use it. If the Pin creation is in global space AND you provide init options, then you will get the pin setup via the global constructors list called in early startup code. If you would rather init a pin when and where you want, just provide the instance name only and call the init function when you want.**
```
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
```
**Here is the init call sequence of the sw Pin instance above. The only code that ends up on the mcu is the final register writes, the rest is done in the compiler at compile time.**
```
init( LOWISON, INPUT, PULLUPON );
init_( it, LOWISON, {INPUT, PULLUPON} );
init_( it, INPUT, {PULLUPON} );
init_( it, PULLUPON );
init_( it ); //done, now write registers
```

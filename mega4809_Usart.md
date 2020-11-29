# A Standard C++ Peripheral Style

#### my style, anyway
----------

**ATMega4809 Usart example**
[usart example for a mega4809](https://godbolt.org/z/bGv8ad)

**The above link is an example of something more advanced again that builds on the example in the mega4809_Pin.md file.**

**This time we will add a simple Usart class, using templates to help out. The mega4809 has peripherals such as the usart which have multiple instances that differ in base address and other things like pin use, vector numbers, and so on, but otherwise are the same. We can take advantage of C++ to easily account for those differences and produce a single class for the peripheral (usart in this case).**

**Since we are creating class templates and also creating static functions, the class we end up with does not result in reused code for each instance, rather we end up with a class that needs to be written only once. A peripheral's functions are typically quite simple, so there is really no downside to the result of having each instance creating its own code and the compiler is good at determining what is optimal in any case.**

**We first need a way to tell our Usart class about those differences mentioned earlier. This will not end up being complete, and there are other ways to do similar things, but the following example given will work for now.**

**A struct makes a good container, so a struct will be used to contain usart instance specific information which we will pass along as a template argument to the Usart class.**

**The template arguments for this instance class will take in the usart number 0-3 (which is static_assert at compile time), the tx and rx pins from our previously created PINS::PIN enum (already limited, so no assert needed), and whether we want to use the alternate pin set. We can store everything as static constexpr auto (auto works for most things), which is always ideal until the compiler is unhappy about its use (it will tell you when its not happy, of course).**

**The functions set the portmux value, and init the tx and rx pins. Although storing the Rx_ and Tx_ values seems unnecessary as the Rx_ and Tx_ template values could be used directly in the pin functions, this will allow the usart class to access a pin directly if needed. The portmux code is not pretty here but I didn't want to create another peripheral (Portmux) so I just manipulate the single register needed in this case (all avr0 mega's the same), and is why the N_ also works well here (to get to the right pair of bits in the portmux register for the usart we are interested in).**

```
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
```
**Now we have what we need to create all the Usart instances. First we have to declare the Usart struct and what it will look like (a template with a typename parameter). Then we simply create a 'using' name for each (like typedef) providing the information unique to each instance. We no longer have to concern ourselves about the details that make up Usart0, as we can now just use Usart0, or any of the others. So with this little amount of code, we have 8 different Usart variations and all we are left to do is choose which one to use.** 

**There is nothing special about the names, and if you wanted to call Usart0 UsartA01 or Usart0alt UsartA45 to give an indication which pins are involved, you could also do that instead (probably makes more sense as the name is meaningless after the instance is created, and we are moslty interested in the pins used and not the usart numbers).**

```
template<typename> struct Usart;
using Usart0    = Usart< USART_INST<0,PINS::A0,PINS::A1,0> >;
using Usart0alt = Usart< USART_INST<0,PINS::A4,PINS::A5,1> >;
using Usart1    = Usart< USART_INST<1,PINS::C0,PINS::C1,0> >;
using Usart1alt = Usart< USART_INST<1,PINS::C4,PINS::C5,1> >;
using Usart2    = Usart< USART_INST<2,PINS::F0,PINS::F1,0> >;
using Usart2alt = Usart< USART_INST<2,PINS::F4,PINS::F5,1> >;
using Usart3    = Usart< USART_INST<3,PINS::B0,PINS::B1,0> >;
using Usart3alt = Usart< USART_INST<3,PINS::B4,PINS::B5,1> >;
```

**The Usart class comes next. There is not that much different than what has already been covered previously. You can see that the template argument takes in the instance specific information we created above. To access anything in that Inst_, you simply access direclty via Inst_:: which should look normal if you are familiar with C++.**

**You can see the addition of public enums in this case, which are Usart specific enums used for function argumemts. Also, we will create our own complete peripheral register struct in this case, and simply declaring it up top and creating it later means we can park it below all the other code and do not have to stumble over it every time we want to look at the parts of the code that need looking at.**

**The Usart class is stripped down for this example, but it will function and when turned on it will take care of the pins and portmux, and be ready to go for simple usage.**
```
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
```

**The final usage is simple- create a Usart instance of your choice (Usart0 with default pins, in this case), set the baud register value to something, then read and write as needed. In this case the read and write are both blocking, and the read takes in a u8 reference so we can use the return value to inform if there were any rx errors involved.**

```
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
```

**This is obviously an incomplete Usart class, but everything else to make it complete is just more of the same. You can end up with an interrupt driven usart that can optionally use buffers (another class), can handle all the various modes, can hook into the things in stdio.h, and so on.**

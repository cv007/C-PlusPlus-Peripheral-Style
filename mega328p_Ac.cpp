// you are here - https://godbolt.org/z/6h7Pqdf
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
using u8 = uint8_t;
#define SA static auto

/*---------------------------------------------------------------------
    Ac - Analog Comparator - mega328p
---------------------------------------------------------------------*/
struct Ac {

    enum AINNEG  { ADC0, ADC1, ADC2, ADC3, ADC4, ADC5, ADC6, ADC7, AIN1 };
    enum AINPOS  { AIN0, BANDGAP };
    enum IRQMODE { TOGGLE, FALLING = 2, RISING };

//===========
    private:
//===========
    struct Reg; //at end of struct
    //note - since we are including io.h and also creating our own
    //register layout, we will avoid io.h names by using lowercase
    //as first letter ( ADEN = aDEN )
    //could also skip creating our own register layout and use what
    //is already in io.h, but just showing what it takes to 'skip' io.h

//===========
    public:
//===========

    static inline volatile Reg& reg{ *reinterpret_cast<Reg*>(0x50) };

SA  ain0Analog  ()              { reg.aIN0D = 1; }
SA  ain0Digital ()              { reg.aIN0D = 0; }
SA  ain1Analog  ()              { reg.aIN1D = 1; }
SA  ain1Digital ()              { reg.aIN1D = 0; }

SA  negSel      (AINNEG e)      {
                                if( e == AIN1 ){
                                    reg.aCME = 0;
                                    ain1Analog();
                                } else {
                                    reg.aDEN = 0;
                                    reg.mUX = e;
                                }
                                }

SA  posSel      (AINPOS e)      {
                                reg.aCBG = e;
                                if( e == AIN0 ) ain0Analog();
                                }

SA  captureOn   ()              { reg.aCIC = 1; } //to timer
SA  captureOff  ()              { reg.aCIC = 0; }

SA  irqMode     (IRQMODE e)     { reg.aCIS = e; }
SA  irqOn       ()              { reg.aCIE = 1; }
SA  irqOn       (IRQMODE e)     { irqMode(e); irqOn(); }
SA  irqOff      ()              { reg.aCIE = 0; }

SA  isFlag      ()              { return reg.aCI; }
SA  clearFlag   ()              { reg.aCI = 1; } //hardware does when using isr

SA  on          ()              { reg.aCD = 0; } //default
SA  off         ()              { irqOff(); reg.aCD = 1; }

SA  on          (AINNEG n, AINPOS p) {
                    negSel(n);
                    posSel(p);
                    on();
                }

SA  on          (AINNEG n, AINPOS p, IRQMODE m) {
                    on( n, p );
                    irqMode( m );
                    clearFlag();
                    irqOn();
                }                

//===========
    private:
//===========

    //will include all registers needed in one struct, padding as required
    struct Reg {
        union  { u8 aCSR;           //0x50
        struct {    u8 aCIS :2;
                    u8 aCIC :1;
                    u8 aCIE :1;
                    u8 aCI  :1;     //R,W1
                    u8 aCO  :1;     //RO
                    u8 aCBG :1;
                    u8 aCD  :1;
        };};
                 u8 unused1[0x7A-0x50-1];
        union  { u8 aDCSRA;         //0x7A
        struct {    u8 :7;
                    u8 aDEN :1;                
        };};
        union  { u8 aDCSRB;         //0x7B
        struct {    u8 :6;
                    u8 aCME :1;
        };};
        union  { u8 aDCMUX;        //0x7C
        struct {    u8 mUX :3;
        };};
                 u8 unused2[2];
        union  { u8 dIDR1;          //0x7F
        struct {    u8 aIN0D :1;
                    u8 aIN1D :1;
        };};
        
    };

};



[[ using gnu : signal, used ]] //effectively same as ISR macro
void ANALOG_COMP_vect(){
    //do something
}


/*---------------------------------------------------------------------
    main
---------------------------------------------------------------------*/
int main(void) {

    Ac ac;
    ac.on( ac.ADC0, ac.AIN0, ac.FALLING );

    sei();

    while(true){}

}



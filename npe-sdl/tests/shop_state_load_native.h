/* Original save/load resident fields. No LCD/position/audio proof. */
typedef struct {const char *name;unsigned title,kind,mode,event,map,progress,context,npcContext,selected,first,last,row,ui,quantity,choice,descStart,descEnd;} ShopLoadCase;
static const ShopLoadCase shop_load_cases[]={
{"buy-resident",0,1,1,1,9,1,13,13,46,45,54,1,1,99,1,82,97},
{"empty-sell-resident",0,2,1,1,9,1,10,10,65535,9,37,6,2,0,0,0,65535},
{"word-byte-boundaries",0,1,1,1,9,1,65535,255,254,1,254,6,2,255,1,65534,65535},
{"buy-resident",1,1,1,1,9,1,13,13,46,45,54,1,1,99,1,82,97},
{"empty-sell-resident",1,2,1,1,9,1,10,10,65535,9,37,6,2,0,0,0,65535},
{"word-byte-boundaries",1,1,1,1,9,1,65535,255,254,1,254,6,2,255,1,65534,65535},
};

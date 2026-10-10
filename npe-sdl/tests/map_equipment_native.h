/* Independent original key sequence, including replacement effects. */
static const struct { unsigned id; uint16_t hp,qi,attack,defense,status[4]; uint8_t equipment[6]; uint16_t inventory[3]; } map_equipment_native[] = {
    {85,130,60,42,40,{5,2,3,4},{0,0,0,0,0,85},{1,0,1}},
    {91,130,60,80,78,{5,2,3,5},{0,0,0,0,0,91},{1,1,0}},
    {26,130,60,86,133,{5,2,3,5},{0,0,26,0,0,91},{0,1,0}},
};

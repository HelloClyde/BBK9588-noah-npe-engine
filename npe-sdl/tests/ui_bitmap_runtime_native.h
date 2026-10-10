/* Original indexed loader and draw, UI resource ordinals and guest group slots. */
typedef struct {const char *name;uint16_t index,group,offset,base;int16_t x,y;uint8_t kind;
 const char *ram,*trace,*pixels;} UiRuntimeCase;
static const UiRuntimeCase ui_runtime_cases[]={
 {"ui-money",1706,7,1,1705,0,7,0,"tmp/map-bitmap-runtime-native/ui-money-ram.bin","tmp/map-bitmap-runtime-native/ui-money-trace.bin","tmp/map-bitmap-runtime-native/ui-money-pixels.bin"},
 {"ui-parent",1707,7,2,1705,0,37,0,"tmp/map-bitmap-runtime-native/ui-parent-ram.bin","tmp/map-bitmap-runtime-native/ui-parent-trace.bin","tmp/map-bitmap-runtime-native/ui-parent-pixels.bin"},
 {"ui-property",1708,7,3,1705,42,37,0,"tmp/map-bitmap-runtime-native/ui-property-ram.bin","tmp/map-bitmap-runtime-native/ui-property-trace.bin","tmp/map-bitmap-runtime-native/ui-property-pixels.bin"},
 {"ui-item",1709,7,4,1705,42,70,0,"tmp/map-bitmap-runtime-native/ui-item-ram.bin","tmp/map-bitmap-runtime-native/ui-item-trace.bin","tmp/map-bitmap-runtime-native/ui-item-pixels.bin"},
 {"ui-system",1710,7,5,1705,42,37,0,"tmp/map-bitmap-runtime-native/ui-system-ram.bin","tmp/map-bitmap-runtime-native/ui-system-trace.bin","tmp/map-bitmap-runtime-native/ui-system-pixels.bin"},
 {"ui-music",1711,7,6,1705,105,70,0,"tmp/map-bitmap-runtime-native/ui-music-ram.bin","tmp/map-bitmap-runtime-native/ui-music-trace.bin","tmp/map-bitmap-runtime-native/ui-music-pixels.bin"},
 {"ui-quantity",1714,7,9,1705,30,5,0,"tmp/map-bitmap-runtime-native/ui-quantity-ram.bin","tmp/map-bitmap-runtime-native/ui-quantity-trace.bin","tmp/map-bitmap-runtime-native/ui-quantity-pixels.bin"},
 {"ui-equipment",1715,7,10,1705,0,0,0,"tmp/map-bitmap-runtime-native/ui-equipment-ram.bin","tmp/map-bitmap-runtime-native/ui-equipment-trace.bin","tmp/map-bitmap-runtime-native/ui-equipment-pixels.bin"},
 {"ui-selector-first",1716,7,11,1705,7,78,2,"tmp/map-bitmap-runtime-native/ui-selector-first-ram.bin","tmp/map-bitmap-runtime-native/ui-selector-first-trace.bin","tmp/map-bitmap-runtime-native/ui-selector-first-pixels.bin"},
 {"ui-selector-last",1716,7,11,1705,197,78,2,"tmp/map-bitmap-runtime-native/ui-selector-last-ram.bin","tmp/map-bitmap-runtime-native/ui-selector-last-trace.bin","tmp/map-bitmap-runtime-native/ui-selector-last-pixels.bin"},
 {"ui-save",1717,7,12,1705,105,46,0,"tmp/map-bitmap-runtime-native/ui-save-ram.bin","tmp/map-bitmap-runtime-native/ui-save-trace.bin","tmp/map-bitmap-runtime-native/ui-save-pixels.bin"},
 {"ui-title",1720,7,15,1705,0,0,0,"tmp/map-bitmap-runtime-native/ui-title-ram.bin","tmp/map-bitmap-runtime-native/ui-title-trace.bin","tmp/map-bitmap-runtime-native/ui-title-pixels.bin"},
 {"ui-member",1723,7,18,1705,0,0,0,"tmp/map-bitmap-runtime-native/ui-member-ram.bin","tmp/map-bitmap-runtime-native/ui-member-trace.bin","tmp/map-bitmap-runtime-native/ui-member-pixels.bin"},
 {"ui-story",1724,7,19,1705,0,0,0,"tmp/map-bitmap-runtime-native/ui-story-ram.bin","tmp/map-bitmap-runtime-native/ui-story-trace.bin","tmp/map-bitmap-runtime-native/ui-story-pixels.bin"},
 {"ui-portrait-1",1703,6,36,1667,14,9,0,"tmp/map-bitmap-runtime-native/ui-portrait-1-ram.bin","tmp/map-bitmap-runtime-native/ui-portrait-1-trace.bin","tmp/map-bitmap-runtime-native/ui-portrait-1-pixels.bin"},
 {"ui-portrait-2",1704,6,37,1667,14,9,0,"tmp/map-bitmap-runtime-native/ui-portrait-2-ram.bin","tmp/map-bitmap-runtime-native/ui-portrait-2-trace.bin","tmp/map-bitmap-runtime-native/ui-portrait-2-pixels.bin"},
 {"ui-portrait-3",1705,6,38,1667,14,9,0,"tmp/map-bitmap-runtime-native/ui-portrait-3-ram.bin","tmp/map-bitmap-runtime-native/ui-portrait-3-trace.bin","tmp/map-bitmap-runtime-native/ui-portrait-3-pixels.bin"},
 {"ui-drop",1698,6,31,1667,40,30,0,"tmp/map-bitmap-runtime-native/ui-drop-ram.bin","tmp/map-bitmap-runtime-native/ui-drop-trace.bin","tmp/map-bitmap-runtime-native/ui-drop-pixels.bin"},
 {"ui-learn",1699,6,32,1667,36,40,0,"tmp/map-bitmap-runtime-native/ui-learn-ram.bin","tmp/map-bitmap-runtime-native/ui-learn-trace.bin","tmp/map-bitmap-runtime-native/ui-learn-pixels.bin"},
 {"ui-level",1700,6,33,1667,10,14,0,"tmp/map-bitmap-runtime-native/ui-level-ram.bin","tmp/map-bitmap-runtime-native/ui-level-trace.bin","tmp/map-bitmap-runtime-native/ui-level-pixels.bin"},
 {"ui-base-shift",1712,7,6,1706,105,70,0,"tmp/map-bitmap-runtime-native/ui-base-shift-ram.bin","tmp/map-bitmap-runtime-native/ui-base-shift-trace.bin","tmp/map-bitmap-runtime-native/ui-base-shift-pixels.bin"},
 {"ui-base-wrap",5,7,11,65530,7,78,2,"tmp/map-bitmap-runtime-native/ui-base-wrap-ram.bin","tmp/map-bitmap-runtime-native/ui-base-wrap-trace.bin","tmp/map-bitmap-runtime-native/ui-base-wrap-pixels.bin"},
};

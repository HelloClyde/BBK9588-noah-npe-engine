/* Unmodified role loader, softfloat, rounding and draw. Clear=0. */
typedef struct {const char *ram,*trace,*pixels;int16_t x,y;} RoleRuntimeFrame;
typedef struct {const char *name;int16_t group,x,y,end_x,end_y;uint8_t kind;
 uint32_t directory,payload;const char *ram,*trace,*pixels;
 unsigned frames;const RoleRuntimeFrame *references;} RoleRuntimeCase;
static const RoleRuntimeFrame role_runtime_frames_0[]={
 {"tmp/map-bitmap-runtime-native/role-cast-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-cast-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-cast-frame-000-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-cast-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-cast-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-cast-frame-001-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-cast-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-cast-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-cast-frame-002-pixels.bin",60,40},
};
static const RoleRuntimeFrame role_runtime_frames_1[]={
 {"tmp/map-bitmap-runtime-native/role-flee-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-000-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-flee-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-001-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-flee-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-002-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-flee-frame-003-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-003-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-003-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-flee-frame-004-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-004-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-004-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-flee-frame-005-ram.bin","tmp/map-bitmap-runtime-native/role-flee-frame-005-trace.bin","tmp/map-bitmap-runtime-native/role-flee-frame-005-pixels.bin",60,40},
};
static const RoleRuntimeFrame role_runtime_frames_2[]={
 {"tmp/map-bitmap-runtime-native/role-combine-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-000-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-001-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-002-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-003-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-003-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-003-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-004-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-004-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-004-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-005-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-005-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-005-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-combine-frame-006-ram.bin","tmp/map-bitmap-runtime-native/role-combine-frame-006-trace.bin","tmp/map-bitmap-runtime-native/role-combine-frame-006-pixels.bin",60,40},
};
static const RoleRuntimeFrame role_runtime_frames_3[]={
 {"tmp/map-bitmap-runtime-native/role-background-odd-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-000-pixels.bin",61,40},
 {"tmp/map-bitmap-runtime-native/role-background-odd-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-001-pixels.bin",61,40},
 {"tmp/map-bitmap-runtime-native/role-background-odd-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-background-odd-frame-002-pixels.bin",61,40},
};
static const RoleRuntimeFrame role_runtime_frames_4[]={
 {"tmp/map-bitmap-runtime-native/role-transparent-odd-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-000-pixels.bin",61,40},
 {"tmp/map-bitmap-runtime-native/role-transparent-odd-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-001-pixels.bin",61,40},
 {"tmp/map-bitmap-runtime-native/role-transparent-odd-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-frame-002-pixels.bin",61,40},
};
static const RoleRuntimeFrame role_runtime_frames_5[]={
 {"tmp/map-bitmap-runtime-native/role-forward-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-forward-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-forward-frame-000-pixels.bin",58,38},
 {"tmp/map-bitmap-runtime-native/role-forward-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-forward-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-forward-frame-001-pixels.bin",55,37},
 {"tmp/map-bitmap-runtime-native/role-forward-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-forward-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-forward-frame-002-pixels.bin",53,35},
};
static const RoleRuntimeFrame role_runtime_frames_6[]={
 {"tmp/map-bitmap-runtime-native/role-backward-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-backward-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-backward-frame-000-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-backward-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-backward-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-backward-frame-001-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-backward-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-backward-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-backward-frame-002-pixels.bin",60,40},
};
static const RoleRuntimeFrame role_runtime_frames_7[]={
 {"tmp/map-bitmap-runtime-native/role-ties-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-ties-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-ties-frame-000-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-ties-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-ties-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-ties-frame-001-pixels.bin",59,40},
};
static const RoleRuntimeFrame role_runtime_frames_8[]={
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-000-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-001-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-002-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-003-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-003-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-003-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-004-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-004-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-004-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-005-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-005-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-005-pixels.bin",59,40},
 {"tmp/map-bitmap-runtime-native/role-small-fractions-frame-006-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-006-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-frame-006-pixels.bin",59,40},
};
static const RoleRuntimeFrame role_runtime_frames_10[]={
 {"tmp/map-bitmap-runtime-native/role-single-moving-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-000-pixels.bin",64,56},
 {"tmp/map-bitmap-runtime-native/role-single-moving-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-001-pixels.bin",48,48},
 {"tmp/map-bitmap-runtime-native/role-single-moving-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-002-pixels.bin",32,40},
 {"tmp/map-bitmap-runtime-native/role-single-moving-frame-003-ram.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-003-trace.bin","tmp/map-bitmap-runtime-native/role-single-moving-frame-003-pixels.bin",16,32},
};
static const RoleRuntimeFrame role_runtime_frames_11[]={
 {"tmp/map-bitmap-runtime-native/role-single-mixed-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-000-pixels.bin",64,32},
 {"tmp/map-bitmap-runtime-native/role-single-mixed-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-001-pixels.bin",48,32},
 {"tmp/map-bitmap-runtime-native/role-single-mixed-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-002-pixels.bin",32,32},
 {"tmp/map-bitmap-runtime-native/role-single-mixed-frame-003-ram.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-003-trace.bin","tmp/map-bitmap-runtime-native/role-single-mixed-frame-003-pixels.bin",16,32},
};
static const RoleRuntimeFrame role_runtime_frames_19[]={
 {"tmp/map-bitmap-runtime-native/role-payload-eof-frame-000-ram.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-000-trace.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-000-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-payload-eof-frame-001-ram.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-001-trace.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-001-pixels.bin",60,40},
 {"tmp/map-bitmap-runtime-native/role-payload-eof-frame-002-ram.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-002-trace.bin","tmp/map-bitmap-runtime-native/role-payload-eof-frame-002-pixels.bin",60,40},
};
static const RoleRuntimeCase role_runtime_cases[]={
 {"role-cast",2,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-cast-ram.bin","tmp/map-bitmap-runtime-native/role-cast-trace.bin","tmp/map-bitmap-runtime-native/role-cast-pixels.bin",3,role_runtime_frames_0},
 {"role-flee",1,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-flee-ram.bin","tmp/map-bitmap-runtime-native/role-flee-trace.bin","tmp/map-bitmap-runtime-native/role-flee-pixels.bin",6,role_runtime_frames_1},
 {"role-combine",15,60,40,60,40,0,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-combine-ram.bin","tmp/map-bitmap-runtime-native/role-combine-trace.bin","tmp/map-bitmap-runtime-native/role-combine-pixels.bin",7,role_runtime_frames_2},
 {"role-background-odd",2,61,40,61,40,1,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-background-odd-ram.bin","tmp/map-bitmap-runtime-native/role-background-odd-trace.bin","tmp/map-bitmap-runtime-native/role-background-odd-pixels.bin",3,role_runtime_frames_3},
 {"role-transparent-odd",2,61,40,61,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-transparent-odd-ram.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-trace.bin","tmp/map-bitmap-runtime-native/role-transparent-odd-pixels.bin",3,role_runtime_frames_4},
 {"role-forward",2,60,40,53,35,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-forward-ram.bin","tmp/map-bitmap-runtime-native/role-forward-trace.bin","tmp/map-bitmap-runtime-native/role-forward-pixels.bin",3,role_runtime_frames_5},
 {"role-backward",2,60,40,67,45,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-backward-ram.bin","tmp/map-bitmap-runtime-native/role-backward-trace.bin","tmp/map-bitmap-runtime-native/role-backward-pixels.bin",3,role_runtime_frames_6},
 {"role-ties",16,60,40,59,41,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-ties-ram.bin","tmp/map-bitmap-runtime-native/role-ties-trace.bin","tmp/map-bitmap-runtime-native/role-ties-pixels.bin",2,role_runtime_frames_7},
 {"role-small-fractions",15,60,40,59,41,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-small-fractions-ram.bin","tmp/map-bitmap-runtime-native/role-small-fractions-trace.bin","tmp/map-bitmap-runtime-native/role-small-fractions-pixels.bin",7,role_runtime_frames_8},
 {"role-single-stationary",57,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-single-stationary-ram.bin","tmp/map-bitmap-runtime-native/role-single-stationary-trace.bin","tmp/map-bitmap-runtime-native/role-single-stationary-pixels.bin",0,NULL},
 {"role-single-moving",57,80,64,16,32,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-single-moving-ram.bin","tmp/map-bitmap-runtime-native/role-single-moving-trace.bin","tmp/map-bitmap-runtime-native/role-single-moving-pixels.bin",4,role_runtime_frames_10},
 {"role-single-mixed",57,80,32,16,65,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-single-mixed-ram.bin","tmp/map-bitmap-runtime-native/role-single-mixed-trace.bin","tmp/map-bitmap-runtime-native/role-single-mixed-pixels.bin",4,role_runtime_frames_11},
 {"role-single-reversed",57,16,32,80,64,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-single-reversed-ram.bin","tmp/map-bitmap-runtime-native/role-single-reversed-trace.bin","tmp/map-bitmap-runtime-native/role-single-reversed-pixels.bin",0,NULL},
 {"role-single-small",57,32,32,25,25,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-single-small-ram.bin","tmp/map-bitmap-runtime-native/role-single-small-trace.bin","tmp/map-bitmap-runtime-native/role-single-small-pixels.bin",0,NULL},
 {"role-zero",0,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-zero-ram.bin","tmp/map-bitmap-runtime-native/role-zero-trace.bin","tmp/map-bitmap-runtime-native/role-zero-pixels.bin",0,NULL},
 {"role-negative",-1,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-negative-ram.bin","tmp/map-bitmap-runtime-native/role-negative-trace.bin","tmp/map-bitmap-runtime-native/role-negative-pixels.bin",0,NULL},
 {"role-empty",51,60,40,60,40,2,UINT32_C(690348),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-empty-ram.bin","tmp/map-bitmap-runtime-native/role-empty-trace.bin","tmp/map-bitmap-runtime-native/role-empty-pixels.bin",0,NULL},
 {"role-directory-eof",2,60,40,60,40,2,UINT32_C(3751913),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-directory-eof-ram.bin","tmp/map-bitmap-runtime-native/role-directory-eof-trace.bin","tmp/map-bitmap-runtime-native/role-directory-eof-pixels.bin",0,NULL},
 {"role-directory-short",2,60,40,60,40,2,UINT32_C(3751911),UINT32_C(693456),"tmp/map-bitmap-runtime-native/role-directory-short-ram.bin","tmp/map-bitmap-runtime-native/role-directory-short-trace.bin","tmp/map-bitmap-runtime-native/role-directory-short-pixels.bin",0,NULL},
 {"role-payload-eof",2,60,40,60,40,2,UINT32_C(690348),UINT32_C(3751913),"tmp/map-bitmap-runtime-native/role-payload-eof-ram.bin","tmp/map-bitmap-runtime-native/role-payload-eof-trace.bin","tmp/map-bitmap-runtime-native/role-payload-eof-pixels.bin",3,role_runtime_frames_19},
};

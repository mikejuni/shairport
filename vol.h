#ifndef VOL_H
#define VOL_H
//void set_volume_param(char* params, ...);
void init_volume_ctl();
void set_volume(double d);
void deinit_volume_ctl();
void print_vol_args();
void parse_vol_arg(char*);
#endif

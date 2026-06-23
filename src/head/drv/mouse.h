#ifndef MOUSE_H
#define MOUSE_H

#include "harlin_API.h"

#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

void mouse_init(void);
int mouse_has_data(void);
int mouse_read_packet(int* dx, int* dy, u8* buttons);
int mouse_get_x(void);
int mouse_get_y(void);
u8 mouse_get_buttons(void);
void mouse_set_position(int x, int y);

#define Harlin_MouseInit              mouse_init
#define Harlin_MouseHasData           mouse_has_data
#define Harlin_MouseReadPacket        mouse_read_packet
#define Harlin_MouseGetX              mouse_get_x
#define Harlin_MouseGetY              mouse_get_y
#define Harlin_MouseGetButtons        mouse_get_buttons
#define Harlin_MouseSetPosition       mouse_set_position

#endif

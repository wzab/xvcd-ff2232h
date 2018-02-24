// Basic XVC core for an FTDI device in MPSSE mode, using
// the libftdi library under Linux. I've tested this under Linux:
// I guess it could work under Windows as well, but since XVC
// is network based, I never saw the point.

// Thanks to tmbinc for the original xvcd implementation:
// this code however is (I'm pretty sure) a total rewrite of the
// physical layer code.

// Author: P.S. Allison (allison.122@osu.edu)
// This code is in the public domain (CC0):
// see https://wiki.creativecommons.org/CC0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ftdi.h>
#include <usb.h>

unsigned int ftdi_verbosity;
#define DEBUGCOND(lvl) (lvl<=ftdi_verbosity)
#define DEBUG(lvl,...) if (lvl<=ftdi_verbosity) printf(__VA_ARGS__)
#define DEBUGPRINTF(...) printf(__VA_ARGS__)

struct ftdi_context ftdi;

/** \brief Read bytes from the FTDI device, possibly in multiple chunks. */
void ftdi_xvc_read_bytes(unsigned int len, unsigned char *buf) {
  int read, to_read, last_read;
  to_read = len;
  read = 0;
  last_read = ftdi_read_data(&ftdi, buf, to_read);
  if (last_read > 0) read += last_read;
  while (read < to_read) {
    last_read = ftdi_read_data(&ftdi, buf+read, to_read-read);
    if (last_read > 0) read += last_read;
  }
}


/** \brief Close the FTDI device. */
void ftdi_xvc_close_device() {
  ftdi_usb_reset(&ftdi);
  ftdi_usb_close(&ftdi);
  ftdi_deinit(&ftdi);
}

/** \brief Initialize the FTDI library. */
void ftdi_xvc_init(unsigned int verbosity) 
{
   ftdi_init(&ftdi);
   ftdi_verbosity = verbosity;
}

/** \brief Open the FTDI device. */
int ftdi_xvc_open_device(int vendor, int product) 
{
   if (ftdi_usb_open_desc(&ftdi, vendor, product, NULL, NULL) < 0) 
     {
	fprintf(stderr, "xvcd: %s : can't open device.\n", __FUNCTION__);
	return -1;
     }
   ftdi_usb_reset(&ftdi);
   ftdi_set_interface(&ftdi, INTERFACE_A);
   ftdi_set_latency_timer(&ftdi, 1);
   return 0;
}

/** \brief Fetch the FTDI context, in case someone else wants to muck with the device before we do. */
struct ftdi_context *ftdi_xvc_get_context() 
{
   return &ftdi;
}

/** \brief Initialize the MPSSE engine on the FTDI device. */
int ftdi_xvc_init_mpsse() {
   int res;
   unsigned char byte;
   
   unsigned char buf[7] = {
    SET_BITS_LOW, 0x08, 0x0B,  // Set TMS high, TCK/TDI/TMS as outputs.
    TCK_DIVISOR, 0x01, 0x00,   // Set TCK clock rate = 6 MHz.
    SEND_IMMEDIATE
  };   
  ftdi_set_bitmode(&ftdi, 0x0B, BITMODE_BITBANG);
  ftdi_set_bitmode(&ftdi, 0x0B, BITMODE_MPSSE);
  while (res = ftdi_read_data(&ftdi, &byte, 1));
  if (ftdi_write_data(&ftdi, buf, 7) != 7) 
     {
	fprintf(stderr, "xvcd: %s : FTDI initialization failed.\n", __FUNCTION__);
	return -1;
     }
   return 0;
}

#define MAX_DATA 2048
/* WZab - simplified command for shifting */
/** \brief Handle a 'shift:' command sent via XVC. */
int ftdi_xvc_shift_command(unsigned int len,
			   unsigned char *buffer,
			   unsigned char *result) 
{
   int i;
   int to_send;
   int nr_bytes;
   int pos;
   int left;
   nr_bytes = (len+7)/8;
   left = len;
   pos = 0;
   unsigned char ftdi_cmd[3*MAX_DATA];
   unsigned char ftdi_res[MAX_DATA];
   //Prepare the result buffer
   memset(result,0,nr_bytes);
   while(left) {
     int to_send = (MAX_DATA < left) ? MAX_DATA : left;
     for(i=0;i<to_send;i++) {
        ftdi_cmd[3*i] = MPSSE_WRITE_TMS|MPSSE_DO_READ|MPSSE_LSB|MPSSE_BITMODE|MPSSE_WRITE_NEG;
        ftdi_cmd[3*i+1] = 0; //One bit - 0!
        ftdi_cmd[3*i+2] = 
	   ((buffer[(pos+i)/8] & (1<<((pos+i) & 7))) ? 0x03 : 0x00) |
           ((buffer[(pos+i)/8+nr_bytes] & (1<<((pos+i) & 7))) ? 0x80 : 0x00);
        }
     if (ftdi_write_data(&ftdi, ftdi_cmd, 3*to_send) != 3*to_send) {
        return 1;
        }
     //Read the response
     ftdi_xvc_read_bytes(to_send, ftdi_res);
     //Unpack the response
     for(i=0;i<to_send;i++) {
        result[(pos+i)/8] |= (ftdi_res[i] & 0x80) ? (1<<((pos+i)&7)) : 0;
        //fprintf(stderr,"%x\n",(int)result[(pos+i)/8]);
     }
     left -= to_send;
     pos += to_send;
   }     
   return 0;
}
